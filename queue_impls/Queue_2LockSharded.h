#pragma once
#include "BaseQueue.h"
#include <map>
#include <optional>


namespace Impl::Queue_2LockSharded
{

template<typename BaseQueue>
class Shard
{
private:
	using KVPair = typename BaseQueue::KVPair;
	using Key = typename BaseQueue::key_type;
	using Value = typename BaseQueue::value_type;
	
	
	Utils::Queue<Key> m_queue;
	std::map<Key, Value> m_map;
	std::mutex m_queueLock, m_mapLock;
	
	[[nodiscard]] std::optional<Key> _locked_queue_pop() {
		DECL_LOCK_GUARD(m_queueLock);
		if (m_queue.empty()) { return std::nullopt; }
		
		Key key = std::move(m_queue.front());
		m_queue.pop();
		return key;
	}
	
	[[nodiscard]] Value _locked_map_pop(const Key &key) {
		DECL_LOCK_GUARD(m_mapLock);
		
		auto iter = m_map.find(key);
		assert(iter != m_map.end());
		Value val = std::move(iter->second);
		m_map.erase(iter);
		return val;
	}
public:
	Shard() = default;
	
	bool write(const Key &key, const Value &value, bool dedupOnly) {
		std::unique_lock<std::mutex> uniqueLock{ m_mapLock };
		auto iter = m_map.find(key);
		if (iter != m_map.end()) { // dedup
			iter->second = value;
			return true;
		}
		else if (!dedupOnly) {
			m_map[key] = value;
			uniqueLock.unlock();
			DECL_LOCK_GUARD(m_queueLock);
			m_queue.push(key);
		}
		return false;
	}
	
	[[nodiscard]] std::optional<KVPair> try_read() {
		std::optional<Key> key = _locked_queue_pop();
		if (key.has_value()) {
			Value val = _locked_map_pop(*key);
			return KVPair{ std::move(*key), std::move(val) };
		}
		return std::nullopt;
	}
};

template<typename Key, typename Value, size_t N_SHARDS>
class ShardArray : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	using KVPair = typename BaseQ::KVPair;
	
	
	std::array<Shard<BaseQ>, N_SHARDS> m_shards;
	std::atomic<usize> m_readIndex, m_size;
	
	[[nodiscard]] constexpr static
	usize _index_from_key(const Key &key) { return std::hash<Key>{}(key); }
public:
	ShardArray(const usize capacity)
		: BaseQ{ capacity }
		, m_readIndex{ 0 }
		, m_size{ 0 }
	{}
	
	[[nodiscard]] constexpr usize size() {
		return m_size.load();
	}
	
	bool try_write(const Key &key, const Value &value) {
		const bool overflow = (m_size.fetch_add(1) >= this->capacity());
		
		auto &shard = m_shards[_index_from_key(key) % m_shards.size()];
		const bool deduped = shard.write(key, value, overflow);
		
		if (overflow || deduped) {
			m_size.fetch_sub(1);
		}
		return !overflow || deduped;
	}
	
	constexpr KVPair read() {
		while (true) {
			for (auto &shard : m_shards) {
				if (std::optional data = shard.try_read()) {
					m_size.fetch_sub(1);
					return *data;
				}
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(Utils::WAIT_TIME);
		}
	}
};

}

/* An array of queues that never compete and each have 2 locks.
 * Round-robin is used to find the correct queue when reading.
 *
 * Similar to the single-lock implementation, but uses the fact that
 * the queue and map can be locked separately when ordered correctly:
 * write(map) -> write(queue) -> read(queue) -> read(map)
 * This shows that an item can only be removed from the map if it was added to the queue.
 */
template<typename Key, typename Value, size_t N_SHARDS>
using Queue_2LockSharded = Impl::Queue_2LockSharded::ShardArray<Key, Value, N_SHARDS>;

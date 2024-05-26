#pragma once
#include "BaseQueue.h"
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
	
	
	Utils::Queue<typename std::map<Key, Value>::iterator> m_queue;
	std::map<Key, Value> m_map;
	std::mutex m_queueLock, m_mapLock;
	
	[[nodiscard]] std::optional<typename std::map<Key, Value>::iterator> _locked_queue_pop() {
		DECL_LOCK_GUARD(m_queueLock);
		if (m_queue.empty()) { return std::nullopt; }
		return m_queue.pop();
	}
public:
	Shard() = default;
	
	bool write(const Key &key, const Value &value, bool dedupOnly) {
		std::unique_lock<std::mutex> uniqueLock{ m_mapLock };
		if (dedupOnly) {
			auto iter = m_map.find(key);
			if (iter == m_map.end()) { return false; }
			iter->second = value;
		}
		else if (auto [iter, inserted] = m_map.insert_or_assign(key, value); inserted) {
			uniqueLock.unlock();
			DECL_LOCK_GUARD(m_queueLock);
			m_queue.push(iter);
			return false;
		}
		return true;
	}
	
	[[nodiscard]] std::optional<KVPair> try_read() {
		std::optional iter = _locked_queue_pop();
		if (iter.has_value()) {
			DECL_LOCK_GUARD(m_mapLock);
			return Utils::map_pop_iter(m_map, *iter);
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
	std::atomic<usize> m_size;
	
	[[nodiscard]] constexpr static
	usize _index_from_key(const Key &key) { return std::hash<Key>{}(key); }
public:
	ShardArray(const usize capacity)
		: BaseQ{ capacity }
		, m_size{ 0 }
	{}
	
	[[nodiscard]] constexpr usize size() {
		return m_size.load();
	}
	
	bool try_write(const Key &key, const Value &value) {
		const bool overflow = (m_size.fetch_add(1) >= this->capacity());
		
		auto &shard = m_shards[_index_from_key(key) % N_SHARDS];
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

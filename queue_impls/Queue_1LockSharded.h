#pragma once
#include "BaseQueue.h"
#include <map>
#include <optional>


namespace Impl::Queue_1LockSharded
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
	std::mutex m_lock;
public:
	Shard() = default;
	
	bool write(const Key &key, const Value &value, bool dedupOnly) {
		DECL_LOCK_GUARD(m_lock);
		auto iter = m_map.find(key);
		if (iter != m_map.end()) { // dedup
			iter->second = value;
			return true;
		}
		else if (!dedupOnly) {
			m_map[key] = value;
			m_queue.push(key);
		}
		return false;
	}
	
	[[nodiscard]] std::optional<KVPair> try_read() {
		DECL_LOCK_GUARD(m_lock);
		if (m_queue.empty()) { return std::nullopt; }
		
		Key key = std::move(m_queue.front());
		m_queue.pop();
		
		auto iter = m_map.find(key);
		assert(iter != m_map.end());
		Value val = std::move(iter->second);
		m_map.erase(iter);
		
		return KVPair{ std::move(key), std::move(val) };
	}
};

/* An array of locks that never compete.
 * This is the simplest and acts as a reference implementation.
 */
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

template<typename Key, typename Value, size_t N_SHARDS>
using Queue_1LockSharded = Impl::Queue_1LockSharded::ShardArray<Key, Value, N_SHARDS>;
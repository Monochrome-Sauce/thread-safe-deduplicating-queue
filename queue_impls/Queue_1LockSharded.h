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
		if (dedupOnly) {
			DECL_LOCK_GUARD(m_lock);
			auto iter = m_map.find(key);
			if (iter == m_map.end()) { return false; }
			iter->second = value;
		}
		else if (DECL_LOCK_GUARD(m_lock); m_map.insert_or_assign(key, value).second) {
			m_queue.push(key);
			return false;
		}
		return true;
	}
	
	[[nodiscard]] std::optional<KVPair> try_read() {
		DECL_LOCK_GUARD(m_lock);
		if (m_queue.empty()) { return std::nullopt; }
		Key key = m_queue.pop();
		
		auto iter = m_map.find(key);
		assert(iter != m_map.end());
		Value val = std::move(iter->second);
		m_map.erase(iter);
		
		return KVPair{ std::move(key), std::move(val) };
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

/* An array of queues that never compete and each have 1 lock.
 * Round-robin is used to find the correct queue when reading.
 */
template<typename Key, typename Value, size_t N_SHARDS>
using Queue_1LockSharded = Impl::Queue_1LockSharded::ShardArray<Key, Value, N_SHARDS>;

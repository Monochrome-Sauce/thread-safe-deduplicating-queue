#pragma once
#include "BaseQueue.h"
#include <optional>


namespace Impl::Queue_1LockShardedUnlimited
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
	std::mutex m_lock;
public:
	Shard() = default;
	
	bool write(Key &&key, Value &&value) {
		DECL_LOCK_GUARD(m_lock);
		auto [iter, inserted] = m_map.insert_or_assign(std::move(key), std::move(value));
		if (inserted) { m_queue.push(iter); }
		return inserted;
	}
	
	[[nodiscard]] std::optional<KVPair> try_read() {
		DECL_LOCK_GUARD(m_lock);
		if (m_queue.empty()) { return std::nullopt; }
		return Utils::map_pop_iter(m_map, m_queue.pop());
	}
};

template<typename Key, typename Value, size_t N_SHARDS>
class ShardArray : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	using typename BaseQ::KVPair;
	
	std::array<Shard<BaseQ>, N_SHARDS> m_shards;
	std::atomic<usize> m_readIndex, m_writeIndex;
	std::atomic<usize> m_size;
public:
	ShardArray(const usize capacity)
		: BaseQ{ capacity }
		, m_readIndex{ 0 }
		, m_writeIndex{ 0 }
		, m_size{ 0 }
	{}
	
	[[nodiscard]] constexpr usize size() {
		return m_size.load();
	}
	
	bool try_write(Key &&key, Value &&value) {
		auto &shard = m_shards[m_writeIndex.fetch_add(1) % N_SHARDS];
		const bool inserted = shard.write(std::move(key), std::move(value));
		if (inserted) {
			m_size.fetch_add(inserted);
		}
		return true;
	}
	
	constexpr KVPair read() {
		auto &shard = m_shards[m_readIndex.fetch_add(1) % N_SHARDS];
		while (true) {
			for (usize i = 0; i < N_SHARDS; ++i) { // several attempts
				if (std::optional data = shard.try_read()) {
					m_size.fetch_sub(1);
					return *data;
				}
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
		}
	}
};

}

template<typename Key, typename Value, size_t N_SHARDS>
using Queue_1LockShardedUnlimited = Impl::Queue_1LockShardedUnlimited::ShardArray<Key, Value, N_SHARDS>;

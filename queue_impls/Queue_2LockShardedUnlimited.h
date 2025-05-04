#pragma once
#include "BaseQueue.h"
#include <optional>


namespace Impl::Queue_2LockShardedUnlimited
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
	
	bool write(Key &&key, Value &&value) {
		std::unique_lock<std::mutex> uniqueLock{ m_mapLock };
		auto [iter, inserted] = m_map.insert_or_assign(std::move(key), std::move(value));
		uniqueLock.unlock();
		if (inserted) {
			DECL_LOCK_GUARD(m_queueLock);
			m_queue.push(iter);
		}
		return inserted;
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
			m_size.fetch_add(1);
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
			Utils::sleep(WAIT_TIME);
		}
	}
};

}

template<typename Key, typename Value, size_t N_SHARDS>
using Queue_2LockShardedUnlimited = Impl::Queue_2LockShardedUnlimited::ShardArray<Key, Value, N_SHARDS>;

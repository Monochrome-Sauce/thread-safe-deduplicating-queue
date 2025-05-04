#pragma once
#include "BaseQueue.h"
#include <optional>


namespace Impl::Queue_SplitSharded
{

template<typename T>
struct PairedMutex
{
	std::mutex _lock;
	T _data;
	
	template<typename ...Args>
	PairedMutex(Args &&...args)
		: _data{ std::forward<Args>(args)... }
	{}
};


template<typename Key, typename Value, size_t N_SHARDS>
class ShardArray : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	using KVPair = typename BaseQ::KVPair;
	using Map = std::map<Key, Value>;
	
	struct MapItemRef {
		typename Map::iterator _iter;
		usize _index;
	};
	
	std::array<PairedMutex<Utils::Queue<MapItemRef>>, 4> m_queues;
	std::array<PairedMutex<Map>, N_SHARDS> m_maps;
	std::atomic<usize> m_size;
	
	[[nodiscard]] constexpr static
	usize _index_from_key(const Key &key) { return std::hash<Key>{}(key); }
	
	[[nodiscard]] static
	std::optional<MapItemRef> _locked_queue_pop(PairedMutex<Utils::Queue<MapItemRef>> &queue) {
		DECL_LOCK_GUARD(queue._lock);
		if (queue._data.empty()) { return std::nullopt; }
		return queue._data.pop();
	}
public:
	ShardArray(const usize capacity)
		: BaseQ{ capacity }
		, m_size{ 0 }
	{}
	
	[[nodiscard]] constexpr usize size() {
		return m_size.load();
	}
	
	bool try_write(const Key &key, const Value &value) {
		const usize index = _index_from_key(key) % N_SHARDS;
		PairedMutex<Map> &shard = m_maps[index];
		
		if (m_size.fetch_add(1) >= this->capacity()) {
			m_size.fetch_sub(1);
			
			DECL_LOCK_GUARD(shard._lock);
			auto iter = shard._data.find(key);
			if (iter == shard._data.end()) {
				return false;
			}
			iter->second = value;
			return true;
		}
		
		std::unique_lock<std::mutex> uniqueLock{ shard._lock };
		if (auto [iter, inserted] = shard._data.insert_or_assign(key, value); inserted) {
			uniqueLock.unlock();
			auto &queue = m_queues[index % m_queues.size()];
			DECL_LOCK_GUARD(queue._lock);
			queue._data.push({ iter, index });
		}
		else {
			m_size.fetch_sub(1);
		}
		return true;
	}
	
	KVPair read() {
		while (true) {
			for (auto &queue : m_queues) {
				std::optional<MapItemRef> opt = _locked_queue_pop(queue);
				if (!opt.has_value()) { continue; }
				
				m_size.fetch_sub(1);
				PairedMutex<Map> &shard = m_maps[opt->_index];
				DECL_LOCK_GUARD(shard._lock);
				return Utils::map_pop_iter(shard._data, opt->_iter);
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(WAIT_TIME);
		}
	}
};

}

/* An array of deduplication maps that never compete and each have 1 lock.
 * There is a single queue which holds the key and an index to the right map.
 *
 * Similar to the double-lock implementation, which lets the queue and map be locked separately.
 */
template<typename Key, typename Value, size_t N_SHARDS>
using Queue_SplitSharded = Impl::Queue_SplitSharded::ShardArray<Key, Value, N_SHARDS>;

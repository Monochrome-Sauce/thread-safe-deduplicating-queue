#pragma once
#include "BaseQueue.h"
#include <optional>


/* 2 global locks.
 * Similar to the single-lock implementation, but uses the fact that
 * the queue and map can be locked separately when ordered correctly:
 * write(map) -> write(queue) -> read(queue) -> read(map)
 * This shows that an item can only be removed from the map if it was added to the queue.
 */
template<typename Key, typename Value>
class Queue_2Lock : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	
	Utils::Queue<typename std::map<Key, Value>::iterator> m_queue;
	std::map<Key, Value> m_map;
	std::mutex m_queueLock, m_mapLock;
	
	[[nodiscard]] std::optional<typename std::map<Key, Value>::iterator> _locked_queue_pop() {
		DECL_LOCK_GUARD(m_queueLock);
		if (m_queue.empty()) { return std::nullopt; }
		return m_queue.pop();
	}
public:
	using typename BaseQ::KVPair;
	
	
	Queue_2Lock(const usize capacity)
		: BaseQ{ capacity }
	{}
	
	[[nodiscard]] usize size() {
		DECL_LOCK_GUARD(m_queueLock);
		return m_queue.size();
	}
	
	bool try_write(const Key &key, const Value &value) {
		std::unique_lock<std::mutex> uniqueLock{ m_mapLock };
		if (m_map.size() >= this->capacity()) { // try to dedup
			auto iter = m_map.find(key);
			if (iter == m_map.end()) {
				return false;
			}
			iter->second = value;
		}
		else if (auto [iter, inserted] = m_map.insert_or_assign(key, value); inserted) {
			uniqueLock.unlock();
			DECL_LOCK_GUARD(m_queueLock);
			m_queue.push(iter);
		}
		return true;
	}
	
	KVPair read() {
		while (true) {
			std::optional iter = _locked_queue_pop();
			if (iter.has_value()) {
				DECL_LOCK_GUARD(m_mapLock);
				return Utils::map_pop_iter(m_map, *iter);
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(Utils::WAIT_TIME);
		}
	}
};

#pragma once
#include "BaseQueue.h"
#include <map>
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
	using typename BaseQ::KVPair;
	
	
	Queue_2Lock(const usize capacity)
		: BaseQ{ capacity }
	{}
	
	[[nodiscard]] usize size() {
		DECL_LOCK_GUARD(m_queueLock);
		return m_queue.size();
	}
	
	bool try_write(const Key &key, const Value &value) {
		{ DECL_LOCK_GUARD(m_mapLock);
			auto iter = m_map.find(key);
			if (iter != m_map.end()) { // dedup
				iter->second = value;
				return true;
			}
			
			if (m_map.size() >= this->capacity()) {
				return false;
			}
			m_map[key] = value;
		}
		{ DECL_LOCK_GUARD(m_queueLock);
			m_queue.push(key);
		}
		return true;
	}
	
	KVPair read() {
		while (true) {
			std::optional<Key> key = _locked_queue_pop();
			if (key.has_value()) {
				Value val = _locked_map_pop(*key);
				return KVPair{ std::move(*key), std::move(val) };
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(Utils::WAIT_TIME);
		}
	}
};

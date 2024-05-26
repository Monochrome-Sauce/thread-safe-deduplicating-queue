#pragma once
#include "BaseQueue.h"
#include <map>


/* Single global lock.
 * This is the simplest and acts as a reference implementation.
 */
template<typename Key, typename Value>
class Queue_1Lock : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	
	Utils::Queue<Key> m_queue;
	std::map<Key, Value> m_map;
	std::mutex m_lock;
public:
	using typename BaseQ::KVPair;
	
	
	Queue_1Lock(const usize capacity)
		: BaseQ{ capacity }
	{}
	
	[[nodiscard]] usize size() {
		DECL_LOCK_GUARD(m_lock);
		return m_queue.size();
	}
	
	bool try_write(const Key &key, const Value &value) {
		DECL_LOCK_GUARD(m_lock);
		if (m_queue.size() >= this->capacity()) { // try to dedup
			auto iter = m_map.find(key);
			if (iter == m_map.end()) {
				return false;
			}
			iter->second = value;
		}
		else if (m_map.insert_or_assign(key, value).second) {
			m_queue.push(key);
		}
		return true;
	}
	
	constexpr KVPair read() {
		while (true) {
			if (DECL_LOCK_GUARD(m_lock); !m_queue.empty()) {
				Key key = m_queue.pop();
				
				auto iter = m_map.find(key);
				assert(iter != m_map.end());
				Value val = std::move(iter->second);
				m_map.erase(iter);
				
				return KVPair{ std::move(key), std::move(val) };
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(Utils::WAIT_TIME);
		}
	}
};

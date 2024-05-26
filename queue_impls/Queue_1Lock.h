#pragma once
#include "BaseQueue.h"


/* Single global lock.
 * This is the simplest and acts as a reference implementation.
 */
template<typename Key, typename Value>
class Queue_1Lock : public BaseQueue<Key, Value>
{
private:
	using BaseQ = BaseQueue<Key, Value>;
	using typename BaseQ::KVPair;
	
	Utils::Queue<typename std::map<Key, Value>::iterator> m_queue;
	std::map<Key, Value> m_map;
	std::mutex m_lock;
public:
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
		else if (auto [iter, inserted] = m_map.insert_or_assign(key, value); inserted) {
			m_queue.push(iter);
		}
		return true;
	}
	
	constexpr KVPair read() {
		while (true) {
			if (DECL_LOCK_GUARD(m_lock); !m_queue.empty()) {
				return Utils::map_pop_iter(m_map, m_queue.pop());
			}
			
			if (this->stopped()) {
				throw Utils::queue_stopped_exception{};
			}
			Utils::sleep(Utils::WAIT_TIME);
		}
	}
};

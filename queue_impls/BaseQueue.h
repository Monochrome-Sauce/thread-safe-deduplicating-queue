#pragma once
#include "utils.h"
#include <atomic>


constexpr auto WAIT_TIME = chrono::milliseconds{ 1 };

using isize = int32_t;
using usize = std::make_unsigned_t<isize>;

/* Base type that implements common functionality */
template<typename Key, typename Value>
class BaseQueue
{
private:
	static_assert(std::is_same_v<Key, Utils::remove_cvref_t<Key>>);
	static_assert(std::is_same_v<Value, Utils::remove_cvref_t<Value>>);
	
	const uint32_t m_capacity;
	std::atomic<bool> m_stop;
public:
	using KVPair = std::pair<Key, Value>;
	using key_type = Key;
	using value_type = Value;
	
	
	BaseQueue(const usize capacity)
		: m_capacity{ capacity }, m_stop{ false }
	{ printf("Creating queue with capacity of %'u.\n", m_capacity); }
	
	~BaseQueue() { this->stop(); }
	
	constexpr void stop() {
		if (!m_stop.exchange(true)) { printf("Stopping queue...\n"); }
	}
	
	[[nodiscard]] constexpr bool stopped() const { return m_stop.load(); }
	[[nodiscard]] constexpr usize capacity() const { return m_capacity; }
};

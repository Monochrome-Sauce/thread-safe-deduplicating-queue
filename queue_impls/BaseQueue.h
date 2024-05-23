#pragma once
#include "utils.h"


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
	struct KVPair : public std::pair<Key, Value>
	{
		using std::pair<Key, Value>::pair;
		[[nodiscard]] constexpr Key& key() noexcept { return this->first; }
		[[nodiscard]] constexpr Value& value() noexcept { return this->first; }
		[[nodiscard]] constexpr const Key& key() const noexcept { return this->first; }
		[[nodiscard]] constexpr const Value& value() const noexcept { return this->first; }
	};
	
	using usize = decltype(m_capacity);
	using isize = std::make_signed_t<usize>;
	
	using key_type = Key;
	using value_type = Value;
	
	
	BaseQueue(const usize capacity)
		: m_capacity{ capacity }, m_stop{ false }
	{ printf("Creating queue with capacity of %u\n", m_capacity); }
	
	~BaseQueue() {
		printf("Destroying queue...\n");
		this->stop();
	}
	
	constexpr void stop() {
		if (m_stop.exchange(true)) {
			printf("Queue was already stopped.\n");
		}
		else {
			printf("Stopping queue...\n");
		}
	}
	
	[[nodiscard]] constexpr bool stopped() const { return m_stop.load(); }
	[[nodiscard]] constexpr usize capacity() const { return m_capacity; }
};

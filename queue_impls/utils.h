#pragma once
#include <array>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>


#define CONCAT__(a, b) a##b
#define CONCAT(a, b) CONCAT__(a, b)
#define STRINGIFY(...) #__VA_ARGS__

#define DECL_LOCK_GUARD(_mutex) std::lock_guard<std::mutex> CONCAT(guard, __LINE__){ (_mutex) }


namespace chrono = std::chrono;

namespace Utils
{
	template<typename T>
	using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
	
	class queue_stopped_exception : public std::runtime_error
	{
	public:
		queue_stopped_exception()
			: std::runtime_error{ "Queue has been stopped already" }
		{}
		
		virtual ~queue_stopped_exception() {};
	};
	
	/* `std::queue` with iterators. */
	template<typename ...Args>
	class Queue : public std::queue<Args...>
	{
	private:
		using Self = std::queue<Args...>;
	public:
		using Self::Self; // export constructors
		
		[[nodiscard]] constexpr auto begin() { return this->c.begin(); }
		[[nodiscard]] constexpr auto end() { return this->c.end(); }
		
		[[nodiscard]] constexpr auto begin() const { return this->c.begin(); }
		[[nodiscard]] constexpr auto end() const { return this->c.end(); }
		
		[[nodiscard]] constexpr auto pop() {
			auto val = std::move(Self::front());
			Self::pop();
			return val;
		}
	};
	
	template<typename T>
	struct ReverseIterationAdaptor
	{
		T &m_iterable;
		
		[[nodiscard]] constexpr auto begin() { return m_iterable.rbegin(); }
		[[nodiscard]] constexpr auto end() { return m_iterable.rend(); }
		
		[[nodiscard]] constexpr auto begin() const { return m_iterable.rbegin(); }
		[[nodiscard]] constexpr auto end() const { return m_iterable.rend(); }
		
		[[nodiscard]] constexpr auto cbegin() const { return m_iterable.rbegin(); }
		[[nodiscard]] constexpr auto cend() const { return m_iterable.rend(); }
	};
	
	template<typename T>
	[[nodiscard]] constexpr
	ReverseIterationAdaptor<T> iter_reverse(T &iterable) {
		return ReverseIterationAdaptor<T>{ iterable };
	}
	
	[[nodiscard]] constexpr
	chrono::milliseconds to_milli(const chrono::nanoseconds time) {
		return chrono::duration_cast<chrono::milliseconds>(time);
	};
	
	inline void sleep(const chrono::milliseconds time) {
		std::this_thread::sleep_for(time);
	}
	
	template<typename K, typename V>
	[[nodiscard]] constexpr
	auto map_pop_iter(std::map<K, V> &map, typename std::map<K, V>::iterator iter) {
		std::pair result { std::move(iter->first), std::move(iter->second) };
		map.erase(iter);
		return result;
	}
}

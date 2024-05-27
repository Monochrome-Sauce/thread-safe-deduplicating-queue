#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <unistd.h>


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
	struct reverse_iteration_adaptor
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
	reverse_iteration_adaptor<T> iter_reverse(T &iterable) {
		return reverse_iteration_adaptor<T>{ iterable };
	}
	
	
	constexpr auto WAIT_TIME = chrono::milliseconds{ 1 };
	
	inline void sleep(chrono::milliseconds time) {
		usleep(chrono::duration_cast<chrono::microseconds>(time).count());
	}
	
	template<typename T, std::size_t N, typename F, std::size_t... I>
	[[nodiscard]] constexpr
	auto make_array__impl(F &&func, std::index_sequence<I...>) {
		return std::array<T, N>{ {func(I)...} };
	}
	
	template<typename T, size_t N, typename F>
	[[nodiscard]] constexpr
	std::array<T, N> make_array(F &&func) {
		return make_array__impl<T>(std::forward<F>(func), std::make_index_sequence<N>{});
	}
	
	[[nodiscard]] constexpr
	chrono::milliseconds to_milli(chrono::nanoseconds t) {
		return chrono::duration_cast<chrono::milliseconds>(t);
	};
	
	template<typename K, typename V>
	[[nodiscard]] constexpr
	auto map_pop_iter(std::map<K, V> &map, typename std::map<K, V>::iterator iter) {
		std::pair result { std::move(iter->first), std::move(iter->second) };
		map.erase(iter);
		return result;
	}
}

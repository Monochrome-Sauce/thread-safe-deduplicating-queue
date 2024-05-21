#pragma once
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unistd.h>


#define CONCAT__(a, b) a##b
#define CONCAT(a, b) CONCAT__(a, b)
#define STRINGIFY(x) #x

#define DECL_LOCK_GUARD(_mutex) std::lock_guard<std::mutex> CONCAT(guard, __LINE__){ (_mutex) }

namespace chrono = std::chrono;
namespace fs = std::filesystem;

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
	
	
	constexpr auto WAIT_TIME = chrono::milliseconds{ 1 };
	
	inline void sleep(chrono::milliseconds time) {
		usleep(chrono::duration_cast<chrono::microseconds>(time).count());
	}
}

#pragma once
#include <cassert>
#include <random>
#include <thread>


enum class DataSet {
	LINEAR_8BIT, // frequent duplication
	LINEAR_16BIT, // infrequent duplication
	RANDOM, // nearly no duplication
	ZEROES, // constant duplication
};

template<DataSet DATA_SET>
class DataSource
{
private:
	using Counter = std::conditional_t<DATA_SET == DataSet::LINEAR_8BIT, uint8_t, uint16_t>;
	
	const uint64_t m_threadId;
	std::mt19937 m_rngGen;
	Counter m_linearCounter;
	
	struct Data {
		uint64_t id = {};
		int32_t val = 0;
	};
	
	
	template<typename Int>
	[[nodiscard]] constexpr Int _linear() {
		return static_cast<Int>(m_linearCounter += (
			m_threadId % std::numeric_limits<Counter>::max()
		));
	}
	
	template<typename Int>
	[[nodiscard]] constexpr Int _rand() {
		std::uniform_int_distribution<Int> distribution(
			std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max()
		);
		return distribution(m_rngGen);
	}
	
	[[nodiscard]] constexpr Data _get_data() {
		switch (DATA_SET)
		{
		case DataSet::LINEAR_8BIT:
		case DataSet::LINEAR_16BIT:
			return {
				.id = _linear<uint64_t>(),
				.val = _linear<int32_t>(),
			};
		case DataSet::RANDOM:
			return {
				.id = _rand<uint64_t>(),
				.val = _rand<int32_t>(),
			};
		case DataSet::ZEROES: return Data{};
		}
	}
	
	
	[[nodiscard]] static auto _get_thread_id() {
		using Result = uint64_t;
		std::thread::id id = std::this_thread::get_id();
		static_assert(sizeof (Result) <= sizeof (id));
		return *reinterpret_cast<const Result*>(&id);
	}
public:
	DataSource()
		: m_threadId { _get_thread_id() }
		, m_rngGen{ m_threadId }
		, m_linearCounter{ static_cast<Counter>(m_threadId) }
	{}
	
	~DataSource() {}
	
	[[nodiscard]] std::pair<std::string, int64_t> get() {
		const Data data = _get_data();
		
		char buf[sizeof (data.id) * 2 + 1] = {};
		const int len = snprintf(buf, sizeof (buf), "%016lX", data.id);
		assert(len == (sizeof (buf) - 1));
		
		return std::pair<std::string, int64_t>(std::string_view(buf, len), data.val);
	}
};

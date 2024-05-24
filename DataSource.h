#pragma once
#include <cassert>
#include <random>
#include <thread>


enum class DataSet {
	LINEAR,
	RANDOM,
	ZEROES,
};

template<DataSet DATA_SET>
class DataSource
{
private:
	const std::thread::id m_threadId;
	std::mt19937 m_rngGen;
	uint8_t m_linearCounter;
	
	struct Data {
		uint64_t id = {};
		int32_t val = 0;
	};
	
	
	template<typename Int>
	[[nodiscard]] constexpr Int _linear() {
		using Count = decltype(m_linearCounter);
		static_assert(sizeof (Count) <= sizeof (m_threadId));
		return Int(m_linearCounter += *reinterpret_cast<const Count*>(&m_threadId));
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
		case DataSet::LINEAR:
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
public:
	DataSource()
		: m_threadId { std::this_thread::get_id() }
		, m_rngGen{}
		, m_linearCounter{ 0 }
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

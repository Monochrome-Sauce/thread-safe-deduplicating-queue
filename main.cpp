#include "queue_impls/Queue_1Lock.h"
#include "queue_impls/Queue_2Lock.h"
#include <array>
#include <random>
#include <thread>


#define check_true(_expr) do { \
	if (!static_cast<bool>(_expr)) { \
		printf("\e[31mFailed"); \
	} else { \
		printf("\e[32mPassed"); \
	} \
	printf(" at line %u:\e[0m check_true(\e[33m%s\e[0m)\n", __LINE__, STRINGIFY(_expr)); \
} while (0)

#define check_false(_expr) do { \
	if (!static_cast<bool>(_expr)) { \
		printf("\e[31mFailed"); \
	} else { \
		printf("\e[32mPassed"); \
	} \
	printf(" at line %u:\e[0m check_false(\e[33m%s\e[0m)\n", __LINE__, STRINGIFY(_expr)); \
} while (0)

#define check_reachable() do { \
	printf("\e[32mPassed at line %u:\e[0m check_reachable()\n", __LINE__); \
} while (0)


struct Key { std::string _; };
inline bool operator<(const Key &a, const Key &b) { return a._ < b._; }

struct Value { int64_t _; };
inline bool operator<(const Value &a, const Value &b) { return a._ < b._; }


template<typename Queue>
static void test() {
	Queue queue{ 2 };
	
	check_true(queue.size() == 0);
	check_true(queue.try_write(Key{ "1" }, Value{ 968137 }));
	check_true(queue.size() == 1);
	check_false(queue.try_write(Key{ "2" }, Value{ 34905 }));
	check_true(queue.size() == 2);
	
	static_cast<void>(queue.read());
	check_true(queue.size() == 1);
	
	queue.stop();
	try {
		static_cast<void>(queue.read());
	}
	catch (const Utils::queue_stopped_exception&) {
		check_reachable();
	}
}


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

template<typename Queue, size_t N_CYCLES = (1 << 12)>
static void blackbox_benchmark() {
	Queue queue{ UINT32_MAX };
	
	std::array<std::thread, 128> writers;
	std::array<std::thread, writers.size()> readers;
	
	printf("Running writers for %'zu cycles each...\n", N_CYCLES);
	for (std::thread &thrd : writers) {
		thrd = std::thread([&queue]() {
			DataSource<DataSet::RANDOM> src;
			try {
				for (size_t i = 0; i < N_CYCLES; ++i) {
					auto [id, number] = src.get();
					bool res = queue.try_write(Key{ id }, Value{ number });
					static_cast<void>(res);
				}
			}
			catch (const Utils::queue_stopped_exception&) {
				printf("Writer forced to stop\n");
			}
		});
	}
	
	printf("Running readers...\n");
	for (std::thread &thrd : readers) {
		thrd = std::thread([&queue]() {
			try {
				while (true) { queue.read(); }
			}
			catch (const Utils::queue_stopped_exception&) {}
		});
	}
	
	
	const chrono::time_point start = chrono::system_clock::now();
	printf("Waiting for writers...\n");
	for (std::thread &thrd : writers) { thrd.join(); }
	
	queue.stop();
	printf("Waiting for readers...\n");
	for (std::thread &thrd : readers) { thrd.join(); }
	
	const chrono::nanoseconds diff = chrono::system_clock::now() - start;
	printf("Queue has %'u values remaining.\n", queue.size());
	printf("\nBenchmark result: \e[33m%'ldms\e[0m\n",
		chrono::duration_cast<chrono::milliseconds>(diff).count()
	);
}


#define RUN_TEST(...) do { \
	printf(">>> Running test with type: \e[33m" STRINGIFY(__VA_ARGS__) "\e[0m\n"); \
	test<__VA_ARGS__>(); \
	printf("==========================================\n\n\n"); \
} while (0)

#define RUN_BLACKBOX_BENCHMARK(...) do { \
	printf(">>> Running blackbox_benchmark with type: \e[33m" STRINGIFY(__VA_ARGS__) "\e[0m\n"); \
	blackbox_benchmark<__VA_ARGS__>(); \
	printf("==========================================\n\n\n"); \
} while (0)

int main() {
	setlocale(LC_NUMERIC, ""); // to add commas in printf
	RUN_TEST(Queue_1Lock<Key, Value>);
	RUN_TEST(Queue_2Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_1Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_2Lock<Key, Value>);
	return 0;
}

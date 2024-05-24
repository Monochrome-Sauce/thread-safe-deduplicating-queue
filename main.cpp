#include "DataSource.h"
#include "queue_impls/Queue_1Lock.h"
#include "queue_impls/Queue_1LockSharded.h"
#include "queue_impls/Queue_2Lock.h"


struct Key { std::string _; };
inline bool operator<(const Key &a, const Key &b) { return a._ < b._; }
template<> struct std::hash<Key> {
	size_t operator()(const Key &self) const noexcept {
		return std::hash<std::string>{}(self._);
	}
};

struct Value { int64_t _; };
inline bool operator<(const Value &a, const Value &b) { return a._ < b._; }
template<> struct std::hash<Value> {
	size_t operator()(const Value &self) const noexcept {
		return std::hash<int64_t>{}(self._);
	}
};


#define check_true(_expr) do { \
	if (static_cast<bool>(_expr)) { \
		printf("\e[32mPassed"); \
	} else { \
		printf("\e[31mFailed"); \
	} \
	printf(" at line %u:\e[0m check_true(\e[33m%s\e[0m)\n", __LINE__, STRINGIFY(_expr)); \
} while (0)

#define check_false(_expr) do { \
	if (!static_cast<bool>(_expr)) { \
		printf("\e[32mPassed"); \
	} else { \
		printf("\e[31mFailed"); \
	} \
	printf(" at line %u:\e[0m check_false(\e[33m%s\e[0m)\n", __LINE__, STRINGIFY(_expr)); \
} while (0)

#define check_reachable_true() do { \
	printf("\e[32mPassed at line %u:\e[0m check_reachable()\n", __LINE__); \
} while (0)

#define check_reachable_false() do { \
	printf("\e[31mFailed at line %u:\e[0m check_reachable()\n", __LINE__); \
} while (0)

template<typename Queue>
static void test() {
	Queue queue{ 2 };
	
	check_true(queue.size() == 0);
	check_true(queue.try_write(Key{ "1" }, Value{ 968137 }));
	check_true(queue.try_write(Key{ "1" }, Value{ -41123 }));
	check_true(queue.size() == 1);
	check_true(queue.try_write(Key{ "2" }, Value{ 34905 }));
	check_true(queue.size() == 2);
	check_false(queue.try_write(Key{ "3" }, Value{ -34905 }));
	
	static_cast<void>(queue.read());
	check_true(queue.size() == 1);
	static_cast<void>(queue.read());
	check_true(queue.size() == 0);
	
	queue.stop();
	try {
		static_cast<void>(queue.read());
	}
	catch (const Utils::queue_stopped_exception&) {
		check_reachable_true();
	}
	try {
		check_true(queue.try_write(Key{ "859" }, Value{ 69821 }));
		check_true(queue.try_write(Key{ "312" }, Value{ 9752 }));
		check_false(queue.try_write(Key{ "592" }, Value{ 5823 }));
		check_false(queue.try_write(Key{ "4124" }, Value{ 978736 }));
		check_true(queue.try_write(Key{ "312" }, Value{ 21 }));
		check_reachable_true();
	}
	catch (const Utils::queue_stopped_exception&) {
		check_reachable_false();
	}
}

template<typename Queue, size_t N_CYCLES = (1 << 12)>
static void blackbox_benchmark() {
	Queue queue{ UINT32_MAX };
	
	std::array<std::thread, 128> writers;
	std::array<std::thread, writers.size()> readers;
	
	printf("Running writers for %'zu cycles each...\n", N_CYCLES);
	for (std::thread &thrd : writers) {
		thrd = std::thread([&queue]() {
			DataSource<DataSet::RANDOM> src;
			for (size_t i = 0; i < N_CYCLES; ++i) {
				auto [id, number] = src.get();
				static_cast<void>(queue.try_write(Key{ id }, Value{ number }));
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
	RUN_TEST(Queue_1LockSharded<Key, Value, 16>);
	RUN_TEST(Queue_2Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_1Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_1LockSharded<Key, Value, 16>);
	RUN_BLACKBOX_BENCHMARK(Queue_2Lock<Key, Value>);
	return 0;
}

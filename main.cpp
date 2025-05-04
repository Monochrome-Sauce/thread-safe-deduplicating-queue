#include "DataSource.h"
#include "queue_impls/Queue_1Lock.h"
#include "queue_impls/Queue_1LockSharded.h"
#include "queue_impls/Queue_2Lock.h"
#include "queue_impls/Queue_2LockSharded.h"
#include "queue_impls/Queue_SplitSharded.h"


struct Key { std::string _; };
inline bool operator<(const Key &a, const Key &b) { return a._ < b._; }
template<> struct std::hash<Key> {
	size_t operator()(const Key &self) const noexcept {
		return std::hash<std::string>{}(self._);
	}
};

struct Value { int64_t _; };


constexpr static void check__impl(const bool passed, const size_t line, const char *msg) {
	assert(msg != nullptr);
	printf("%s at line %zu:\e[m %s\n",
		passed ? "\e[32mPassed" : "\e[31mFailed",
		line, msg
	);
}

#define check_true(_expr) \
	check__impl(bool(_expr), __LINE__, "check_true(\e[33m" STRINGIFY(_expr) "\e[m)")

#define check_reachable_true() \
	check__impl(true, __LINE__, "check_reachable()")

#define check_reachable_false() \
	check__impl(false, __LINE__, "check_reachable()")

static void print_kvpair(const std::pair<Key, Value> &pair) {
	printf("std::pair<Key, Value>{ \e[94mkey\e[m: \e[96m%s\e[m, \e[94mvalue\e[m: \e[96m%ld\e[m }\n",
		pair.first._.c_str(), pair.second._
	);
}

template<typename Queue>
static void test() {
	Queue queue{ 2 };
	
	check_true(queue.size() == 0);
	check_true(queue.try_write(Key{ "1" }, Value{ 968137 }));
	check_true(queue.try_write(Key{ "1" }, Value{ -41123 }));
	check_true(queue.size() == 1);
	check_true(queue.try_write(Key{ "2" }, Value{ 34905 }));
	check_true(queue.size() == 2);
	check_true(!queue.try_write(Key{ "3" }, Value{ -34905 }));
	
	print_kvpair(queue.read());
	check_true(queue.size() == 1);
	print_kvpair(queue.read());
	check_true(queue.size() == 0);
	
	queue.stop();
	try {
		print_kvpair(queue.read());
	}
	catch (const Utils::queue_stopped_exception&) {
		check_reachable_true();
	}
	try {
		check_true(queue.try_write(Key{ "859" }, Value{ 69821 }));
		check_true(queue.try_write(Key{ "312" }, Value{ 9752 }));
		check_true(!queue.try_write(Key{ "592" }, Value{ 5823 }));
		check_true(!queue.try_write(Key{ "4124" }, Value{ 978736 }));
		check_true(queue.try_write(Key{ "312" }, Value{ 21 }));
		check_reachable_true();
	}
	catch (const Utils::queue_stopped_exception&) {
		check_reachable_false();
	}
}

template<typename Queue>
static void blackbox_benchmark() {
	constexpr DataSet DATA_SET =  DataSet::LINEAR_16BIT;
	constexpr size_t N_CYCLES = (1 << 16);
	constexpr size_t N_THREADS = 128;
	Queue queue{ N_CYCLES };
	
	std::array<std::thread, N_THREADS> writers;
	std::array<std::thread, N_THREADS> readers;
	
	printf("Running %zu readers...\n", readers.size());
	for (std::thread &thrd : readers) {
		thrd = std::thread([&queue]() {
			try {
				while (true) { queue.read(); }
			}
			catch (const Utils::queue_stopped_exception&) {}
		});
	}
	
	printf("Running %zu writers for %'zu cycles each (total cycles: %'zu)...\n",
		writers.size(), N_CYCLES, writers.size() * N_CYCLES
	);
	std::atomic<bool> waitFlag = true;
	for (std::thread &thrd : writers) {
		thrd = std::thread([&queue, &waitFlag]() {
			DataSource<DATA_SET> src;
			while (waitFlag.load()) { Utils::sleep(chrono::milliseconds{ 1 }); }
			for (size_t i = 0; i < N_CYCLES; ++i) {
				auto [id, number] = src.get();
				if (!queue.try_write(Key{ id }, Value{ number })) {
					printf("\e[31mCapacity reached at cycle %'zu.\e[m\n", i);
				}
			}
		});
	}
	Utils::sleep(chrono::seconds{ 2 });
	const chrono::time_point tpStart = chrono::system_clock::now();
	waitFlag.store(false);
	
	
	const chrono::time_point tpWaitWriters = chrono::system_clock::now();
	for (std::thread &thrd : writers) { thrd.join(); }
	const chrono::time_point tpWaitReaders = chrono::system_clock::now();
	queue.stop();
	for (std::thread &thrd : readers) { thrd.join(); }
	const chrono::time_point tpEnd = chrono::system_clock::now();
	
	printf("> Benchmark ran for \e[93m%'ld\e[mms with \e[93m%'u\e[m items left in queue.\n",
		Utils::to_milli(tpEnd - tpStart).count(), queue.size()
	);
	printf("Waited \e[33m%'ld\e[mms for writer threads.\n",
		Utils::to_milli(tpWaitReaders - tpWaitWriters).count()
	);
	printf("Waited \e[33m%'ld\e[mms for reader threads.\n",
		Utils::to_milli(tpEnd - tpWaitReaders).count()
	);
	printf("Waited \e[33m%'ld\e[mms on all threads.\n",
		Utils::to_milli(tpEnd - tpWaitWriters).count()
	);
}


#define RUN_TEST(...) do { \
	puts("================================================================================"); \
	puts(">>> Running test with type: \e[33m" STRINGIFY(__VA_ARGS__) "\e[m"); \
	test<__VA_ARGS__>(); \
	puts("\n"); \
} while (0)

#define RUN_BLACKBOX_BENCHMARK(...) do { \
	puts("================================================================================"); \
	puts(">>> Running blackbox_benchmark with type: \e[33m" STRINGIFY(__VA_ARGS__) "\e[m"); \
	blackbox_benchmark<__VA_ARGS__>(); \
	puts("\n"); \
} while (0)

int main() {
	setlocale(LC_NUMERIC, ""); // to add commas in printf
	RUN_TEST(Queue_1Lock<Key, Value>);
	RUN_TEST(Queue_1LockSharded<Key, Value, 16>);
	RUN_TEST(Queue_2Lock<Key, Value>);
	RUN_TEST(Queue_2LockSharded<Key, Value, 16>);
	RUN_TEST(Queue_SplitSharded<Key, Value, 16>);
	RUN_BLACKBOX_BENCHMARK(Queue_1Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_1LockSharded<Key, Value, 16>);
	RUN_BLACKBOX_BENCHMARK(Queue_2Lock<Key, Value>);
	RUN_BLACKBOX_BENCHMARK(Queue_2LockSharded<Key, Value, 16>);
	RUN_BLACKBOX_BENCHMARK(Queue_SplitSharded<Key, Value, 16>);
	return 0;
}

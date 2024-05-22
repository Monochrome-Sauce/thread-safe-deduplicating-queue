#include "queue_impls/Queue_1Lock.h"


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

int main() {
	test<Queue_1Lock<Key, Value>>();
	return 0;
}

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <set>
#include <unordered_set>

using namespace std;
using namespace chrono;

constexpr int MAX_THREADS = 32;

constexpr int RET_IM_POP = -1;
constexpr int RET_SOLO_TIMEOUT = -2;
constexpr int RET_BUSY_TIMEOUT = -3;
constexpr int STATUS_MASK = 0xC0000000;
constexpr int VALUE_MASK = 0x3FFFFFFF;
constexpr int ST_EMPTY = 0;
constexpr int ST_WAITING = 0x40000000;
constexpr int ST_BUSY = 0x80000000;
constexpr int WAIT_LOOP = 100;

thread_local int eliminationCount = 0;
atomic_int eliminationSum = 0;

class NODE {
public:
	int v;
	NODE* volatile next;
	NODE() : v(-1), next(nullptr) {}
	NODE(int x) : v(x), next(nullptr) {}
};

class Exchanger {
	volatile int slot;
	bool CAS(int old_slot, int new_status, int new_value)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(&slot),
			&old_slot, new_status | new_value);
	}
public:
	Exchanger() : slot{ 0 } {}
	int exchange(int x, bool* busy)
	{
		if (x == -1) {
			x = x & VALUE_MASK;
		}
		if ((x & STATUS_MASK) != ST_EMPTY) {
			cout << "Invalid Value\n";
			exit(-1);
		}
		while (true) {
			int curr_slot = slot;
			int status = curr_slot & STATUS_MASK;
			int value = curr_slot & VALUE_MASK;
			switch (status)
			{
			case ST_EMPTY:
				if (CAS(curr_slot, ST_WAITING, x)) {
					int loop_count = WAIT_LOOP;
					while ((slot & STATUS_MASK) == ST_WAITING) {
						if (loop_count-- == 0) {
							if (CAS(ST_WAITING | x, ST_EMPTY, 0)) {
								return RET_SOLO_TIMEOUT;
							}
							else {
								break;
							}
						}
					}
					int ret = slot & VALUE_MASK;
					slot = 0;
					if (ret == VALUE_MASK) {
						return -1;
					}
					return ret;
				}
				else {
					continue;
				}
			case ST_WAITING:
				if (CAS(curr_slot, ST_BUSY, x)) {
					if (value == VALUE_MASK) {
						return -1;
					}
					return value;
				}
				else {
					*busy = true;
					continue;
				}
			case ST_BUSY:
				*busy = true;
				continue;
			default:
				cout << "Unknown Status Error\n";
				exit(-1);
			}
		}
	}
	void unsafe_clear() {
		slot = 0;
	}
};

class EliminationArray {
	volatile int range;
	Exchanger ex[MAX_THREADS];
	void CAS(int old_r, int new_r)
	{
		atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(&range),
			&old_r, new_r);
	}
public:
	EliminationArray() : range{ 1 } {}
	int visit(int x)
	{
		int cur_range = range;
		int slot = rand() % cur_range;
		bool busy = false;
		int ret = ex[slot].exchange(x, &busy);
		if ((busy == true) && cur_range < MAX_THREADS / 2) {
			CAS(cur_range, cur_range + 1);
		}
		if (RET_SOLO_TIMEOUT == ret && cur_range > 1) {
			CAS(cur_range, cur_range - 1);
		}
		return ret;
	}
	void unsafe_clear()
	{
		for (int i = 0; i < MAX_THREADS; ++i) {
			ex[i].unsafe_clear();
		}
		range = 1;
	}
};

class Stack {
	EliminationArray ea;
	NODE* volatile top;
	bool CAS(NODE* old_p, NODE* n_ptr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(&top),
			reinterpret_cast<long long*>(&old_p),
			reinterpret_cast<long long>(n_ptr));
	}
public:
	Stack() : top(nullptr) {}
	~Stack() { clear(); }
	void Push(int x)
	{
		NODE* node = new NODE{ x };
		while (true) {
			NODE* tmp = top;
			node->next = tmp;
			if (CAS(tmp, node)) return;
			if (ea.visit(x) == RET_IM_POP) return;
		}
	}
	int Pop()
	{
		while (true) {
			NODE* ptr = top;
			if (!ptr) { return -2; }
			NODE* next = ptr->next;
			int temp = ptr->v;
			if (CAS(ptr, next)) return temp;
			int ret = ea.visit(-1);

			if (ret < 0) {
				++eliminationCount;
				continue;
			}
			return ret;
		}
	}
	void print20()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) break;
			cout << p->v << ", ";
			p = p->next;
		}
		cout << endl;
	}
	void clear()
	{
		NODE* p = top;
		while (p != nullptr) {
			NODE* t = p;
			p = p->next;
			delete t;
		}
		top = nullptr;
		ea.unsafe_clear();
	}
};

thread_local int tls_thid;

int Thread_id()
{
	return tls_thid;
}

typedef Stack MY_STACK;
MY_STACK my_stack;

constexpr int LOOP = 10000000;

struct HISTORY {
	vector <int> push_values, pop_values;
};

atomic_int stack_size = 0;

void worker(int num_threads, int th_id)
{
	tls_thid = th_id;
	eliminationCount = 0;
	for (int i = 0; i < LOOP / num_threads; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			my_stack.Push(i);
		}
		else {
			my_stack.Pop();
		}
	}
	eliminationSum += eliminationCount;
}

void worker_check(int num_threads, int th_id, HISTORY& h)
{
	tls_thid = th_id;
	eliminationCount = 0;
	for (int i = 0; i < LOOP / num_threads; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			h.push_values.push_back(i);
			stack_size++;
			my_stack.Push(i);
		}
		else {
			stack_size--;
			int res = my_stack.Pop();
			if (res == -2) {
				stack_size++;
				if (stack_size > num_threads) {
					cout << "ERROR Non_Empty Stack Returned NULL\n";
					exit(-1);
				}
			}
			else h.pop_values.push_back(res);
		}
	}
	eliminationSum += eliminationCount;
}

void check_history(MY_STACK& my_stack, vector <HISTORY>& h)
{
	unordered_multiset <int> pushed, poped, in_stack;

	for (auto& v : h)
	{
		for (auto num : v.push_values) pushed.insert(num);
		for (auto num : v.pop_values) poped.insert(num);
		while (true) {
			int num = my_stack.Pop();
			if (num == -2) break;
			poped.insert(num);
		}
	}
	for (auto num : pushed) {
		if (poped.count(num) < pushed.count(num)) {
			cout << "Pushed Number " << num << " does not exists in the STACK.\n";
			exit(-1);
		}
		if (poped.count(num) > pushed.count(num)) {
			cout << "Pushed Number " << num << " is poped more than " << poped.count(num) - pushed.count(num) << " times.\n";
			exit(-1);
		}
	}
	for (auto num : poped)
		if (pushed.count(num) == 0) {
			std::multiset <int> sorted;
			for (auto num : poped)
				sorted.insert(num);
			cout << "There was elements in the STACK no one pushed : ";
			int count = 20;
			for (auto num : sorted)
				cout << num << ", ";
			cout << endl;
			exit(-1);

		}
	cout << "NO ERROR detectd.\n";
}

int main()
{
	cout << "==== Error Checking =====\n";
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		vector <HISTORY> log(num_threads);
		my_stack.clear();
		stack_size = 0;
		eliminationSum = 0;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, num_threads, i, ref(log[i]));
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_stack.print20();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << ", Elimination : " << eliminationSum << endl;
		check_history(my_stack, log);
	}

	cout << "======== BENCHMARKING =========\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		vector <HISTORY> log(num_threads);
		my_stack.clear();
		stack_size = 0;
		eliminationSum = 0;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_stack.print20();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << ", Elimination : " << eliminationSum << endl;
	}
}
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

class null_mutex {
public:
	void lock() {}
	void unlock() {}
};

class NODE {
public:
	int v;
	NODE* volatile next;
	NODE() : v(-1), next(nullptr) {}
	NODE(int x) : v(x), next(nullptr) {}
};

class LF_STACK {
	NODE* volatile top;
	bool CAS(NODE* old_p, NODE* n_ptr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(&top),
			reinterpret_cast<long long*>(&old_p),
			reinterpret_cast<long long>(n_ptr));
	}
public:
	LF_STACK() : top(nullptr) {}
	~LF_STACK() { clear(); }
	void Push(int x)
	{
		NODE* node = new NODE{ x };
		while (true) {
			NODE* tmp = top;
			node->next = tmp;
			if (CAS(node->next, node)) return;
		}
	}
	int Pop()
	{
		while (true) {
			NODE* ptr = top;
			if (!ptr) { return -2; }
			NODE* next = ptr->next;
			int temp = ptr->v;
			if (CAS(ptr, next)) {
				return temp;
			}
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
	}
};

thread_local int tls_thid;

int Thread_id()
{
	return tls_thid;
}

typedef LF_STACK MY_STACK;
MY_STACK my_stack;

constexpr int LOOP = 10000000;

struct HISTORY {
	vector <int> push_values, pop_values;
};

atomic_int stack_size = 0;

void worker(int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < LOOP / num_threads; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			my_stack.Push(i);
		}
		else {
			my_stack.Pop();
		}
	}
}

void worker_check(int num_threads, int th_id, HISTORY& h)
{
	tls_thid = th_id;
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
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, num_threads, i, ref(log[i]));
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_stack.print20();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(my_stack, log);
	}

	cout << "======== BENCHMARKING =========\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		vector <HISTORY> log(num_threads);
		my_stack.clear();
		stack_size = 0;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_stack.print20();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
	}
}
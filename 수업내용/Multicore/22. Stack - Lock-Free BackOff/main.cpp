#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <array>
#include <set>
#include <unordered_set>
using namespace std;

constexpr int MAX_THREADS = 32;
constexpr long long POINTER_MASK = 0xFFFFFFFFFFFFFFFF;


struct Node {
public:
	Node() : value{ -1 }, next{ nullptr } {}
	Node(int v) : value{ v }, next{ nullptr } {}

private:


public:
	int value;
	Node* volatile next;
};

class BackOff {
	int minDelay, maxDelay;
	int limit;
public:
	BackOff(int min, int max)
		: minDelay(min), maxDelay(max), limit(min) {}
	void relax() {
		int delay = 0;
		if (limit != 0) delay = rand() % limit;
		limit *= 2;
		if (limit > maxDelay) limit = maxDelay;
		this_thread::sleep_for(chrono::microseconds(delay)); // 시스템 콜
	}
};

class Stack {
public:
	Stack() : top{ nullptr } {}
	~Stack()
	{
	}
	void push(int x)
	{
		BackOff backoff{ 1, MAX_THREADS };
		Node* node = new Node{ x };
		while (true) {
			Node* tmp = top;
			node->next = tmp;
			if (CAS(tmp, node)) return;
			backoff.relax();
		}
	}
	int pop()
	{
		BackOff backoff{ 1, MAX_THREADS };
		while (true) {
			Node* ptr = top;
			if (!ptr) { return -2; }
			Node* next = ptr->next;
			int temp = ptr->value;
			if (CAS(ptr, next)) {
				return temp;
			}
			backoff.relax();
		}
	}
	void unsafe_print()
	{
		Node* p = top;
		for (int i = 0; i < 20; ++i) {
			if (!p) break;
			cout << p->value << " ";
			p = p->next;
		}
		cout << endl;
	}
	void unsafe_clear()
	{
		Node* p = top;
		while (p) {
			Node* t = p;
			p = p->next;
			delete t;
		}
		p = nullptr;
		top = nullptr;
	}
private:
	bool CAS(Node* oldPtr, Node* newPtr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(&top),
			reinterpret_cast<long long*>(&oldPtr),
			reinterpret_cast<long long>(newPtr));
	}

private:
	Node* volatile top;
};

thread_local int tls_thid;

int Thread_id()
{
	return tls_thid;
}

Stack stack;
constexpr int RANGE = 10000000;


struct HISTORY {
	vector <int> push_values, pop_values;
};

atomic_int stack_size = 0;

void worker(int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < RANGE / num_threads; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			stack.push(i);
		}
		else {
			stack.pop();
		}
	}
}

void worker_check(int num_threads, int th_id, HISTORY& h)
{
	tls_thid = th_id;
	for (int i = 0; i < RANGE / num_threads; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			h.push_values.push_back(i);
			stack_size++;
			stack.push(i);
		}
		else {
			stack_size--;
			int res = stack.pop();
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

void check_history(Stack& my_stack, vector <HISTORY>& h)
{
	unordered_multiset<int> pushed, poped, in_stack;

	for (auto& v : h)
	{
		for (auto num : v.push_values) pushed.insert(num);
		for (auto num : v.pop_values) poped.insert(num);
		while (true) {
			int num = my_stack.pop();
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
			std::multiset<int> sorted;
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
	cout << "스택 : 무잠금 백 오프 동기화 (Stack : Lock-Free BackOff Synchronization)" << endl;
	cout << "==== Error Checking =====\n";
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		vector <HISTORY> log(num_threads);
		stack.unsafe_clear();
		stack_size = 0;
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, num_threads, i, ref(log[i]));
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		stack.unsafe_print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(stack, log);
	}

	cout << "======== BENCHMARKING =========\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		vector <HISTORY> log(num_threads);
		stack.unsafe_clear();
		stack_size = 0;
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		stack.unsafe_print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
	}
}
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <array>
#include <chrono>
using namespace std;

constexpr int MAX_LEVEL = 9;
constexpr int MAX_THREADS = 32;

template <typename T>
struct Node {
	T value;
	Node<T>* volatile next[MAX_LEVEL + 1];
	int topLevel;

	Node() : value{}, topLevel{ 0 }
	{
		for (auto& n : next) n = nullptr;
	}
	Node(T t, int top) : value{ t }, topLevel{ top }
	{
		for (auto& n : next) n = nullptr;
	}
};

template <typename T>
class SkipList {
	Node<T> head, tail;
	mutex m;
public:
	SkipList(T minNum, T maxNum)
	{
		head.value = minNum;
		tail.value = maxNum;
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	void find(T x, Node<T>* prev[], Node<T>* curr[])
	{
		prev[MAX_LEVEL] = &head;
		for (int cl = MAX_LEVEL; cl >= 0; --cl) {
			if (cl != MAX_LEVEL) prev[cl] = prev[cl + 1];
			curr[cl] = prev[cl]->next[cl];
			while (curr[cl]->value < x) {
				prev[cl] = curr[cl];
				curr[cl] = curr[cl]->next[cl];
			}
		}
	}
	void clear()
	{
		Node<T>* p = head.next[0];
		while (p != &tail) {
			Node<T>* temp = p;
			p = p->next[0];
			delete temp;
		}
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	bool add(T x)
	{
		Node<T>* prev[MAX_LEVEL + 1];
		Node<T>* current[MAX_LEVEL + 1];
		m.lock();
		find(x, prev, current);
		if (current[0]->value == x) {
			m.unlock();
			return false;
		}
		int level = 0;
		while (rand() % 2 == 1) {
			++level;
			if (level == MAX_LEVEL) break;
		}
		Node<T>* newNode = new Node<T>{ x, level };
		for (int i = 0; i <= level; ++i) {
			newNode->next[i] = current[i];
			prev[i]->next[i] = newNode;
		}
		m.unlock();
		return true;
	}
	bool remove(T x)
	{
		Node<T>* prev[MAX_LEVEL + 1];
		Node<T>* current[MAX_LEVEL + 1];
		m.lock();
		find(x, prev, current);
		if (current[0]->value != x) {
			m.unlock();
			return false;
		}
		for (int i = current[0]->topLevel; i >= 0; --i) {
			prev[i]->next[i] = current[0]->next[i];
		}
		delete current[0];
		m.unlock();
		return true;
	}
	bool contains(T x)
	{
		Node<T>* prev[MAX_LEVEL + 1];
		Node<T>* current[MAX_LEVEL + 1];
		m.lock();
		find(x, prev, current);
		if (current[0]->value == x) {
			m.unlock();
			return true;
		}
		m.unlock();
		return false;
	}
	void print()
	{
		Node<T>* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == &tail) break;
			p = p->next[0];

		}
		cout << endl;
	}
};

constexpr auto LOOP = 4000000;
constexpr auto KEY_RANGE = 1000;

thread_local int tls_thid;

int Thread_id()
{
	return tls_thid;
}

SkipList<int> set{ INT_MIN, INT_MAX };

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void worker(vector<HISTORY>* history, int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < LOOP / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			set.add(v);
			break;
		}
		case 1: {
			int v = rand() % KEY_RANGE;
			set.remove(v);
			break;
		}
		case 2: {
			int v = rand() % KEY_RANGE;
			set.contains(v);
			break;
		}
		}
	}
}

void worker_check(vector<HISTORY>* history, int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < LOOP / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(0, v, set.add(v));
			break;
		}
		case 1: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(1, v, set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(2, v, set.contains(v));
			break;
		}
		}
	}
}

void check_history(array <vector <HISTORY>, MAX_THREADS>& history, int num_threads)
{
	array<int, KEY_RANGE> survive = {};
	cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < KEY_RANGE; ++i) {
		int val = survive[i];
		if (val < 0) {
			cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			cout << "ERROR. The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (set.contains(i)) {
				cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == set.contains(i)) {
				cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	cout << " OK\n";
}

int main()
{
	cout << "SkipList : 성긴 동기화 (SkipList : Coarse-Grained Synchronization)" << endl;
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.clear();
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		set.print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}

	cout << "======== SPEED CHECK =============\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.clear();
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		set.print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}
}
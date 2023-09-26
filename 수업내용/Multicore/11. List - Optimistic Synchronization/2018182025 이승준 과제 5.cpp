#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
using namespace std;

constexpr int MAX_TH = 32;

class my_mutex
{
public:
	void lock() {}
	void unlock() {}
};

class NODE {
public:
	int value;
	NODE* next;
	mutex nlock;
	void lock() { nlock.lock(); }
	void unlock() { nlock.unlock(); }
};

class CSET {
	NODE head, tail;
	mutex g_lock;
public:
	CSET()
	{
		cout << "성긴 동기화 (Coarse-Grained Synchronization)" << endl;
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}
	~CSET()
	{
		clear();
	}
	void clear()
	{
		auto p = head.next;
		while (p != &tail) {
			auto temp = p;
			p = p->next;
			delete temp;
		}
		head.next = &tail;
	}
	bool add(int x)
	{
		NODE* prev = &head;
		g_lock.lock();
		NODE* curr = prev->next;
		while (curr->value < x) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->value == x) {
			g_lock.unlock();
			return false;
		}
		else {
			auto new_node = new NODE;
			new_node->value = x;
			new_node->next = curr;
			prev->next = new_node;
			g_lock.unlock();
			return true;
		}
	}

	bool remove(int x)
	{
		NODE* prev = &head;
		g_lock.lock();
		NODE* curr = prev->next;
		while (curr->value < x) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			g_lock.unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			g_lock.unlock();
			delete curr;
			return true;
		}
	}

	bool contains(int x)
	{
		NODE* prev = &head;
		g_lock.lock();
		NODE* curr = prev->next;
		while (curr->value < x) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			g_lock.unlock();
			return false;
		}
		else {
			g_lock.unlock();
			return true;
		}
	}

	void print()
	{
		auto p = head.next;
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == &tail) break;
			p = p->next;
		}
		cout << endl;
	}
};

class FSET {
	NODE head, tail;
public:
	FSET()
	{
		cout << "세밀한 동기화 (Fine-Grained Synchronization)" << endl;
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}
	~FSET()
	{
		clear();
	}
	void clear()
	{
		auto p = head.next;
		while (p != &tail) {
			auto temp = p;
			p = p->next;
			delete temp;
		}
		head.next = &tail;
	}
	bool add(int x)
	{
		auto new_node = new NODE;
		NODE* prev = &head;
		prev->lock();
		NODE* curr = prev->next;
		curr->lock();
		while (curr->value < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value == x) {
			prev->unlock();
			curr->unlock();
			delete new_node;
			return false;
		}
		else {
			new_node->value = x;
			new_node->next = curr;
			prev->next = new_node;
			prev->unlock();
			curr->unlock();
			return true;
		}
	}

	bool remove(int x)
	{
		NODE* prev = &head;
		prev->lock();
		NODE* curr = prev->next;
		curr->lock();
		while (curr->value < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value != x) {
			prev->unlock();
			curr->unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			curr->unlock();
			prev->unlock();
			delete curr;
			return true;
		}
	}

	bool contains(int x)
	{
		NODE* prev = &head;
		prev->lock();
		NODE* curr = prev->next;
		curr->lock();
		while (curr->value < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value != x) {
			prev->unlock();
			curr->unlock();
			return false;
		}
		else {
			prev->unlock();
			curr->unlock();
			return true;
		}
	}

	void print()
	{
		auto p = head.next;
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == &tail) break;
			p = p->next;
		}
		cout << endl;
	}
};

class OSET {
	NODE head, tail;
public:
	OSET()
	{
		cout << "낙천적 동기화 (Optimistic Synchronization)" << endl;
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}
	~OSET()
	{
		clear();
	}
	void clear()
	{
		auto p = head.next;
		while (p != &tail) {
			auto temp = p;
			p = p->next;
			delete temp;
		}
		head.next = &tail;
	}

	bool validate(NODE* prev, NODE* curr)
	{
		auto p = &head;
		while (p->value <= prev->value) {
			if (p == prev)
				return p->next == curr;
			p = p->next;
		}
		return false;
	}

	bool add(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->value < x) { prev = curr; curr = curr->next; }
			{
				lock_guard<mutex> ll(prev->nlock);
				lock_guard<mutex> ll2(curr->nlock);
				if (false == validate(prev, curr)) {
					continue;
				}
				if (curr->value == x) {
					return false;
				}
				else {
					auto new_node = new NODE;
					new_node->value = x;
					new_node->next = curr;
					prev->next = new_node;
					return true;
				}
			}
		}
	}

	bool remove(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->value < x) { prev = curr; curr = curr->next; }
			{
				lock_guard<mutex> ll(prev->nlock);
				lock_guard<mutex> ll2(curr->nlock);
				if (false == validate(prev, curr)) {
					continue;
				}
				if (curr->value != x) {
					return false;
				}
				else {
					prev->next = curr->next;
					//delete curr;
					return true;
				}
			}
		}
	}

	bool contains(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->value < x) { prev = curr; curr = curr->next; }
			{
				lock_guard<mutex> ll(prev->nlock);
				lock_guard<mutex> ll2(curr->nlock);
				if (false == validate(prev, curr)) {
					continue;
				}
				if (curr->value != x) {
					return false;
				}
				else {
					return true;
				}
			}
		}
	}

	void print()
	{
		auto p = head.next;
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == &tail) break;
			p = p->next;
		}
		cout << endl;
	}
};


constexpr int NUM_TEST = 4000000;
constexpr int KEY_RANGE = 1000;

OSET g_set;

void ThreadFunc(int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			g_set.add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			g_set.remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			g_set.contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}


int main()
{
	for (int num_threads = 1; num_threads <= MAX_TH; num_threads = num_threads * 2) {
		g_set.clear();
		vector <thread> threads;
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(ThreadFunc, num_threads);
		for (auto& th : threads) th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto msec = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		g_set.print();
		cout << num_threads << " threads, " << msec << "ms.\n";
	}
}
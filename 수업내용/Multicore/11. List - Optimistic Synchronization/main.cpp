#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
using namespace std;

struct Node {
	int value;
	Node* next;
	mutex nlock;

	void lock() { nlock.lock(); }
	void unlock() { nlock.unlock(); }
};

struct my_mutex {
	void lock() {}
	void unlock() {}
};

class Set {
public:
	Set() : head{ INT_MIN, &tail }, tail{ INT_MAX, nullptr } {}
	~Set()
	{
		//unsafe_clear();
	}
	void unsafe_clear()
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
		while (true) 
		{
			Node* prev = &head;
			Node* current = prev->next;
			while (current->value < x) 
			{
				prev = current;
				current = current->next;
			}
			lock_guard<mutex> prevLock(prev->nlock);
			lock_guard<mutex> currLock(current->nlock);
			if (!validate(prev, current)) continue;
			if (current->value == x) {
				return false;
			}
			else {
				Node* newNode = new Node;
				newNode->value = x;
				newNode->next = current;
				prev->next = newNode;
				return true;
			}
		}
	}
	bool remove(int x)
	{
		while (true) 
		{
			Node* prev = &head;
			Node* current = prev->next;
			while (current->value < x) 
			{
				prev = current;
				current = current->next;
			}
			lock_guard<mutex> prevLock(prev->nlock);
			lock_guard<mutex> currLock(current->nlock);
			if (!validate(prev, current)) {
				continue;
			}
			if (x == current->value) {
				prev->next = current->next;
				//delete current;
				return true;
			}
			else {
				return false;
			}
		}
	}
	bool contains(int x)
	{
		while (true) {
			Node* prev = &head;
			Node* current = prev->next;
			while (current->value < x) {
				prev = current;
				current = current->next;
			}
			lock_guard<mutex> prevLock(prev->nlock);
			lock_guard<mutex> currLock(current->nlock);
			if (!validate(prev, current)) continue;
			if (x == current->value) {
				return true;
			}
			else {
				return false;
			}
		}
	}
	void unsafe_print()
	{
		Node* p = head.next;
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == &tail) break;
			p = p->next;

		}
		cout << endl;
	}
private:
	bool validate(Node* prev, Node* curr)
	{
		auto p = &head;
		while (p->value <= prev->value) {
			if (p == prev) return p->next == curr;
			p = p->next;
		}
		return false;
	}

private:
	Node head;
	Node tail;
};

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;
Set set;

void Worker(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			set.add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			set.remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			set.contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	cout << "��õ�� ����ȭ (Optimistic Synchronization)" << endl;
	for (int i = 1; i <= 16; i *= 2) {
		set.unsafe_clear();
		vector<thread> v;
		auto t = chrono::high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			v.emplace_back(Worker, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << i << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		set.unsafe_print();
	}
}
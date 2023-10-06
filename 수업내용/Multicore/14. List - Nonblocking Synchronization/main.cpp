#include <iostream>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
using namespace std;

constexpr int MAX_THREADS = 32;
constexpr long long POINTER_MASK = 0xFFFFFFFFFFFFFFFF;

template <typename T>
class MarkableReference
{
public:
	MarkableReference(T* pointer) : address{ reinterpret_cast<void*>(pointer) }

	T* GetReference() const 
	{
		return reinterpret_cast<T*>(address & ~1);
	}
	bool GetMark() const 
	{
		return address & 1;
	}

	void SetReference(T* addr) 
	{
		address = reinterpret_cast<void*>(addr);
	}
	void SetMark(bool mark) 
	{
		if (mark) address = address | 1;
	}

	bool CAS(long long oldValue, long long newValue)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(&address), &oldValue, newValue);
	}
private:
	void* address;
};

struct Node {
	Node() : value{ -1 }, next{ nullptr } {}
	Node(int v) : value{ v }, next{ nullptr } {}

	Node* GetNext()
	{
		return next.GetReference();
	}
	Node* GetNext(bool* removed)
	{

	}
	bool IsRemoved()
	{
		return next.GetMark();
	}

	void SetRemove()
	{
		next.SetMark(true);
	}
	void SetNext(Node* p)
	{
		next.SetReference(p);
	}

	bool CAS(Node* oldNode, Node* newNode, bool oldMark, bool newMark)
	{
		long long oldValue = reinterpret_cast<long long>(oldNode);
		if (oldMark) oldValue = oldValue | 1;
		long long newValue = reinterpret_cast<long long>(newNode);
		if (newMark) newValue = newValue | 1;

		return next.CAS(oldValue, newValue);
	}

private:
	MarkableReference<Node> next;

public:
	int value;
};

struct my_mutex {
	void lock() {}
	void unlock() {}
};

class Set {
public:
	Set() : head{ INT_MIN }, tail{ INT_MAX }
	{
		head.SetNext(&tail);
	}
	~Set()
	{
		//unsafe_clear();
	}
	void unsafe_clear()
	{
		auto p = head.GetNext();
		while (p != &tail) {
			auto temp = p;
			p = p->GetNext();
			delete temp;
		}
		head.SetNext(&tail);
	}
	void find(int x, Node*& prev, Node*& current)
	{
		restart:
		prev = &head;
		current = prev->GetNext();
		while (true) {
			bool removed;
			Node* succ = current->GetNext(&removed);
			while (removed) {
				if (!prev->CAS(current, succ, false, false)) {
					goto restart;
				}
				current = succ;
				succ = current->GetNext(&removed);
			}
			if (current->value >= x) return;
			prev = current;
			current = current->GetNext();
		}
	}
	bool add(int x)
	{
		while (true)
		{
			Node* newNode = new Node(x);
			Node* prev;
			Node* current;
			find(x, prev, current);
			if (current->value == x) {
				delete newNode;
				return false;
			}
			else {
				newNode->SetNext(current);
				prev->CAS(current, newNode, false, false);
				return true;
			}
		}
	}
	bool remove(int x)
	{
		while (true)
		{
			Node* prev;
			Node* current;
			find(x, prev, current);
			if (x == current->value) {
				Node* succ = current->GetNext();
				current->SetRemove();
				if (!current->IsRemoved()) continue;
				
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
		Node* current = &head;
		while (current->value < x) current = current->next;
		return (current->value == x) && !(current->removed);
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
		return !(prev->removed) && !(curr->removed) && (prev->next == curr);
	}

private:
	Node head;
	Node tail;
};

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;
Set set;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

constexpr int RANGE = 1000;

void Worker(vector<HISTORY>* history, int num_threads)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_threads; i++) {
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

void CheckHistory(array<vector<HISTORY>, MAX_THREADS>& history, int num_threads)
{
	array<int, RANGE> survive = {};
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
	for (int i = 0; i < RANGE; ++i) {
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


void WorkerCheck(vector<HISTORY>* history, int num_threads)
{
	for (int i = 0; i < 4000000 / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % RANGE;
			history->emplace_back(0, v, set.add(v));
			break;
		}
		case 1: {
			int v = rand() % RANGE;
			history->emplace_back(1, v, set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % RANGE;
			history->emplace_back(2, v, set.contains(v));
			break;
		}
		}
	}
}

int main()
{
	cout << "스마트 포인터를 사용한 게으른 동기화 (Lazy Synchronization with shared_ptr)" << endl;
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector<thread> v;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.unsafe_clear();
		auto t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i) {
			v.emplace_back(WorkerCheck, &history[i], num_threads);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		set.unsafe_print();
		CheckHistory(history, num_threads);
	}

	cout << "======== SPEED CHECK =============" << endl;

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector<thread> v;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.unsafe_clear();
		auto t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i) {
			v.emplace_back(Worker, &history[i], num_threads);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		set.unsafe_print();
		CheckHistory(history, num_threads);
	}
}
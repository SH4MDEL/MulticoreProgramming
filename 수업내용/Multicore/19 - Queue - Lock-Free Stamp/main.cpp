#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <array>
using namespace std;

constexpr int MAX_THREADS = 32;
constexpr long long POINTER_MASK = 0xFFFFFFFFFFFFFFFF;

struct Node {
public:
	Node() : value{ -1 }, next{ nullptr } {}
	Node(int v) : value{ v }, next{ nullptr } {}

public:
	int value;
	Node* next;
};

class alignas(16) Stamp {
	Node* volatile node;
	int value;
public:
	Stamp() : node{ nullptr }, value{ 0 } {}
	Stamp(Node* ptr, int value) : node{ptr}, value{value}
	{
	
	}
	Node* GetNext() {
		if (!node) return nullptr;
		return node->next; 
	}
	int GetStamp() { return value; }
	Stamp& operator=(const Stamp& rhs) {
		long long t = *reinterpret_cast<long long*>(const_cast<Stamp*>(&rhs));
		return *reinterpret_cast<Stamp*>(&t);
	}
	bool operator!=(const Stamp& rhs) {
		return node != rhs.node || value != rhs.value;
	}
	void Set(Node* n, int v)
	{
		node = n;
		value = v;
	}
	void Set(Stamp s)
	{
		Node* n = s.GetNext();
		if (n) node = n;
		else node = nullptr;
		value = s.GetStamp();
	}
	void Clear()
	{
		node = nullptr;
	}
	void atomic_load(Stamp* t) {
		//long long t1 = *reinterpret_cast<long long*>(this);
		atomic_llong* a = reinterpret_cast<atomic_llong*>(this);
		long long t1 = *a;

		*t = *reinterpret_cast<Stamp*>(&t1);
	}
};

class Queue {
public:
	Queue()
	{
		auto n = new Node{ -1 };
		head.Set(n, 0);
		tail.Set(n, 0);
	}
	~Queue()
	{
		//unsafe_clear();
	}
	void enqueue(int x)
	{
		Node* node = new Node{ x };
		while (true) {
			Stamp last;
			//tail.atomic_load(&last);
			last.Set(tail);
			Node* next = last.GetNext();
			if (last != tail) continue;
			if (!next) {
				if (CAS(&last, nullptr, node, 0)) {
					CAS(&tail, last.GetNext(), node, last.GetStamp());
					return;
				}
			}
			else {
				CAS(&tail, last.GetNext(), next, last.GetStamp());
			}
		}
	}
	int dequeue()
	{
		while (true) {
			Stamp first;
			//head.atomic_load(&first);
			first.Set(head);
			Stamp last;
			//tail.atomic_load(&last);
			last.Set(tail);
			Node* next = first.GetNext();
			if (first.GetNext() != head.GetNext()) continue;
			if (!next) return -1;
			if (first.GetNext() == last.GetNext()) {
				CAS(&tail, last.GetNext(), next, last.GetStamp());
				continue;
			}
			int value = next->value;
			if (!CAS(&head, first.GetNext(), next, first.GetStamp())) continue;
			delete first.GetNext();
			return value;
		}
	}
	void unsafe_print()
	{
		Node* p = head.GetNext();
		for (int i = 0; i < 20; ++i) {
			if (!p) break;
			cout << p->value << " ";
			p = p->next;
		}
		cout << endl;
	}
	void unsafe_clear()
	{
		Node* p = head.GetNext();
		while (p != nullptr) {
			Node* t = p;
			p = p->next;
			delete t;
		}
		head = tail;
		head.Clear();
	}
private:
	bool CAS(Stamp* sptr, Node* oldPtr, Node* newPtr, int stamp)
	{
		Stamp oldStp{ oldPtr, stamp };
		Stamp newStp{ newPtr, stamp + 1 };
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong*>(sptr),
			reinterpret_cast<long long*>(&oldStp),
			*reinterpret_cast<long long*>(&newStp));
	}

private:
	Stamp head;
	Stamp tail;
};

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;
Queue queue;

void Worker(int num_threads, int threadID)
{
	for (int i = 0; i < NUM_TEST / num_threads; i++) {
		if ((rand() % 2) || i < 32 / num_threads) {
			queue.enqueue(i);
		}
		else {
			//queue.dequeue();
		}
	}
}

int main()
{
	cout << "큐 : 무제한 무잠금 동기화 (Queue : Lock-Free Stamp Synchronization) 시도횟수 : " << NUM_TEST << endl;
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector<thread> v;
		queue.unsafe_clear();
		auto t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i) {
			v.emplace_back(Worker, num_threads, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		queue.unsafe_print();
	}
}
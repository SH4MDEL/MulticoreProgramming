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

class alignas(8) Stamp {
	Node* volatile next;
	int value;
public:
	Stamp() : next{ nullptr }, value{ 0 } {}
	Stamp(Node* ptr, int value) : next{ptr}, value{value} {}
	Node* GetNext() { return next; }
	int GetStamp() { return value; }
	Stamp operator=(Stamp& rhs) {
		long long t = *reinterpret_cast<long long*>(&rhs);
		return *reinterpret_cast<Stamp*>(&t);
	}
	bool CAS(Node* oldPtr, Node* newPtr, int stamp)
	{

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
		head = tail = Stamp{ new Node{-1}, 0 };
	}
	~Queue()
	{
		//unsafe_clear();
	}
	void enqueue(int x)
	{
		Node* node = new Node{ x };
		while (true) {
			Stamp* last = &tail;
			Node* next = last->GetNext();
			if (last != tail) continue;
			if (!next) {
				if (CAS(&(last->next), nullptr, node)) {
					CAS(&tail, last, node);
					return;
				}
			}
			else {
				CAS(&tail, last, next);
			}
		}
	}
	int dequeue()
	{
		while (true) {
			Stamp first;
			head.atomic_load(&first);
			Stamp last;
			tail.atomic_load(&last);
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
		Node* p = head.GetNext()->next;
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
		while (p != tail.GetNext()) {
			Node* t = p;
			p = p->next;
			delete t;
		}
		head = tail;
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
			queue.dequeue();
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
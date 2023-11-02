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

class Queue {
public:
	Queue()
	{
		head = tail = new Node{ -1 };
	}
	~Queue()
	{
		//unsafe_clear();
	}
	void enqueue(int x)
	{
		Node* node = new Node{ x };
		while (true) {
			Node* last = tail;
			Node* next = last->next;
			if (last != tail) continue;	// 다른 작업이 이뤄지지 않았는지 검사
			if (!next) {				// tail->next가 nullptr이면?
				if (CAS(&(last->next), nullptr, node)) {	// CAS를 통해 새 노드 끼워넣음
					CAS(&tail, last, node);					// tail이 last 아직 맞으면 노드로 바꿈
					return;
				}
			}
			else {						// 누군가 tail->next에 뭘 끼워넣었다?
				CAS(&tail, last, next);	// tail이 last 아직 맞으면 last->next로 바꿈
			}
		}
	}
	int dequeue()
	{
		while (true) {
			Node* first = head;
			Node* last = tail;
			Node* next = first->next;
			if (first != head) continue;
			if (!next) return -1;
			if (first == next) {
				CAS(&tail, last, next);
				continue;
			}
			int value = next->value;
			if (!CAS(&head, first, next)) continue;
			//delete first;
			return value;
		}
	}
	void unsafe_print()
	{
		Node* p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (!p) break;
			cout << p->value << " ";
			p = p->next;
		}
		cout << endl;
	}
	void unsafe_clear()
	{
		Node* p = head->next;
		while (p) {
			if (p == tail) break;
			Node* t = p;
			p = p->next;
			delete t;
		}
		tail = head;
		head->next = nullptr;
	}
private:
	bool CAS(Node* volatile* ptr, Node* oldPtr, Node* newPtr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(ptr),
			reinterpret_cast<long long*>(&oldPtr),
			reinterpret_cast<long long>(newPtr));
	}

private:
	Node* volatile head;
	Node* volatile tail;
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
	cout << "큐 : 무잠금 동기화 (Queue : Lock-Free Synchronization) 시도횟수 : " << NUM_TEST << endl;
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
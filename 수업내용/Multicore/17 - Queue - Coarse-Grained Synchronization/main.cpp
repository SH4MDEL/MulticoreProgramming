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

private:


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
		enqueueLock.lock();
		tail->next = node;
		tail = node;
		enqueueLock.unlock();
	}
	int dequeue()
	{
		dequeueLock.lock();
		if (!head->next) {
			dequeueLock.unlock();
			return -1;
		}
		int res = head->next->value;	// 멀티쓰레드에서는 다른 쓰레드가 head를 바꿀 수 있다.
		Node* t = head;					// return은 locking에서 벗어나야 하기 때문이다.
		head = head->next;				
		delete t;
		dequeueLock.unlock();
		return res;
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
			Node* t = p;
			p = p->next;
			delete t;
		}
		tail = head;
		head->next = nullptr;
	}

private:
	Node* volatile head;
	Node* volatile tail;
	mutex enqueueLock, dequeueLock;
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
	cout << "큐 : 성긴 동기화 (Queue : Coarse-Grained Synchronization) 시도횟수 : " << NUM_TEST << endl;
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
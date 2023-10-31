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
	Node* volatile next;
};

class Stack {
public:
	Stack() : top{ nullptr } {}
	~Stack()
	{
	}
	void push(int x)
	{
		Node* node = new Node{ x };
		while (true) {
			Node* tmp = top;
			node->next = tmp;
			if (CAS(node->next, node)) return;
		}
	}
	int pop()
	{
		while (true) {
			Node* ptr = top;
			if (!ptr) { return -2; }
			Node* next = ptr->next;
			int temp = ptr->value;
			if (CAS(ptr, next)) {
				return temp;
			}
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

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;
Stack stack;

constexpr int RANGE = 10000000;

void Worker(int num_threads, int threadID)
{
	for (int i = 0; i < 10000000 / num_threads; i++) {
		if ((rand() % 2) || i < 32 / num_threads) {
			stack.push(i);
		}
		else {
			stack.pop();
		}
	}
}

int main()
{
	cout << "스택 : 무잠금 동기화 (Stack : Lock-Free Synchronization)" << endl;
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector<thread> v;
		stack.unsafe_clear();
		auto t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i) {
			v.emplace_back(Worker, num_threads, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		stack.unsafe_print();
	}
}
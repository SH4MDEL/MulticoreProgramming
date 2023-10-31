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
	Stack() : top{nullptr} {}
	~Stack()
	{
		//unsafe_clear();
	}
	void push(int x)
	{
		Node* e = new Node{ x };
		lock_guard<mutex> l(m);
		e->next = top;
		top = e;
	}
	int pop()
	{
		m.lock();
		if (!top) { m.unlock(); return -2; }
		int temp = top->value;
		Node* ptr = top;
		top = top->next;
		m.unlock();
		delete ptr;
		return temp;
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
	Node* volatile top;
	mutex m;
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
	cout << "스택 : 성긴 동기화 (Stack : Coarse-Grained Synchronization)" << endl;
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
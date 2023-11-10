#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
using namespace std;

constexpr int MAX_LEVEL = 9;

template <typename T>
struct Node {
	T value;
	Node* volatile next[MAX_LEVEL + 1];
	int topLevel;

	Node() : topLevel{0} 
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
public:
	SkipList(T minNum, T maxNum)
	{
		head.value = minNum;
		tail.value = maxNum;
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	~SkipList()
	{
		//clear();
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
		if (current[0]->value == x) {
			m.unlock();
			return false;
		}
		for (int i = current[0]->topLevel; i >= 0; --i) {
			prev[i]->next[i] = current[i]->next[i];
		}
		m.unlock();
		return true;
	}
	void find(T x, Node<T>* prev[], Node<T>* current[])
	{
		prev[MAX_LEVEL] = &head;
		for (int cl = MAX_LEVEL; cl >= 0; --cl) {
			if (cl != MAX_LEVEL) 
				prev[cl] = prev[cl + 1];
			current[cl] = prev[cl]->next[cl];
			while (current[cl]->value < x) {
				prev[cl] = current[cl];
				current[cl] = current[cl]->next[cl];
			}
		}
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
private:
	Node<T> head, tail;
	mutex m;
};

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;
SkipList<int> set{INT_MIN, INT_MAX};

void Worker(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 2) {
		case 0: key = rand() % KEY_RANGE;
			set.add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			set.remove(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	cout << "SkipList : 성긴 동기화 (SkipList : Coarse-Grained Synchronization)" << endl;
	for (int i = 1; i <= 8; i *= 2) {
		set.clear();
		vector<thread> v;
		auto t = chrono::high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			v.emplace_back(Worker, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << i << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		set.print();
	}
}
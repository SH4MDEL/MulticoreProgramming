#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <array>
#include <chrono>
using namespace std;

constexpr int MAX_LEVEL = 9;
constexpr int MAX_THREADS = 32;

template <typename T>
struct Node {
private:
	recursive_mutex m;
public:
	T value;
	Node<T>* volatile next[MAX_LEVEL + 1];
	int topLevel;
	volatile bool removed;
	volatile bool fullyLinked;

	Node() : value{}, topLevel{ 0 }, removed{ false }, fullyLinked{ false }
	{
		for (auto& n : next) n = nullptr;
	}
	Node(T t, int top) : value{ t }, topLevel{ top }, removed{ false }, fullyLinked{ false }
	{
		for (auto& n : next) n = nullptr;
	}
	void lock()
	{
		m.lock();
	}
	void unlock()
	{
		m.unlock();
	}
};

template <typename T>
class SkipList {
	Node<T> head, tail;
public:
	SkipList(T minNum, T maxNum)
	{
		head.value = minNum;
		tail.value = maxNum;
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	//~SkipList() { clear(); }
	int find(T x, Node<T>* prev[], Node<T>* curr[])
	{
		int findLevel = -1;
		prev[MAX_LEVEL] = &head;
		for (int cl = MAX_LEVEL; cl >= 0; --cl) {
			if (cl != MAX_LEVEL) prev[cl] = prev[cl + 1];
			curr[cl] = prev[cl]->next[cl];
			while (curr[cl]->value < x) {
				prev[cl] = curr[cl];
				curr[cl] = curr[cl]->next[cl];
			}
			if (curr[cl]->value == x && findLevel == -1) findLevel = cl;
		}
		return findLevel;
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
		Node<T>* curr[MAX_LEVEL + 1];

		int topLevel = 0;
		while (rand() % 2 == 1) {
			++topLevel;
			if (topLevel == MAX_LEVEL) break;
		}

		while (true) {
			int findLevel = find(x, prev, curr);

			if (findLevel != -1) {
				Node<T>* nodeFound = curr[findLevel];
				if (!nodeFound->removed) {
					while (!nodeFound->fullyLinked) {}
					return false;
				}
				continue;
			}
			int highestLocked = -1;
			bool valid = true;
			for (int level = 0; valid && (level <= topLevel); ++level) {
				Node<T>* previous = prev[level];
				Node<T>* current = curr[level];
				previous->lock();
				highestLocked = level;
				valid = (!previous->removed && !current->removed && previous->next[level] == current);
			}
			if (!valid) {
				for (int i = 0; i <= highestLocked; ++i) {
					prev[i]->unlock();
				}
				continue;
			}

			Node<T>* newNode = new Node<T>{ x, topLevel };
			for (int i = 0; i <= topLevel; ++i) {
				newNode->next[i] = curr[i];
			}
			for (int i = 0; i <= topLevel; ++i) {
				prev[i]->next[i] = newNode;
			}
			newNode->fullyLinked = true;
			for (int i = 0; i <= highestLocked; ++i) {
				prev[i]->unlock();
			}
			return true;
		}
	}
	bool remove(T x)
	{
		Node<T>* prev[MAX_LEVEL + 1];
		Node<T>* curr[MAX_LEVEL + 1];
		Node<T>* victim;

		bool removed = false;
		int topLevel = -1;
		while (true) {
			int findLevel = find(x, prev, curr);
			if (findLevel != -1) victim = curr[findLevel];
			else return false;
			if (removed || (findLevel != -1 && 
				victim->fullyLinked && victim->topLevel == findLevel 
				&& !victim->removed)) {
				if (!removed) {
					topLevel = victim->topLevel;
					victim->lock();
					if (victim->removed) {
						victim->unlock();
						return false;
					}
					victim->removed = true;
					removed = true;
				}
				int highestLocked = -1;
				while (true) {
					bool valid = true;
					for (int i = 0; valid && i <= topLevel; i++) {
						auto previous = prev[i];
						previous->lock();
						highestLocked = i;
						valid = !previous->removed && previous->next[i] == victim;
					}
					if (!valid) {
						for (int i = 0; i <= highestLocked; ++i) {
							prev[i]->unlock();
						}
						victim->unlock();
						continue;
					}
					for (int i = topLevel; 0 <= i; i--)
						prev[i]->next[i] = victim->next[i];
					for (int i = 0; i <= highestLocked; ++i) {
						prev[i]->unlock();
					}
					victim->unlock();
					return true;
				}
			}
			return false;
		}

		//Node<T>* prev[MAX_LEVEL + 1];
		//Node<T>* curr[MAX_LEVEL + 1];
		//int findLevel = find(x, prev, curr);

		//if (findLevel == -1) return false;

		//Node<T>* victim = curr[findLevel];
		//if (!victim->fullyLinked) return false;
		//if (victim->removed) return false;
		//if (victim->topLevel != findLevel) return false;

		//// 지우려면 prev, curr이 제대로 locking 된 다음 지워야 함.
		//victim->lock();
		//if (victim->removed) {
		//	victim->unlock();
		//	return false;
		//}
		//victim->removed = true;

		//// 링크 정리
		//int topLevel = victim->topLevel;
		//for (int i = 0; i <= topLevel; ++i) {
		//	prev[i]->lock();
		//	if (prev[i]->removed || prev[i]->next[i] != victim) {
		//		// 언락 후 다시 검색. 다시 시도
		//		for (int j = i; j <= topLevel; ++j) {
		//			prev[j]->unlock();
		//		}
		//		find(x, prev, curr);
		//		i = topLevel;
		//		continue;
		//	}
		//}
		//for (int i = 0; i <= topLevel; ++i) {
		//	prev[i]->next[i] = victim->next[i];
		//}
		//for (int i = 0; i <= topLevel; ++i) {
		//	prev[i]->unlock();
		//}
		//victim->unlock();
		//return true;
	}
	bool contains(T x)
	{
		Node<T>* prev[MAX_LEVEL + 1];
		Node<T>* curr[MAX_LEVEL + 1];

		int findLevel = find(x, prev, curr);
		return (findLevel != -1) &&
			(!curr[findLevel]->removed) &&
			(curr[findLevel]->fullyLinked);
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
};

constexpr auto LOOP = 4000000;
constexpr auto KEY_RANGE = 1000;

thread_local int tls_thid;

int Thread_id()
{
	return tls_thid;
}

SkipList<int> set{ INT_MIN, INT_MAX };

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void worker(vector<HISTORY>* history, int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < LOOP / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			set.add(v);
			break;
		}
		case 1: {
			int v = rand() % KEY_RANGE;
			set.remove(v);
			break;
		}
		case 2: {
			int v = rand() % KEY_RANGE;
			set.contains(v);
			break;
		}
		}
	}
}

void worker_check(vector<HISTORY>* history, int num_threads, int th_id)
{
	tls_thid = th_id;
	for (int i = 0; i < LOOP / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(0, v, set.add(v));
			break;
		}
		case 1: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(1, v, set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % KEY_RANGE;
			history->emplace_back(2, v, set.contains(v));
			break;
		}
		}
	}
}

void check_history(array <vector <HISTORY>, MAX_THREADS>& history, int num_threads)
{
	array<int, KEY_RANGE> survive = {};
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
	for (int i = 0; i < KEY_RANGE; ++i) {
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

int main()
{
	cout << "SkipList : 게으른 동기화 (SkipList : Lazy Synchronization)" << endl;
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.clear();
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		set.print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}

	cout << "======== SPEED CHECK =============\n";

	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, MAX_THREADS> history;
		set.clear();
		auto start_t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();
		set.print();
		cout << num_threads << " Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}
}
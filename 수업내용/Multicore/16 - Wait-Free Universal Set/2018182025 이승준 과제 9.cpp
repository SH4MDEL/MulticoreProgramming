#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <set>
#include <array>
using namespace std;

constexpr int MAX_THREADS = 32;

enum METHOD_TYPE { MT_ADD, MT_REMOVE, MT_CONTAINS, MT_CLEAR, MT_GET20 };

struct Invocation {
	METHOD_TYPE mt;
	int v;

};

struct Response {
	bool success;
	vector<int> res20;
};

struct Node;
class Consensus {
public:
	Consensus() : next{ nullptr } {}
	Node* decide(Node* p)
	{
		Node* old = nullptr;
		atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong*>(&next),
			reinterpret_cast<long long*>(&old),
			reinterpret_cast<long long>(p)
		);
		return next;
	}
	void clear()
	{
		next = nullptr;
	}
private:
	Node* next;
};

struct Node {
	Invocation inv;
	Node* volatile next;
	volatile int seq;
	Consensus decideNext;
	Node() : next{ nullptr }, seq{ 0 } {}
	Node(Invocation inv) : inv{ inv }, next{ nullptr }, seq{ 0 } {}
	void clear()
	{
		next = nullptr;
		decideNext.clear();
	}
};

thread_local int threadid;
int GetThreadID()
{
	return threadid;
}

class SeqObject_set {
public:
	Response apply(const Invocation& inv) {
		Response res;
		switch (inv.mt) {
		case MT_ADD:
			if (m_set.count(inv.v)) {
				res.success = false;
			}
			else {
				m_set.insert(inv.v);
				res.success = true;
			}
			break;
		case MT_REMOVE:
			if (!m_set.count(inv.v)) {
				res.success = false;
			}
			else {
				m_set.erase(inv.v);
				res.success = true;
			}
			break;
		case MT_CONTAINS:
			res.success = m_set.count(inv.v);
			break;
		case MT_CLEAR:
			m_set.clear();
			break;
		case MT_GET20:
			for (int i = 0; const auto & x : m_set) {
				res.res20.push_back(x);
				if (++i == 20) break;
			}
			break;
		}
		return res;
	}

private:
	set<int> m_set;
};

class UnivObject_set {
private:
	Node* announce[MAX_THREADS];
	Node* head[MAX_THREADS];	// 각 쓰레드가 보고 있는 최신 노드?
	Node tail;
public:
	UnivObject_set() {
		tail.seq = 1;
		for (int i = 0; i < MAX_THREADS; ++i) announce[i] = &tail;
		for (int i = 0; i < MAX_THREADS; ++i) head[i] = &tail;
	}
	Node* GetMaxNode() {
		Node* p = head[0];
		for (auto h : head) {
			if (h->seq > p->seq) {
				p = h;
			}
		}
		return p;
	}

	Response apply(Invocation invoc) {
		int i = GetThreadID();
		announce[i] = new Node(invoc);
		head[i] = GetMaxNode();
		while (!announce[i]->seq) {
			Node* before = head[i];
			Node* help = announce[((before->seq + 1) % MAX_THREADS)];
			Node* prefer;
			if (!help->seq) prefer = help;
			else prefer = announce[i];
			Node* after = before->decideNext.decide(prefer);
			before->next = after;
			after->seq = before->seq + 1;
			head[i] = after;
		}
		SeqObject_set myObject;
		Node* current = tail.next;
		while (current != announce[i]) {
			myObject.apply(current->inv);
			current = current->next;
		}
		head[i] = announce[i];
		return myObject.apply(current->inv);
	}

	void clear()
	{
		Node* p = tail.next;
		while (p != nullptr) {
			Node* t = p;
			p = p->next;
			delete t;
		}
		for (int i = 0; i < MAX_THREADS; ++i) announce[i] = &tail;
		for (int i = 0; i < MAX_THREADS; ++i) head[i] = &tail;
		tail.clear();
	}
};


class Set {
public:
	Set() = default;
	~Set() = default;
	void clear()
	{
		//Invocation inv{ MT_CLEAR, -1 };
		//m_set.apply(inv);
		m_set.clear();
	}
	bool add(int x)
	{
		Invocation inv{ MT_ADD, x };
		Response res = m_set.apply(inv);
		return res.success;
	}
	bool remove(int x)
	{
		Invocation inv{ MT_REMOVE, x };
		Response res = m_set.apply(inv);
		return res.success;
	}
	bool contains(int x)
	{
		Invocation inv{ MT_CONTAINS, x };
		Response res = m_set.apply(inv);
		return res.success;
	}
	void print()
	{
		Invocation inv{ MT_GET20, 0 };
		Response res = m_set.apply(inv);
		for (const auto& x : res.res20) {
			cout << x << ", ";
		}
		cout << endl;
	}
private:
	UnivObject_set m_set;
};

class MutexSet {
public:
	MutexSet() = default;
	~MutexSet() = default;
	void clear()
	{
		m_set.clear();
	}
	bool add(int x)
	{
		m_set.insert(x);
		return true;
	}
	bool remove(int x)
	{
		m_set.erase(x);
		return true;
	}
	bool contains(int x)
	{
		return m_set.contains(x);
	}
	void print()
	{
		for (int i = 0; const auto & x : m_set) {
			cout << x << ", ";
			if (++i == 20) break;
		}
		cout << endl;
	}
private:
	set<int> m_set;
};

constexpr auto NUM_TEST = 40000;
constexpr auto KEY_RANGE = 1000;
constexpr int RANGE = 1000;
MutexSet g_set;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void Worker(vector<HISTORY>* history, int num_threads, int tid)
{
	threadid = tid;
	int key;
	for (int i = 0; i < NUM_TEST / num_threads; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			g_set.add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			g_set.remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			g_set.contains(key);
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
			if (g_set.contains(i)) {
				cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == g_set.contains(i)) {
				cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	cout << " OK\n";
}


void WorkerCheck(vector<HISTORY>* history, int num_threads, int tid)
{
	threadid = tid;
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % RANGE;
			history->emplace_back(0, v, g_set.add(v));
			break;
		}
		case 1: {
			int v = rand() % RANGE;
			history->emplace_back(1, v, g_set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % RANGE;
			history->emplace_back(2, v, g_set.contains(v));
			break;
		}
		}
	}
}

int main()
{
	cout << "single thread set" << endl;
	//for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
	//	vector<thread> v;
	//	array<vector <HISTORY>, MAX_THREADS> history;
	//	g_set.clear();
	//	auto t = chrono::high_resolution_clock::now();
	//	for (int i = 0; i < num_threads; ++i) {
	//		v.emplace_back(WorkerCheck, &history[i], num_threads, i);
	//	}
	//	for (auto& t : v) t.join();
	//	auto d = chrono::high_resolution_clock::now() - t;

	//	cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
	//		<< " millisec" << endl;
	//	g_set.print();
	//	CheckHistory(history, num_threads);
	//}

	//cout << "======== SPEED CHECK =============" << endl;

	for (int num_threads = 1; num_threads <= 1; num_threads *= 2) {
		vector<thread> v;
		array<vector <HISTORY>, MAX_THREADS> history;
		g_set.clear();
		auto t = chrono::high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i) {
			v.emplace_back(Worker, &history[i], num_threads, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << num_threads << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec" << endl;
		g_set.print();
		CheckHistory(history, num_threads);
	}
}
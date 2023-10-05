#include <iostream>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
using namespace std;

constexpr int MAX_THREADS = 32;

namespace JNH {
	using namespace std;

	template <class T>
	struct atomic_shared_ptr {
	private:
		mutable mutex m_lock;
		shared_ptr<T> m_ptr;
	public:
		bool is_lock_free() const noexcept
		{
			return false;
		}

		void store(shared_ptr<T> sptr, memory_order = memory_order_seq_cst) noexcept
		{
			m_lock.lock();
			m_ptr = sptr;
			m_lock.unlock();
		}

		shared_ptr<T> load(memory_order = memory_order_seq_cst) const noexcept
		{
			m_lock.lock();
			shared_ptr<T> t = m_ptr;
			m_lock.unlock();
			return t;
		}

		operator shared_ptr<T>() const noexcept
		{
			m_lock.lock();
			shared_ptr<T> t = m_ptr;
			m_lock.unlock();
			return t;
		}

		shared_ptr<T> exchange(shared_ptr<T> sptr, memory_order = memory_order_seq_cst) noexcept
		{
			m_lock.lock();
			shared_ptr<T> t = m_ptr;
			m_ptr = sptr;
			m_lock.unlock();
			return t;
		}

		bool compare_exchange_strong(shared_ptr<T>& expected_sptr, shared_ptr<T> new_sptr, memory_order, memory_order) noexcept
		{
			bool success = false;
			m_lock.lock();
			shared_ptr<T> t = m_ptr;
			if (m_ptr.get() == expected_sptr.get()) {
				m_ptr = new_sptr;
				success = true;
			}
			expected_sptr = m_ptr;
			m_lock.unlock();
		}

		bool compare_exchange_weak(shared_ptr<T>& expected_sptr, shared_ptr<T> target_sptr, memory_order, memory_order) noexcept
		{
			return compare_exchange_strong(expected_sptr, target_sptr, memory_order);
		}

		atomic_shared_ptr() noexcept = default;

		constexpr atomic_shared_ptr(shared_ptr<T> sptr) noexcept
		{
			m_lock.lock();
			m_ptr = sptr;
			m_lock.unlock();
		}

		shared_ptr<T> operator=(shared_ptr<T> sptr) noexcept
		{
			m_lock.lock();
			m_ptr = sptr;
			m_lock.unlock();
			return sptr;
		}

		void reset()
		{
			m_lock.lock();
			m_ptr = nullptr;
			m_lock.unlock();
		}

		atomic_shared_ptr(const atomic_shared_ptr& rhs)
		{
			store(rhs);
		}

		atomic_shared_ptr& operator=(const atomic_shared_ptr& rhs)
		{
			store(rhs);
			return *this;
		}

		shared_ptr<T>& operator->() {
			std::lock_guard<mutex> tt(m_lock);
			return m_ptr;
		}

		template< typename TargetType >
		inline bool operator ==(shared_ptr< TargetType > const& rhs)
		{
			return load() == rhs;
		}

		template< typename TargetType >
		inline bool operator ==(atomic_shared_ptr<TargetType> const& rhs)
		{
			std::lock_guard<mutex> t1(m_lock);
			std::lock_guard<mutex> t2(rhs.m_lock);
			return m_ptr == rhs.m_ptr;
		}
	};

	template <class T> struct atomic_weak_ptr {
	private:
		mutable mutex m_lock;
		weak_ptr<T> m_ptr;
	public:
		bool is_lock_free() const noexcept
		{
			return false;
		}
		void store(weak_ptr<T> wptr, memory_order = memory_order_seq_cst) noexcept
		{
			m_lock.lock();
			m_ptr = wptr;
			m_lock.unlock();
		}
		weak_ptr<T> load(memory_order = memory_order_seq_cst) const noexcept
		{
			m_lock.lock();
			weak_ptr<T> t = m_ptr;
			m_lock.lock();
			return t;
		}
		operator weak_ptr<T>() const noexcept
		{
			m_lock.lock();
			weak_ptr<T> t = m_ptr;
			m_lock.unlock();
			return t;
		}
		weak_ptr<T> exchange(weak_ptr<T> wptr, memory_order = memory_order_seq_cst) noexcept
		{
			m_lock.lock();
			weak_ptr<T> t = m_ptr;
			m_ptr = wptr;
			m_lock.unlock();
			return t;
		}

		bool compare_exchange_strong(weak_ptr<T>& expected_wptr, weak_ptr<T> new_wptr, memory_order, memory_order) noexcept
		{
			bool success = false;
			lock_guard(m_lock);

			weak_ptr<T> t = m_ptr;
			shared_ptr<T> my_ptr = t.lock();
			if (!my_ptr) return false;
			shared_ptr<T> expected_sptr = expected_wptr.lock();
			if (!expected_wptr) return false;

			if (my_ptr.get() == expected_sptr.get()) {
				success = true;
				m_ptr = new_wptr;
			}
			expected_wptr = t;
			return success;
		}

		bool compare_exchange_weak(weak_ptr<T>& exptected_wptr, weak_ptr<T> new_wptr, memory_order, memory_order) noexcept
		{
			return compare_exchange_strong(exptected_wptr, new_wptr, memory_order);
		}

		atomic_weak_ptr() noexcept = default;

		constexpr atomic_weak_ptr(weak_ptr<T> wptr) noexcept
		{
			m_lock.lock();
			m_ptr = wptr;
			m_lock.unlock();
		}

		atomic_weak_ptr(const atomic_weak_ptr&) = delete;
		atomic_weak_ptr& operator=(const atomic_weak_ptr&) = delete;
		weak_ptr<T> operator=(weak_ptr<T> wptr) noexcept
		{
			m_lock.lock();
			m_ptr = wptr;
			m_lock.unlock();
			return wptr;
		}
		shared_ptr<T> lock() const noexcept
		{
			m_lock.lock();
			shared_ptr<T> sptr = m_ptr.lock();
			m_lock.unlock();
			return sptr;
		}
		void reset()
		{
			m_lock.lock();
			m_ptr.reset();
			m_lock.unlock();
		}
	};
}

struct Node {
	Node() : value{ -1 }, next{ nullptr }, removed{ false } {}
	Node(int v) : value{ v }, next{ nullptr }, removed{ false } {}
	Node(int v, JNH::atomic_shared_ptr<Node> node) : value{ v }, next{ node }, removed{ false } {}

	int value;
	JNH::atomic_shared_ptr<Node> next;
	volatile bool removed;
	mutex nlock;

	void lock() { nlock.lock(); }
	void unlock() { nlock.unlock(); }
};

struct my_mutex {
	void lock() {}
	void unlock() {}
};

class Set {
public:
	Set()
	{
		head = make_shared<Node>(INT_MIN, tail);
		tail = make_shared<Node>(INT_MAX, head);
	}
	~Set()
	{
		//unsafe_clear();
	}
	void unsafe_clear()
	{
		head->next = tail;
	}
	bool add(int x)
	{
		while (true)
		{
			shared_ptr<Node> prev = head;
			shared_ptr<Node> current = prev->next;
			while (current->value < x)
			{
				prev = current;
				current = current->next;
			}
			lock_guard<mutex> prevLock(prev->nlock);
			lock_guard<mutex> currLock(current->nlock);
			if (!validate(prev, current)) continue;
			if (current->value == x) {
				return false;
			}
			else {
				auto newNode = make_shared<Node>(x, current);
				prev->next = newNode;
				return true;
			}
		}
	}
	bool remove(int x)
	{
		while (true)
		{
			shared_ptr<Node> prev = head;
			shared_ptr<Node> current = prev->next;
			while (current->value < x)
			{
				prev = current;
				current = current->next;
			}
			lock_guard<mutex> prevLock(prev->nlock);
			lock_guard<mutex> currLock(current->nlock);
			if (!validate(prev, current)) {
				continue;
			}
			if (x == current->value) {
				current->removed = true;
				prev->next = current->next;
				return true;
			}
			else {
				return false;
			}
		}
	}
	bool contains(int x)
	{
		shared_ptr<Node> current = head;
		while (current->value < x) current = current->next;
		return (current->value == x) && !(current->removed);
	}
	void unsafe_print()
	{
		auto p = head->next;
		for (int i = 0; i < 20; ++i) {
			cout << p->value << ", ";
			if (p == tail) break;
			p = p->next;

		}
		cout << endl;
	}
private:
	bool validate(const shared_ptr<Node>& prev, const shared_ptr<Node>& curr)
	{
		return !(prev->removed) && !(curr->removed) && (prev->next == curr);
	}

private:
	JNH::atomic_shared_ptr<Node> head;
	JNH::atomic_shared_ptr<Node> tail;
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
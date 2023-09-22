#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
using namespace std;

volatile int sum;
volatile int LOCK;

bool CAS(volatile int* addr, int expected, int update)
{
	return atomic_compare_exchange_strong(
		reinterpret_cast<volatile atomic_int*>(addr), &expected, update);
}

void CAS_LOCK()
{
	while (!CAS(&LOCK, 0, 1)) { this_thread::yield(); }
}

void CAS_UNLOCK()
{
	LOCK = 0;
}

void worker(int num_threads)
{
	const int loop_count = 50000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) {
		CAS_LOCK();
		sum = sum + 2;
		CAS_UNLOCK();
	}
}

int main()
{

	for (int i = 1; i <= 8; i *= 2) {
		sum = 0;
		vector<thread> v;
		auto t = chrono::high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			v.emplace_back(worker, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << i << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count()
			<< " millisec, ans : " << sum << endl;
	}

}
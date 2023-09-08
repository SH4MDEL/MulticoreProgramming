#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
using namespace std;

class SpinLock {
public:
	void lock()
	{
		bool expected = false;
		while (!locked.compare_exchange_strong(expected, true)) {}
	}
	void unlock()
	{
		locked.store(false);
	}
private:
	atomic_bool locked;
};

volatile atomic_int sum;
SpinLock m;

void func(int n)
{
	volatile int s = 0;
	for (auto i = 0; i < (50'000'000 / n); ++i) {
		s += 2;
	}
	sum.fetch_add(s, std::memory_order_relaxed);
}

int main()
{
	for (int i = 1; i < 64; i *= 2) {
		sum = 0;
		vector<thread> v;
		auto t = chrono::high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			v.emplace_back(func, i);
		}
		for (auto& t : v) t.join();
		auto d = chrono::high_resolution_clock::now() - t;

		cout << i << " threads : " << chrono::duration_cast<chrono::milliseconds>(d).count() 
			<< " millisec, ans : " << sum << endl;
	}

}
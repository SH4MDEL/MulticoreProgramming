#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
using namespace std;

atomic_int victim = 0;
volatile bool flag[2] = { false, false };

void Lock(int myID) {
	int other = 1 - myID;
	flag[myID] = true;

	victim = myID;

	while (flag[other] && victim == myID) {}
}

void Unlock(int myID) {
	flag[myID] = false;
}

volatile int sum;

void Worker(int id)
{
	for (int i = 0; i < 25'000'000; ++i) {
		Lock(id);
		sum += 1;
		Unlock(id);
	}
}

int main()
{
	auto t = chrono::high_resolution_clock::now();
	thread t1(Worker, 0);
	thread t2(Worker, 1);

	t1.join(); t2.join();
	auto d = chrono::high_resolution_clock::now() - t;

	cout << chrono::duration_cast<chrono::milliseconds>(d).count()
		<< " millisec, ans : " << sum << endl;

	// atomic_int victim = 0;
	// atomic_bool flag[2] = { false, false };
	// 약 4000 ms
	// 적절한 위치에 atomic_thread_fence를 넣은 것보다 느리다.

	// atomic_int victim = 0;
	// volatile bool flag[2] = { false, false };
	// 약 3300 ms

}

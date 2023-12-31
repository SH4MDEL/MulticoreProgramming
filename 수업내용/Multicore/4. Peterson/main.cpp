#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
using namespace std;

volatile int victim = 0;
volatile bool flag[2] = { false, false };

void Lock(int myID) {
	int other = 1 - myID;
	flag[myID] = true;

	victim = myID;

	atomic_thread_fence(memory_order_seq_cst);

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

	// �� 2000ms
}

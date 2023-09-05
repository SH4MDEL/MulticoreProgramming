#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int sum;
mutex m;

void func()
{
	auto s = 0;
	for (auto i = 0; i < 25'000'000; ++i) {
		s += 2;
	}
	m.lock();
	sum += s;
	m.unlock();
}

int main()
{
	thread t1(func);
	thread t2(func);
	t1.join(); t2.join();

	cout << sum << endl;
}
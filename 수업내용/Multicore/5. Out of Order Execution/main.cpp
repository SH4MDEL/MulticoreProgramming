#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
using namespace std;

constexpr int SIZE = 50'000'000;
int trace_x[SIZE];
int trace_y[SIZE];
volatile int x, y;


void Func0(int n)
{
	for (int i = 0; i < SIZE; ++i) {
		x = i;
		trace_y[i] = y;
	}
}

void Func1(int n)
{
	for (int i = 0; i < SIZE; ++i) {
		y = i;
		trace_x[i] = x;
	}
}

int main()
{
	thread t1(Func0, 0);
	thread t2(Func1, 1);

	t1.join(); t2.join();

	int count = 0;
	for (int i = 0; i < SIZE; ++i) {
		if (trace_x[i] != trace_y[i + 1]) continue;
		++count;
	}
	cout << count << endl;
}

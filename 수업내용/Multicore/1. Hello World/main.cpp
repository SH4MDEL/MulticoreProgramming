#include <iostream>
#include <thread>
using namespace std;

int a;

void func()
{
	for (int i = 0; i < 1'000'000; ++i) {
		a = a + 1;
	}
}

int main()
{
	thread c1(func);
	thread c2(func);
	c1.join(); c2.join();

	cout << a << endl;
}
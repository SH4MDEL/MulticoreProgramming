#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
using namespace std;

volatile bool done;
int* bound;
int g_error;

void Func0()
{
	for (int i = 0; i < 25000000; ++i) {
		*bound = -(1 + *bound);
		done = true;
	}
}

void Func1()
{
	while (!done) {
		int v = *bound;
		if ((v != 0) && (v != -1)) {
			++g_error;
		}
	}
}

int main()
{
	int ARR[32];

	long long temp = (long long)&ARR[31];	// 128����Ʈ �迭�� 124����Ʈ° ��ġ�� ��´�.	
	temp = temp - (temp % 64);				// �ּҸ� 64�� ����� �����. �迭�� ������ ����� �ʴ´�.
	temp -= 2;
	bound = (int*)temp;
	*bound = 0;

	thread t1(Func0);
	thread t2(Func1);

	t1.join(); t2.join();

	cout << g_error << endl;
}

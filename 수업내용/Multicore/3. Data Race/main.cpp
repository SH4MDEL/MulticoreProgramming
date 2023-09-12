#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <vector>
using namespace std;

volatile bool g_ready = false;
int g_data = 0;

void Receiver()
{
	while (!g_ready) {};
	std::cout << "I got " << g_data << std::endl;
}

void Sender()
{
	cin >> g_data;
	g_ready = true;
}

int main()
{
	thread t1(Receiver);
	thread t2(Sender);

	t1.join(); t2.join();

}
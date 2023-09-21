#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
using namespace std;

class Bakery 
{
public:
	Bakery(int threadNum) : m_threadNum{threadNum}
	{
		m_flag = new bool[threadNum];
		m_label = new int[threadNum];

		memset(m_flag, false, sizeof(bool) * threadNum);
		memset(m_label, 0, sizeof(int) * threadNum);
	}
	void Release()
	{
		delete[] m_flag;
		delete[] m_label;
	}
	void lock(int threadID) const
	{
		int i = threadID;
		m_flag[i] = true;
		m_label[i] = *max_element(m_label, m_label + m_threadNum) + 1; 
		m_flag[i] = false;

		for (int k = 0; k < m_threadNum; ++k) {
			while (m_flag[k]) {}
			while (m_label[k] and (m_label[k] < m_label[i] or
				(((m_label[k] == m_label[i]) and k < i) ? true : false))) {
			}
		}
	}
	void unlock(int threadID) const
	{
		m_label[threadID] = 0;
	}
private:
	bool* volatile m_flag;
	int* volatile m_label;
	int m_threadNum;
};

volatile int sum;
mutex m;

void func(int threadNum)
{
	for (auto i = 0; i < (10'000'000 / threadNum); ++i) {
		sum += 1;
	}
}

void funcWithMutex(int threadNum)
{
	for (auto i = 0; i < (10'000'000 / threadNum); ++i) {
		m.lock();
		sum += 1;
		m.unlock();
	}
}

void funcWithBakery(int threadNum, int threadID, Bakery b)
{
	for (auto i = 0; i < (10'000'000 / threadNum); ++i) {
		b.lock(threadID);
		sum += 1;
		b.unlock(threadID);
	}
}

int main()
{
	for (int i = 1; i <= 8; i *= 2) {
		cout << " ---------- " << i << " threads" << " ---------- " << endl;

		{
			sum = 0;
			vector<thread> v;
			auto t = chrono::high_resolution_clock::now();
			for (int j = 0; j < i; ++j) {
				v.emplace_back(func, i);
			}
			for (auto& t : v) t.join();
			auto d = chrono::high_resolution_clock::now() - t;

			cout << "No Lock" << endl;
			cout << chrono::duration_cast<chrono::milliseconds>(d).count()
				<< " millisec, ans : " << sum << endl;
		}

		{
			sum = 0;
			vector<thread> v;
			auto t = chrono::high_resolution_clock::now();
			for (int j = 0; j < i; ++j) {
				v.emplace_back(funcWithMutex, i);
			}
			for (auto& t : v) t.join();
			auto d = chrono::high_resolution_clock::now() - t;

			cout << "Mutex" << endl;
			cout << chrono::duration_cast<chrono::milliseconds>(d).count()
				<< " millisec, ans : " << sum << endl;
		}

		{
			sum = 0;
			Bakery b{ i };
			vector<thread> v;
			auto t = chrono::high_resolution_clock::now();
			for (int j = 0; j < i; ++j) {
				v.emplace_back(ref(funcWithBakery), i, j, b);
			}
			for (auto& t : v) t.join();
			auto d = chrono::high_resolution_clock::now() - t;

			cout << "Bakery" << endl;
			cout << chrono::duration_cast<chrono::milliseconds>(d).count()
				<< " millisec, ans : " << sum << endl;

			b.Release();
		}
	}

}
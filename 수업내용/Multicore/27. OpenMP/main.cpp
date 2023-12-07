#include <omp.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
using namespace std;
using namespace chrono;

int main()
{
	int sum = 0;
	auto start_t = system_clock::now();
	const int num_threads = thread::hardware_concurrency();
	vector<thread> threads;
	mutex m;
	for (int i = 0; i < num_threads; ++i) {
		threads.emplace_back([&sum, &num_threads, &m]() {
			for (int i = 0; i < 50'000'000 / num_threads; ++i) {
				lock_guard<mutex> l(m);
				sum += 2;
			}
		});
	}
	for (auto& th : threads) {
		th.join();
	}
	auto exec_t = system_clock::now() - start_t;
	auto m_sec = duration_cast<milliseconds>(exec_t).count();
	cout << "thread sum is " << sum << ", " << m_sec << "ms" << endl;

	sum = 0;
	start_t = system_clock::now();
#pragma omp parallel shared(sum)
	{
		const int num_threads = omp_get_num_threads();
		for (int i = 0; i < 50'000'000 / num_threads; ++i) {
#pragma omp critical
			sum += 2;
		}
	}

	exec_t = system_clock::now() - start_t;
	m_sec = duration_cast<milliseconds>(exec_t).count();
	cout << "OpenMP sum is " << sum << ", " << m_sec << "ms" << endl;

	sum = 0;
	start_t = system_clock::now();
#pragma omp parallel shared(sum)
	{
#pragma omp for schedule(dynamic, 100000) nowait
		for (int i = 0; i < 50'000'000 / num_threads; ++i) {
#pragma omp critical
			sum += 2;
		}
	}

	exec_t = system_clock::now() - start_t;
	m_sec = duration_cast<milliseconds>(exec_t).count();
	cout << "OpenMP sum is " << sum << ", " << m_sec << "ms" << endl;
}
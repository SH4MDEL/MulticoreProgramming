#include <tbb/parallel_for.h>
#include <iostream>
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
using namespace std;
using namespace chrono;

int main()
{
	{
		auto num_th = thread::hardware_concurrency();
		for (int i = 1; i <= num_th; ++i) {
			atomic_int sum = 0;
			vector<thread> threads;
			auto start_t = system_clock::now();
			threads.emplace_back([&]() {
				for (int j = 0; j < 50'000'000 / i; ++j) {
					sum += 2;
				}
				});
			for (auto& th : threads) {
				th.join();
			}
			auto exec_t = system_clock::now() - start_t;
			auto ms = duration_cast<milliseconds>(exec_t).count();
			cout << i << " Thread Sum = " << sum << ", " << ms << "ms " << endl;
		}
	}

	{
		atomic_int sum = 0;
		auto start_t = system_clock::now();
		tbb::parallel_for(0, 50'000'000, [&](int i) {
			sum += 2;
			});
		auto exec_t = system_clock::now() - start_t;
		auto ms = duration_cast<milliseconds>(exec_t).count();
		cout << "TBB Thread Sum = " << sum << ", " << ms << "ms " << endl;
	}
}
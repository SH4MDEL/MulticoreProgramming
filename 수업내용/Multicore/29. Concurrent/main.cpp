#include <fstream>
#include <iostream>
#include <chrono>
#include <string>
#include <unordered_map>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <tbb/parallel_for.h>

using namespace std;
using namespace chrono;

int main()
{
	vector<string> origin, data;
	ifstream readFile("TextBook.txt");
	string word;
	while (readFile >> word) {
		if (isalpha(word[0])) {
			origin.push_back(word);
		}
	}
	for (int i = 0; i < 1000; ++i) {
		for (const auto& s : origin) {
			data.push_back(s);
		}
	}
	{
		unordered_map<string, int> um;
		auto start_t = system_clock::now();

		for (const auto& s : data) ++um[s];

		auto exec_t = system_clock::now() - start_t;
		auto ms = duration_cast<milliseconds>(exec_t).count();
		cout << "Single Thread : " << ms << "ms " << endl;
	}

	{
		const int num_th = thread::hardware_concurrency();
		vector<thread> threads;
		mutex m;
		unordered_map<string, int> um;
		auto start_t = system_clock::now();

		for (int i = 0; i < num_th; ++i) {
			threads.emplace_back([&]() {
				const auto N = data.size();
				const auto SIZE = N / num_th;
				const auto start = i * SIZE;
				const auto end = start + SIZE;
				for (int ti = start; ti < end; ++i) {
					if (ti >= N) break;
					m.lock();
					++um[data[ti]];
					m.unlock();
				}
			});
		}
		for (auto& th : threads) th.join();

		auto exec_t = system_clock::now() - start_t;
		auto ms = duration_cast<milliseconds>(exec_t).count();
		cout << "Multi Thread : " << ms << "ms " << endl;
	}

	{
		const int num_th = thread::hardware_concurrency();
		vector<thread> threads;
		concurrency::concurrent_unordered_map<string, atomic_int> um;
		auto start_t = system_clock::now();
		const int N = data.size();
		parallel_for(0, N, [&](int i) {
			
			++um[data[i]];
			});
		auto exec_t = system_clock::now() - start_t;
		auto ms = duration_cast<milliseconds>(exec_t).count();
		cout << "TBB Concurrent unordered_map : " << ms << "ms " << endl;
	}

	{
		const int num_th = thread::hardware_concurrency();
		vector<thread> threads;
		concurrent_hash_map<string, atomic_int> um;
		auto start_t = system_clock::now();
		const int N = data.size();
		parallel_for(0, N, [&](int i) {
			++um[data[i]];
			});
		auto exec_t = system_clock::now() - start_t;
		auto ms = duration_cast<milliseconds>(exec_t).count();
		cout << "TBB Concurrent unordered_map : " << ms << "ms " << endl;
	}
}
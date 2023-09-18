#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
using namespace std;

class Bakery 
{
public:
	Bakery(int threadNum) {
		m_flag = make_unique<bool[]>(threadNum);
		m_label = make_unique<int[]>(threadNum);
	}
private:
	unique_ptr<bool[]> m_flag;
	unique_ptr<int[]> m_label;
};

int main()
{

}
// thread.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>                // std::cout
#include <thread>                // std::thread
#include <mutex>                // std::mutex, std::unique_lock
#include <vector>
#include <condition_variable>    // std::condition_variable
std::once_flag flag;
std::mutex mtx; // 全局互斥锁.
std::condition_variable cv; // 全局条件变量.
bool ready = false; // 全局标志位.

void go()
{
	std::unique_lock <std::mutex> lck(mtx);
	ready = true; // 设置全局标志位为 true.
	cv.notify_all(); // 唤醒所有线程.
}
void do_print_id(int id)
{
	std::unique_lock <std::mutex> lck(mtx);
	while (!ready) // 如果标志位不为 true, 则等待...
	{
		std::cout << "thread while" << id << '\n';
		cv.wait(lck); // 当前线程被阻塞, 当全局标志位变为 true 之后,
					  // 线程被唤醒, 继续往下执行打印线程编号id.
	}
	std::cout << "thread " << id << '\n';
}

void test1()
{
	std::thread threads[10];
	// spawn 10 threads:
	for (int i = 0; i < 10; ++i)
		threads[i] = std::thread(do_print_id, i);

	std::cout << "10 threads ready to race...\n";
	go(); // go!

	for (auto & th : threads)
		th.join();
}



void print_block(int n, char c) {
	//unique_lock有多组构造函数, 这里std::defer_lock不设置锁状态
	std::unique_lock<std::mutex> my_lock(mtx, std::defer_lock);
	//尝试加锁, 如果加锁成功则执行
	//(适合定时执行一个job的场景, 一个线程执行就可以, 可以用更新时间戳辅助)
	if (my_lock.try_lock())
	{
		for (int i = 0; i<n; ++i)
			std::cout << c;
		std::cout << '\n';
	}
}

void run_one(int &n) {
	std::call_once(flag, [&n] {n = n + 1; }); //只执行一次, 适合延迟加载; 多线程static变量情况
}

void test2()
{
	std::vector<std::thread> ver;
	int num = 0;
	for (auto i = 0; i < 10; ++i) {
		ver.emplace_back(print_block, 50, '*');
		ver.emplace_back(run_one, std::ref(num));
	}

	for (auto &t : ver) {
		static int i = 0;
		t.join();
		std::cout << i++ << std::endl;
	}
	std::cout << num << std::endl;
}


#include <functional>

void f(int& n1, int& n2, const int& n3)
{
	std::cout << "In function: n1[" << n1 << "]    n2[" << n2 << "]    n3[" << n3 << "]" << std::endl;
	++n1; // 增加存储于函数对象的 n1 副本
	++n2; // 增加 main() 的 n2
		  //++n3; // 编译错误
	std::cout << "In function end: n1[" << n1 << "]     n2[" << n2 << "]     n3[" << n3 << "]" << std::endl;
}

void test3()
{
	int n1 = 1, n2 = 1, n3 = 1;
	std::cout << "Before function: n1[" << n1 << "]     n2[" << n2 << "]     n3[" << n3 << "]" << std::endl;
	std::function<void()> bound_f = std::bind(f, n1, std::ref(n2), std::cref(n3));
	bound_f();
	std::cout << "After function: n1[" << n1 << "]     n2[" << n2 << "]     n3[" << n3 << "]" << std::endl;
}



//static std::once_flag oc;
//class Singleton;
//Singleton* Singleton::m_instance;
//
//Singleton* Singleton::getInstance() {
//
//	std::call_once(oc, [&]() { m_instance = newSingleton(); });
//
//	return m_instance;
//
//}
#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

int test4()
{

	ThreadPool pool(4);
	std::vector< std::future<int> > results;

	for (int i = 0; i < 8; ++i) {
		results.emplace_back(
			pool.enqueue([i] {
			std::cout << "hello " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << "world " << i << std::endl;
			return i*i;
		})
		);
	}

	for (auto && result : results)
		std::cout << result.get() << ' ';
	std::cout << std::endl;

	return 0;
}



#include <future>
#include <iostream>


bool is_prime(int x)
{
	while (x > 1)
	{
		x--;
	}
	return true;
}

int test5()
{
	std::future<bool> fut = std::async(is_prime, 700020007);
	std::cout << "please wait";
	std::chrono::milliseconds span(100);
	while (fut.wait_for(span) != std::future_status::ready)
		std::cout << ".";
	std::cout << std::endl;

	bool ret = fut.get();
	std::cout << "final result: " << ret << std::endl;
	return 0;
}

int main()
{
//	test1();
//	test2();
//	test3();
	test4();


	//test5();
	// create thread pool with 4 worker threads




	//ThreadPool pool(4);

	// enqueue and store future
	//auto result = pool.enqueue([](int answer) { return answer; }, 42);

	// get result from future
	//std::cout << result.get() << std::endl;
    return 0;
}


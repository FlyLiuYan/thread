#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif
/*代码详细解释：

    首先我们看析构函数：inline ThreadPool::ThreadPool(size_t threads) : stop(false){}，这个函数向线程队列worker插入threads个线程，每个线程对象用一个匿名函数lambda来初始化，每个匿名函数会一直循环着从任务队列中取出任务来执行。它首先对互斥量加锁，获得互斥锁后，调用条件变量的wait()函数等待条件发生，传入wait()函数的第二个参数为一个匿名函数lambda，当函数返回值为false时，wait会使得该线程阻塞（任务队列为空时会阻塞），并且对互斥量进行解锁。当函数返回值为true（任务队列不为空）并且收到条件变量的通知后，wait函数使线程解阻塞，并且对互斥量加锁。接着从任务队列里面弹出一个任务，对互斥量自动解锁，并执行这个任务。

    接着我们看添加任务的函数：auto ThreadPool::enqueue(F&& f, Args&&... args)-> std::future<typename std::result_of<F(Args...)>::type>{}，这个函数向任务队列中加入一个任务。它首先创建一个名为task的智能指针，接着对互斥量加锁，然后把该任务加入到任务队列中，对互斥量自动解锁。调用条件变量的notify_one()函数，使得阻塞到这个条件变量的线程解阻塞。

    总结：初始化时，线程中的每个函数都会阻塞到条件变量那里，当任务队列中新加入一个任务时，通知阻塞到条件变量的某一个线程，接着这个线程执行：互斥量加锁——>任务队列出队——>互斥量解锁——>执行任务。当线程执行完任务之后，如果任务队列不为空，则继续从任务队列那里取出任务执行，如果任务队列为空则阻塞到条件变量那里。

  (1) std::result_of<func()>::type：获取func()函数返回值的类型；

  (2) typename：作用是告诉c++编译器，typename后面的字符串为一个类型名称，而不是成员函数或者成员变量；

  (3) 匿名函数lambda: 允许incline函数的定义被当做一个参数，用法为:[]()->type{},中括号内指定了传递方式（值传递或引用传递），小括号内指明函数的形参，->type指定了返回值类型，花括号内指定了函数体。

  (4) std::funtion<void()> ：声明一个函数类型，接收任意原型是void()的函数、或函数对象、或是匿名函数。void() 意思是不带参数，没有返回值。

  (5) std::unique_lock<> ： C++多线程编程中通常会对共享的数据进行写保护，以防止多线程在对共享数据成员进行读写时造成资源争抢导致程序出现未定义的行为。通常的做法是在修改共享数据成员的时候进行加锁--mutex。在使用锁的时候通常是在对共享数据进行修改之前进行lock操作，在写完之后再进行unlock操作，进场会出现由于疏忽导致由于lock之后在离开共享成员操作区域时忘记unlock，导致死锁。针对以上的问题，C++11中引入了std::unique_lock与std::lock_guard两种数据结构。通过对lock和unlock进行一次薄的封装，实现对象生命周期结束后自动解锁的功能，参考。用法如下：
    1）新建一个unique_lock对象 ：std::mutex mymutex; 
    2）给对象传入一个std::mutex 对象作为参数：unique_lock lock(mymutex);

  (6) std::condition_variable：当 std::condition_variable 对象的某个 wait 函数被调用的时候，它使用 std::unique_lock(通过 std::mutex) 来锁住当前线程。当前线程会一直被阻塞，直到另外一个线程在相同的 std::condition_variable 对象上调用了 notification 函数来唤醒当前线程。
    std::condition_variable::wait() ：std::condition_variable提供了两种 wait() 函数：
    1) 第一种情况：void wait (unique_lock<mutex>& lock);当前线程调用 wait() 后将被阻塞(此时当前线程应该获得了锁（mutex），不妨设获得锁 lock)，直到另外某个线程调用 notify_* 唤醒了当前线程。在线程被阻塞时，该函数会自动调用 lock.unlock() 释放锁，使得其他被阻塞在锁竞争上的线程得以继续执行。另外，一旦当前线程获得通知(notified，通常是另外某个线程调用 notify_* 唤醒了当前线程)，wait() 函数也是自动调用 lock.lock()，使得 lock 的状态和 wait 函数被调用时相同。
   2) 第二种情况：void wait (unique_lock<mutex>& lock, Predicate pred)（即设置了 Predicate），只有当 pred 条件为 false 时调用 wait() 才会阻塞当前线程，并且在收到其他线程的通知后只有当 pred 为 true 时才会被解除阻塞。
   std::condition_variable::notify_one() ：唤醒某个等待(wait)线程。如果当前没有等待线程，则该函数什么也不做，如果同时存在多个等待线程，则唤醒某个线程是不确定的；

  (7) using return_type = typename xxx ：指定别名,和typedef的作用类似；

  (8) std::make_shared<type> : 创建智能指针，需要传入类型；

  (9) std::future<> : 可以用来获取异步任务的结果，因此可以把它当成一种简单的线程间同步的手段；

  (10) std::packaged_task<> : std::packaged_task 包装一个可调用的对象，并且允许异步获取该可调用对象产生的结果，从包装可调用对象意义上来讲，std::packaged_task 与 std::function 类似，只不过 std::packaged_task 将其包装的可调用对象的执行结果传递给一个 std::future 对象（该对象通常在另外一个线程中获取 std::packaged_task 任务的执行结果）；

  (11) std::bind() : bind的思想实际上是一种延迟计算的思想，将可调用对象保存起来，然后在需要的时候再调用。而且这种绑定是非常灵活的，不论是普通函数、函数对象、还是成员函数都可以绑定，而且其参数可以支持占位符，比如你可以这样绑定一个二元函数auto f = bind(&func, _1, _2);，调用的时候通过f(1,2)实现调用；

  (12) std::forward<F> : 左值与右值的概念其实在C++0x中就有了。概括的讲，凡是能够取地址的可以称之为左值，反之称之为右值，C++中并没有对左值和右值给出明确的定义，从其解决手段来看类似上面的定义，当然我们还可以定义为：有名字的对象为左值，没有名字的对象为右值。std :: forward有一个用例：将模板函数参数(函数内部)转换为调用者用来传递它的值类别(lvalue或rvalue)。这允许右值参数作为右值传递，左值作为左值传递，一种称为“完全转发”的方案；

  (13) std::packged_task::get_future： 获取与共享状态相关联的 std::future 对象。在调用该函数之后，两个对象共享相同的共享状态。
————————————————
版权声明：本文为CSDN博主「蓬莱道人」的原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/MOU_IT/article/details/88712090
*/
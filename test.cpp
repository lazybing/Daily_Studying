#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
using namespace std;

static const int kItemsToProduce = 200;//定义生产者能够生产的最大产品个数
std::mutex stdoutMutex;//多线程标准输出 同步锁

struct ItemRepository
{
    deque<int> itemQueue;   // 这里用队列代表仓库缓冲区
    int MaxSize = 10;       // 仓库所容纳的产品最大个数
    int itemCounter=0;
    std::mutex mtx;         // 互斥量,保护产品缓冲区
    std::mutex itemCounterMtx;
    std::condition_variable repository_notFull;     // 条件变量, 指产品仓库缓冲区不为满
    std::condition_variable repository_notEmpty;    // 条件变量, 指产品仓库缓冲区不为空
}gItemRepository;   // 产品库全局变量，生产者和消费者操作该变量.

typedef struct ItemRepository ItemRepository;


// 生产 产品
void ProduceItem(ItemRepository &itemRepo, int item)
{
    std::unique_lock<std::mutex> lock(itemRepo.mtx);
    itemRepo.repository_notFull.wait(lock, [&itemRepo] {
        bool full = itemRepo.itemQueue.size() >= itemRepo.MaxSize;
        if (full)
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "仓库满了,生产者等待中..." << "thread id = " << std::this_thread::get_id() << endl;
        }
        return !full;
    });

    itemRepo.itemQueue.push_back(item);         // 仓库放入产品
    itemRepo.repository_notEmpty.notify_all();  // 通知消费者仓库不为空
    lock.unlock();  // 释放锁
}

// 消费 产品
int ConsumeItem(ItemRepository &itemRepo)
{
    int data;
    std::unique_lock<std::mutex> lock(itemRepo.mtx);

    // 等待信号不为空的通知，wait 第二参数为true时 向下执行，否则一直等待
    itemRepo.repository_notEmpty.wait(lock, [&itemRepo] {
        bool empty = itemRepo.itemQueue.empty();
        if (empty)
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "仓库空了,消费者等待中..." << "thread id = " << std::this_thread::get_id() << endl;
        }

        return !empty;
    });

    data = itemRepo.itemQueue.front();
    itemRepo.itemQueue.pop_front();
    itemRepo.repository_notFull.notify_all();
    lock.unlock();
    return data;
}

// 生产者任务
void ProducerTask()
{
    for (int i = 1;i <= kItemsToProduce;++i)
    {
        ProduceItem(gItemRepository, i);    // 生产产品
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "Produce the " << i << " ^th item..." << endl;
        }
    }
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        cout << "Producer Thread exit.... " << endl;
    }
}

// 消费者任务
void ConsumerTask(int th_ID)
{
    bool readyToExit = false;
    while (true)
    {
        this_thread::sleep_for(std::chrono::seconds(1));
        std::unique_lock<std::mutex> lock(gItemRepository.itemCounterMtx);  // 仓库产品消费计数器保持多线程互斥
        if (gItemRepository.itemCounter < kItemsToProduce)
        {
            int item = ConsumeItem(gItemRepository);    // 消费产品
            gItemRepository.itemCounter++;  // 每消费一次进行计数器+1
            {
                std::lock_guard<std::mutex> lock(stdoutMutex);
                cout << "Consume Thread " <<th_ID<<" the " <<item << "^th item..." << endl;
            }
        }
        else
        {
            readyToExit = true;
        }
        lock.unlock();
        if (readyToExit)
            break;
    }
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        cout << "Consumer Thread "<<th_ID<<" exit...." << endl;
    }
}

int main()
{
    std::thread producer(ProducerTask);
    std::thread consumer1(ConsumerTask,1);
    std::thread consumer2(ConsumerTask,2);
    std::thread consumer3(ConsumerTask,3);
    std::thread consumer4(ConsumerTask,4);

    producer.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();

    system("pause");
    return 0;
}


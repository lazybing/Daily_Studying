#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

using namespace std;

static const int kItemsToProduce = 20;  //the total number of producer produce
std::mutex stdoutMutex; //multithread standard output

struct ItemRepository
{
    deque<int> itemQueue;
    int MaxSize = 10;
    int itemCounter = 0;
    std::mutex mtx;
    std::mutex itemCounterMtx;
    std::condition_variable repository_notFull;
    std::condition_variable repository_notEmpty;
}gItemRepository;

typedef struct ItemRepository ItemRepository;

//produce product
void ProduceItem(ItemRepository &itemRepo, int item)
{
    std::unique_lock<std::mutex> lock(itemRepo.mtx);
    itemRepo.repository_notFull.wait(lock, [&itemRepo] {
        bool full = itemRepo.itemQueue.size() >= itemRepo.MaxSize;                         
        if (full)
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "repository is full, producer is waiting..." << "thread id = " << std::this_thread::get_id() << endl;
        }
        return !full;
    });

    itemRepo.itemQueue.push_back(item);
    itemRepo.repository_notEmpty.notify_all();
    lock.unlock();
}

//produce task
void ProducerTask()
{
    for (int i = 1; i <= kItemsToProduce; ++i)
    {
        ProduceItem(gItemRepository, i);
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "Produce the " << i << " ^th item..." << endl;
        }
    }
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        cout << "Producer Thread exit..." << endl;
    }
}

int ConsumeItem(ItemRepository &itemRepo)
{
    int data;
    std::unique_lock<std::mutex> lock(itemRepo.mtx);

    itemRepo.repository_notEmpty.wait(lock, [&itemRepo] {
        bool empty = itemRepo.itemQueue.empty();                              
        if (empty)
        {
            std::lock_guard<std::mutex> lock(stdoutMutex);
            cout << "repo is empty, consumer waiting ..." << "thread id = " << std::this_thread::get_id() << endl;
        }

        return !empty;
    });

    data = itemRepo.itemQueue.front();
    itemRepo.itemQueue.pop_front();
    itemRepo.repository_notFull.notify_all();
    lock.unlock();
    return data;
}

//consumver task
void ConsumerTask(int th_ID)
{
    bool readyToExit = false;
    while (true)
    {
        this_thread::sleep_for(std::chrono::seconds(1));
        std::unique_lock<std::mutex> lock(gItemRepository.itemCounterMtx);
        if (gItemRepository.itemCounter < kItemsToProduce)
        {
            int item = ConsumeItem(gItemRepository);
            gItemRepository.itemCounter++;
            {
                std::lock_guard<std::mutex> lock(stdoutMutex);
                cout << "Consume Thread " << th_ID << " the " << item << " ^th item..." << endl;
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
        cout << "Consumer Thread " << th_ID << " exit..." << endl;
    }
}

int main()
{
    std::thread producer(ProducerTask);
    std::thread consumer1(ConsumerTask, 1);
    std::thread consumer2(ConsumerTask, 2);
    std::thread consumer3(ConsumerTask, 3);
    std::thread consumer4(ConsumerTask, 4);

    producer.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();

    system("pause");
    return 0;
}

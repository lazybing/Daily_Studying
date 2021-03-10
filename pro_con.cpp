#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static const int repository_size = 10;
static const int item_total = 20;

std::mutex mtx;
std::mutex mtx_counter;

std::condition_variable repo_not_full;  //cond indicate the buffer is not full
std::condition_variable repo_not_empty; //cond indicate the buffer is not empty, has data

int item_buffer[repository_size];

static std::size_t read_position    = 0;    //consumer read position
static std::size_t write_position   = 0;    //producer write position

static std::size_t item_counter = 0;    //consumer consume counter

std::chrono::seconds t(1);

void produce_item(int i)
{
    std::unique_lock<std::mutex> lck(mtx);
    //item buffer is full, just wait here.
    while(((write_position + 1) % repository_size) == read_position)
    {
        std::cout << "Porducer is waiting for an empty slot ..." << std::endl;
        repo_not_full.wait(lck);
    }

    item_buffer[write_position] = i;
    write_position++;

    if (write_position == repository_size)
    {
        write_position = 0;
    }

    repo_not_empty.notify_all();    //notify to all consumer
    lck.unlock();
}

void Producer_thread()
{
    for (int i = 1; i <= item_total; i++)
    {
        std::cout << "producer produce " << i << "product" << std::endl;
        produce_item(i);
    }
}

int consume_item()
{
    int data;
    std::unique_lock<std::mutex> lck(mtx);

    //item buffer is empty, just wait here.
    while (write_position == read_position)
    {
        std::cout << "Consumer si waiting for item ..." << std::endl;
        repo_not_empty.wait(lck);
    }

    data = item_buffer[read_position];
    read_position++;

    if (read_position >= repository_size)
    {
        read_position = 0;
    }

    repo_not_full.notify_all();
    lck.unlock();

    return data;
}

void Consumer_thread()
{
    bool read_to_exit = false;
    while (1)
    {
        std::this_thread::sleep_for(t);
        std::unique_lock<std::mutex> lck(mtx_counter);
        if (item_counter < item_total)
        {
            int item = consume_item();
            ++item_counter;
            std::cout << "consumer thread" << std::this_thread::get_id()
                << "consume " << item << "producer" << std::endl;
        } else {
            read_to_exit = true;
        }

        if (read_to_exit = true)
            break;
    }

    std::cout << "Consumer thread " << std::this_thread::get_id()
        << " is exiting..." << std::endl;
}

int main()
{
    std::thread producer(Producer_thread);  //create producer thread
    std::vector<std::thread> thread_vector;
    for (int i = 0; i != 5; i++)
    {
        thread_vector.push_back(std::thread(Consumer_thread));  //create consumer thread
    }

    producer.join();
    for (auto &thr:thread_vector)
    {
        thr.join();
    }
}

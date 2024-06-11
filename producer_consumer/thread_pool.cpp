#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <Windows.h>

//template<typename returnType, typename... Args>
class Task
{
public:
    Task()
        : m_isDefault(true), m_task([] {std::cout << "default task\n"; }) {}
    Task(std::function<void()>&& task)
        : m_isDefault(false), m_task(std::move(task)) {}

    void exec()
    {
        m_task();
    }

    bool isDefault() { return m_isDefault; }
private:
    std::function<void()> m_task;
    bool m_isDefault;
};

class Worker
{
    enum {maxThreads = 20};
public:
    Worker(size_t threadCount = 1) :
        m_running(true),
        m_finish(false),
        m_threads( ((threadCount > maxThreads) ? maxThreads : threadCount) )
    {
        for (int i = 0; i < ((threadCount > maxThreads) ? maxThreads : threadCount); i++) {
            m_threads[i] = std::thread(&Worker::workThread, this);
        }
    }

    ~Worker()
    {
        m_running = false;
        m_cv.notify_all();
        for (int i = 0; i < m_threads.size(); i++) {
            if(m_threads[i].joinable())
                m_threads[i].join();
        }
    }

    void push(Task&& task)
    {
        {
            std::lock_guard<std::mutex> lock(this->m_lock);
            m_taskQueue.push(task);
        }
        m_cv.notify_one();
    }
    void join()
    {
        m_finish = true;
        m_cv.notify_all();
        for (int i = 0; i < m_threads.size(); i++) {
            if (m_threads[i].joinable())
                m_threads[i].join();
        }
    }

    bool running() const { return m_running; }
    bool finish() const { return m_finish; }
    int threadCount() const { return m_threads.size(); }

private:
    void workThread()
    {
        /*DWORD_PTR mask = 1 << (m_core++ % 4);
        if (!SetThreadAffinityMask(GetCurrentThread(), mask)) {
            std::cerr << "Error setting thread affinity." << std::endl;
        }*/
        while (m_running)
        {
            std::unique_lock<std::mutex> lock(m_lock);
            this->m_cv.wait(lock, [this] { return (!this->m_taskQueue.empty() || !this->running() || this->finish()); });
            if (!this->running()) { return; }
            if (this->m_taskQueue.empty() && this->finish()) { return; }
            Task t = m_taskQueue.front();
            m_taskQueue.pop();
            lock.unlock();
            m_cv.notify_one();
            
            // Do work on t
            if (!t.isDefault()) {
                //std::cout << "thread ID: " << std::this_thread::get_id() << std::endl;
                t.exec();
            }
        }
    }

private:
    std::queue<Task> m_taskQueue;
    std::mutex m_lock;
    std::condition_variable m_cv;
    bool m_running;
    bool m_finish;
    //int m_core = 0;
    std::vector<std::thread> m_threads;
};

void test()
{
    Worker w(15);
    std::cout << "Number of threads: " << w.threadCount() << std::endl;

    for (int i = 0; i < 10; i++) {
        w.push(Task([] {std::this_thread::sleep_for(std::chrono::seconds(1)); }));
    }
    w.join();
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    test();

    auto end = std::chrono::high_resolution_clock::now();


    auto durationMicro = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto durationMili = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto durationSec = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "\n\ntime:\n" << durationSec.count() << "\t\t[seconds]" << std::endl;
    std::cout << durationMili.count() << "\t\t[miliseconds]" << std::endl;
    std::cout << durationMicro.count() << "\t\t[microseconds]\n" << std::endl;
}

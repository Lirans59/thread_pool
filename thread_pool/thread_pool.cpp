#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip>

//template<typename returnType, typename... Args>
class Task
{
public:
    typedef std::function<void()> task_t;

    Task() :
        m_isDefault(true),
        m_task([] {})
    {}
    Task(task_t&& task) :
        m_isDefault(false),
        m_task(task)
    {}

    void exec()
    {
        m_task();
    }

    bool isDefault() { return m_isDefault; }
private:
    bool m_isDefault;
    task_t m_task;
};

class ThreadPool
{
    const size_t maxThreads = std::thread::hardware_concurrency();
public:
    ThreadPool(size_t threadCount = 1) :
        m_running(true),
        m_finish(false),
        m_thread_pool( ((threadCount > maxThreads) ? maxThreads : threadCount) )
    {
        for (int i = 0; i < ((threadCount > maxThreads) ? maxThreads : threadCount); i++) {
            m_thread_pool[i] = std::thread(&ThreadPool::workThread, this);
        }
    }

    ~ThreadPool()
    {
        m_running = false;
        m_task_cv.notify_all();
        for (int i = 0; i < m_thread_pool.size(); i++) {
            if(m_thread_pool[i].joinable())
                m_thread_pool[i].join();
        }
    }

    void push(Task&& task)
    {
        {
            std::lock_guard<std::mutex> lock(this->m_task_mtx);
            m_task_queue.push(task);
        }
        m_task_cv.notify_one();
    }

    void join()
    {
        m_finish = true;
        m_task_cv.notify_all();
        for (int i = 0; i < m_thread_pool.size(); i++) {
            if (m_thread_pool[i].joinable())
                m_thread_pool[i].join();
        }
    }

    bool running() const { return m_running; }
    bool finish() const { return m_finish; }
    int threadCount() const { return m_thread_pool.size(); }

private:
    void workThread()
    {
        while (m_running)
        {
            std::unique_lock<std::mutex> lock(m_task_mtx);
            this->m_task_cv.wait(lock, [this] { return (!this->m_task_queue.empty() || !this->running() || this->finish()); });
            if (!this->running()) { return; }
            if (this->m_task_queue.empty() && this->finish()) { return; }
            Task t = std::move(m_task_queue.front());
            m_task_queue.pop();
            lock.unlock();
            m_task_cv.notify_one();
            
            // Do work on t
            if (!t.isDefault()) {
                //std::cout << "thread ID: " << std::this_thread::get_id() << std::endl;
                t.exec();
            }
        }
    }

private:
    std::queue<Task>            m_task_queue;
    std::mutex                  m_task_mtx;
    std::condition_variable     m_task_cv;

    std::queue<std::istream>    m_result;
    std::mutex                  m_result_mtx;
    std::condition_variable     m_result_cv;

    bool                        m_running;
    bool                        m_finish;

    std::vector<std::thread>    m_thread_pool;
};

void test(unsigned int threadCount, unsigned int taskCount)
{
    ThreadPool tp(threadCount);
    for (int i = 0; i < taskCount; i++) {
        tp.push(Task([i] {std::this_thread::sleep_for(std::chrono::seconds(1)); }));
    }
    tp.join();
}

int main()
{
    unsigned int threadCount, taskCount;
    std::cout << "Enter number of threads: "; std::cin >> threadCount;
    std::cout << "Enter number of tasks: "; std::cin >> taskCount;

    auto start = std::chrono::high_resolution_clock::now();

    test(threadCount, taskCount);

    auto end = std::chrono::high_resolution_clock::now();


    auto durationMicro = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto durationMili = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto durationSec = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "\n\nTime:" << std::endl;
    std::cout << std::setw(20) << std::left << durationSec.count() << "[seconds]" << std::endl;
    std::cout << std::setw(20) << std::left << durationMili.count() << "[miliseconds]" << std::endl;
    std::cout << std::setw(20) << std::left << durationMicro.count() << "[microseconds]\n" << std::endl;
}

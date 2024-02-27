/**
 * @brief 协程调度器
 * @details N-M协程调度器，内部有一个线程池(直接使用sylar的线程池)，
 * 支持协程在线程池中切换
 */
#include <memory>
#include "Mutex.h"
#include <string>
#include <vector>
#include "Threads.h"
#include "Coroutine.h"
#include <list>
#include <atomic>

class Scheduler
{
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    /**
     * @brief 创建调度器
     * @param[in] threads 线程数量
     * @param[in] use_caller 是否使用当前的线程作为执行任务的线程
     * @param[in] name 名称
     */
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "Scheduler");

    /**
     * @brief 析构函数
     */

    virtual ~Scheduler();

private:
    /**
     * @brief 调度任务，协程/函数二选一，可指定在哪个线程上调度
     *
     */
    struct ScheduleTask
    {
        Coroutine::ptr coroutine;
        std::function<void()> cb;
        int thread;

        ScheduleTask(Coroutine::ptr c, int thr)
        {
            coroutine = c;
            thread = thr;
        }
        ScheduleTask(Coroutine::ptr *c, int thr)
        {
            coroutine.swap(*c);
            thread = thr;
        }
        ScheduleTask(std::function<void()> f, int thr)
        {
            cb = f;
            thread = thr;
        }
        ScheduleTask()
        {
            thread = -1;
        }

        void reset()
        {
            coroutine = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

    // 成员变量
private:
    // 协程调度器名称
    std::string m_name;
    // 互斥锁
    MutexType m_mutex;
    // 线程池
    std::vector<Thread::ptr> m_threads;
    // 任务队列
    std::list<ScheduleTask> m_tasks;
    // 线程池的线程ID数组
    std::vector<int> m_threadIds;
    // 工作线程数量，不包含use_caller的主线程
    size_t m_threadCount = 0;
    // 活跃线程数量
    std::atomic<size_t> m_activeThreadCount = {0};
    // idle线程数
    std::atomic<size_t> m_idleThreadCount = {0};

    // 是否use_caller
    bool m_useCaller;
    // use_caller为true时，调度器的调度协程
    Coroutine::ptr m_scheduleCoroutine;
    // use_caller为true时，调度器所在的线程id
    int m_rootThread = 0;

    // 是否正在停止
    bool m_stopping = false;
};
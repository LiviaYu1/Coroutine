#ifndef SCHEDULER_H
#define SCHEDULER_H
/**
 * @brief 协程调度器
 * @details N-M协程调度器，内部有一个线程池(直接使用sylar的线程池)，
 * 支持协程在线程池中切换
 */
#include <memory>
#include "../Mutex/Mutex.h"
#include <string>
#include <vector>
#include "../Thread/Threads.h"
#include "../Coroutine/Coroutine.h"
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

    /**
     * @brief 获取调度器名称
     */
    const std::string &getName() const
    {
        return m_name;
    }

    /**
     * @brief 获取当前调度器指针
     */
    static Scheduler *GetThis();

    /**
     * @brief 获取当前线程主协程
     */
    static Coroutine *GetMainCoroutine();

    /**
     * @brief 添加调度任务
     * @tparam CoOrFunc 调度任务类型，可以是协程或者函数指针
     * @param[in] cf 协程或者指针
     * @param[in] thread 指定该任务的线程号，-1为任意线程
     */
    template <class CoOrFunc>
    void schedule(CoOrFunc cf, int thread = -1)
    {
        bool need_tickle=false;
        //获得锁，
        //根据scheduler的状态决定是否need_tickle，如果为true
        //则当前任务队列有东西了，就tickle()(通知有任务了)
        {
            MutexType::Lock lock(m_mutex);
            need_tickle=scheduleNoLock(cf,thread);
        }
        if(need_tickle)
        {
            tickle();
        }
    }

    /**
     * @brief 启动调度器
     */

    void start();

    /**
     * @brief 停止调度器，等所有调度任务都执行完成之后再返回
     */
    void stop();

protected:
    /**
     * @brief 通知协程调度器有任务了
     */
    virtual void tickle();

    /**
     * @brief 协程调度函数
     */

    void run();

    /**
     * @brief 无任务调度时执行idle协程
     */
    virtual void idle();

    /**
     * @brief 返回是否可以停止
     */
    virtual bool stopping();

    /**
     * @brief 设置当前的协程调度器
     */
    void setThis();

    /**
     * @brief 返回是否有空闲线程
     * @details 调度协程调度idle协程时该数量+1，返回时-1
     */
    bool hasIdleThreads()
    {
        return m_idleThreadCount > 0;
    }

private:
    /**
     * @brief 调度任务，协程/函数二选一，可指定在哪个线程上调度
     *
     */
    struct ScheduleTask
    {
        Coroutine::ptr coroutine;
        std::function<void()> func;
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
            func = f;
            thread = thr;
        }
        ScheduleTask()
        {
            thread = -1;
        }

        void reset()
        {
            coroutine = nullptr;
            func = nullptr;
            thread = -1;
        }
    };

    /**
     * @brief 添加调度任务，无锁
     * @tparam CoOrFunc 调度任务类型，可以是协程对象或函数指针
     * @param[in] fc 协程对象或指针
     * @param[in] thread 指定运行该任务的线程号，-1表示任意线程
     */
    template<class CoOrFunc>
    bool scheduleNoLock(CoOrFunc cf,int thread)
    {
        bool need_tickle=m_tasks.empty();//如果为空，那么scheduler需要被通知加任务了，如果不空，说明这个scheduler是一直在执行的，不需要tickle

        ScheduleTask task(cf,thread);
        if(task.coroutine||task.func)
        {
            m_tasks.push_back(task);
        }
        return need_tickle;
    }


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

#endif
#include "Scheduler.h"
#include <assert.h>
#include "../util.h"

// 当前线程的调度器
static thread_local Scheduler *t_scheduler = nullptr;
// 当前线程的调度协程，每个线程都独有一份
static thread_local Coroutine *t_scheduler_coroutine = nullptr;

/**
 * @brief 初始化调度线程池，如果只使用caller线程进行调度，那这个方法啥也不做
 * @param[in] threads 线程数量
 * @param[in] use_caller 是否使用当前的线程作为执行任务的线程
 * @param[in] name 名称
 */
Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
{
    assert(threads > 0);
    m_useCaller = use_caller;
    m_name = name;

    if (use_caller)
    {
        --threads;
        Coroutine::GetThis();
        assert(GetThis() == nullptr);
        t_scheduler = this;
        /**
         * 在user_caller为true时，初始化caller线程的调度协程
         * caller线程的调度协程不会被调度器所调度，caller线程的调度协程停止时，应返回caller线程的主协程
         */
        m_scheduleCoroutine.reset(new Coroutine(std::bind(&Scheduler::run, this), 0, false));

        Thread::SetName(m_name);
        t_scheduler_coroutine = m_scheduleCoroutine.get();
        m_rootThread = ybb::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    }
    else
    {
        m_rootThread = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    printf("Scheduler::~Scheduler()\n");
    assert(m_stopping);
    if (GetThis() == this)
    {
        t_scheduler = nullptr;
    }
}

Scheduler *Scheduler::GetThis()
{
    return t_scheduler;
}

Coroutine *Scheduler::GetMainCoroutine()
{
    return t_scheduler_coroutine;
}

/**
 * @brief 启动调度器
 */

void Scheduler::start()
{
    printf("Scheduler start\n");
    MutexType::Lock lock(m_mutex);

    if (m_stopping)
    {
        perror("Scheduler is stopped\n");
        return;
    }
    assert(m_threads.empty());
    m_threads.resize(m_threadCount);

    for (size_t i = 0; i < m_threadCount; i++)
    {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + '_' + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
}

// 所有任务都执行完了才能stop
bool Scheduler::stopping()
{
    MutexType::Lock lock(m_mutex);
    return m_tasks.empty() && m_activeThreadCount == 0 && m_stopping;
}
/**
 * @brief 设置当前的协程调度器
 */
void Scheduler::setThis()
{
    t_scheduler=this;
}

/**
 * @brief 协程调度函数
 */
void Scheduler::run()
{
    printf("Scheduler run\n");
    setThis();

    if (ybb::GetThreadId() != m_rootThread)
    {
        t_scheduler_coroutine = Coroutine::GetThis().get();
    }

    // 挂机协程
    Coroutine::ptr idle_coroutine(new Coroutine(std::bind(&Scheduler::idle, this)));
    Coroutine::ptr func_coroutine;
    ScheduleTask task;
    while (true)
    {
        task.reset();
        bool tickle_me = false; // 是否tickle其他线程进行任务调度
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_tasks.begin();
            // 遍历所有调度任务
            while (it != m_tasks.end())
            {
                // thread!=-1指定任意线程
                // thread!=GetThreadId()指定的不是当前线程
                if (it->thread != -1 && it->thread != ybb::GetThreadId())
                {
                    // 指定了调度线程，但不是在当前线程上调度，标记一下需要通知其他线程进行调度，然后跳过这个任务，继续下一个
                    ++it;
                    tickle_me = true;
                    continue;
                }
                // 排除了上面两种情况，那么就只有指定任意线程或者是当前线程了
                assert(it->coroutine || it->func);
                if (it->coroutine)
                {
                    // 任务队列中的协程应该都是ready的
                    assert(it->coroutine->getState() == Coroutine::READY);
                }
                // 调度线程找到一个任务，准备开始调度，将其从任务队列中删除，并且活动线程数++
                task = *it;
                m_tasks.erase(it++);
                ++m_activeThreadCount;
                break;
            }
            tickle_me = tickle_me | (it != m_tasks.end());
        }
        // 还有任务，唤醒一下其他的线程
        if (tickle_me)
        {
            tickle();
        }
        if (task.coroutine) // task中是协程
        {
            // resume返回的时候，已经执行完毕了，所以active--
            task.coroutine->resume();
            --m_activeThreadCount;
            task.reset();
        }
        else if (task.func) // task中是函数
        {
            if (func_coroutine)
            {
                func_coroutine->reset(task.func);
            }
            else
            {
                func_coroutine.reset(new Coroutine(task.func));
            }
            task.reset();
            func_coroutine->resume();
            --m_activeThreadCount;
            func_coroutine.reset();
        }
        else // 这里就是任务队列为空，调度idle
        {
            if (idle_coroutine->getState() == Coroutine::TERM)
            {
                printf("idle coroutine term\n");
                break;
            }
            ++m_idleThreadCount;
            idle_coroutine->resume();
            --m_idleThreadCount;
        }
    }
    printf("Scheduler run exit()\n");
}

void Scheduler::stop()
{
    printf("Scheduler stop\n");
    if (stopping())
    {
        return;
    }
    m_stopping = true;

    /// 如果use caller，那只能由caller线程发起stop
    if (m_useCaller)
    {
        assert(GetThis() == this);
    }
    else
    {
        assert(GetThis() != this);
    }

    for (size_t i = 0; i < m_threadCount; i++)
    {
        tickle();
    }

    if (m_scheduleCoroutine)
    {
        tickle();
    }

    /// 在use caller情况下，调度器协程结束时，应该返回caller协程
    if (m_scheduleCoroutine)
    {
        m_scheduleCoroutine->resume();
        printf("m_rootFiber end");
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }
    for (auto &i : thrs)
    {
        i->join();
    }
}

void Scheduler::tickle()
{
    printf("Scheduler tickle\n");
}

void Scheduler::idle()
{
    printf("Scheduler idle\n");
    while (!stopping())
    {
        Coroutine::GetThis()->yield();
    }
}
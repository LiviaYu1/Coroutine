/**
 * @file thread.cc
 * @brief 线程封装实现
 * @version 0.1
 * @date 2021-06-15
 */

#include "Threads.h"
#include "../util.h"
#include <pthread.h>
#include <mutex>
#include <semaphore.h>

static thread_local Thread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

Thread *Thread::GetThis()
{
    return t_thread;
}

const std::string &Thread::GetName()
{
    return t_thread_name;
}

void Thread::SetName(const std::string &name)
{
    if (name.empty())
    {
        return;
    }
    if (t_thread)
    {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name)
    : m_cb(cb), m_name(name)
{
    if (name.empty())
    {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt)
    {
        printf("pthread_create thread fail, rt= %d name= %s ", rt, m_name);
        throw std::logic_error("pthread_create error");
    }
    m_semaphore.wait();
}

Thread::~Thread()
{
    if (m_thread)
    {
        pthread_detach(m_thread);
    }
}

void Thread::join()
{
    if (m_thread)
    {
        int rt = pthread_join(m_thread, nullptr);
        if (rt)
        {
            printf("pthread_join thread fail, rt= %d name= %s ", rt, m_name);
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void *Thread::run(void *arg)
{
    Thread *thread = (Thread *)arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = ybb::GetThreadId();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);

    thread->m_semaphore.notify();

    cb();
    return 0;
}

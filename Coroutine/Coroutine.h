#ifndef COROUTINE_H
#define COROUTINE_H
#include <memory>
#include <functional>
#include <ucontext.h>

class Coroutine : public std::enable_shared_from_this<Coroutine>
{
public:
    typedef std::shared_ptr<Coroutine> ptr;
    /*
    协程状态：running运行中，ready就绪，term结束运行
    */
    enum State
    {
        // 就绪态，刚创建或者yield之后
        READY,
        // 运行态，resume之后的状态
        RUNNING,
        // 结束态，协程的回调函数执行完后的状态
        TERM
    };

private:
    /*
    @brief:构造函数
    只用于创建第一个协程，即线程主函数对应的协程
    只能由GetThis()方法调用，所以为私有
    */
    Coroutine();

public:
    /*
    @brief:构造函数，用于创建用户协程
    @param1 func 协程入口函数
    @param2 stacksize 栈大小
    @param3 run_in_scheduler 本协程是否参与调度器调度，默认true
    */
    Coroutine(std::function<void()> func, size_t stacksize = 0, bool run_in_scheduler = true);

    /*
    @brief:析构函数
    */
    ~Coroutine();

    /*
    @brief: 重置协程状态和入口函数，复用已经存在的栈空间
    @param1 func
    */
    void reset(std::function<void()> func);

    /*
    @brief: resume，将当前协程切换到执行状态
    @details 当前协程和正在运行的协程进行交换，
    当前协程状态变为running，正在运行的协程状态变成ready
    */
    void resume();

    /*
    @brief yield让出执行权
    @details 当前协程和上次退到后台的协程进行交换，
    当前协程状态变为ready，后台协程变为running
    */
    void yield();

    /*
    @brief 获取协程id
    */
    uint64_t getId() const { return m_id; };

    /*
    @brief 获取协程状态
    */
    State getState() const { return m_state; };

public:
    /*
    @brief 设置当前正在运行的协程，设置线程局部变量t_coroutine的值
    */
    static void SetThis(Coroutine *crt);

    /*
    @brief 返回当前线程正在执行的协程
    @details 如果当前线程还没有创建协程，则创建第一个协程，
    且该协程为当前线程的主协程，[为调度协程，可以调度管理别的线程]不一定
    其他协程结束时，需要切回到主协程，由主协程选择别的协程resume
    @attention 线程如果要创建协程，需要首先执行GetThis函数，以此初始化主协程
    */
    static Coroutine::ptr GetThis();

    /*
    @brief 返回总协程数
    */
    static uint64_t TotalCoroutines();
    /*
    @brief 获取当前协程id
    */
    static uint64_t GetCoroutineId();
    /*
    @brief 协程⼊⼝函数
    */
    static void MainFunc();
   

    // 成员变量
private:
    // 协程id
    uint64_t m_id = 0;
    // 协程栈大小
    uint32_t m_stacksize = 0;
    // 协程栈地址
    void *m_stack = nullptr;
    // 协程状态
    State m_state = READY;
    // 协程上下文
    ucontext_t m_context;
    // 协程执行函数
    std::function<void()> m_func;
    // 协程是否参与调度器调度
    bool m_runInScheduler;
};

#endif // COROUTINE_H
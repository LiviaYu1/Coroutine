#include "Coroutine.h"
#include <atomic>
#include <string.h>
#include <assert.h>
#include <iostream>

// 默认栈大小
#define DEFAULT_STACK_SIZE 1024 * 128

// 全局静态变量，用于生成协程id
static std::atomic<uint64_t> s_coroutine_id{0};
// 全局静态变量，用于统计当前协程数量
static std::atomic<uint64_t> s_coroutine_count{0};

// 线程局部变量，当前线程正在运行的协程
static thread_local Coroutine *thread_coroutine = nullptr;
// 用于保存当前协程库中的主协程，即调度协程
static thread_local Coroutine::ptr main_coroutine = nullptr;

Coroutine::Coroutine()
{
    // 设置当前协程为运行协程，因为改构造函数只用来创建第一个协程，所以状态一定为running
    SetThis(this);
    c_state = RUNNING;

    // 成功时返回0
    // 失败了返回-1
    if (getcontext(&c_context))
    {
        perror("getcontext failed...");
    }

    // 当前协程id和协程数量自增
    ++s_coroutine_count;
    c_id = s_coroutine_id++;

    std::cout << "Coroutine() id : " << c_id << std::endl;
}

/*
    @brief:构造函数，用于创建用户协程
    @param1 func 协程入口函数
    @param2 stacksize 栈大小默认128k
    */
Coroutine::Coroutine(std::function<void()> func, size_t stacksize, bool run_in_scheduler)
    : c_id(s_coroutine_id++), c_func(func), c_runInScheduler(run_in_scheduler)
{
    ++s_coroutine_count;

    // 分配协程栈空间
    c_stacksize = stacksize ? stacksize : DEFAULT_STACK_SIZE;
    c_stack = malloc(c_stacksize);
    if (c_stack == nullptr)
    {
        perror("malloc stack failed...");
    }
    // c_stack = new char[c_stacksize];
    memset(c_stack, 0, c_stacksize);

    // 获取上下文
    if (getcontext(&c_context))
    {
        perror("getcontext failed...");
    }
    // 设置上下文
    c_context.uc_stack.ss_sp = c_stack;
    c_context.uc_stack.ss_size = c_stacksize;
    c_context.uc_link = nullptr;

    makecontext(&c_context, &Coroutine::MainFunc, 0);

    std::cout << "Coroutine() id : " << c_id << std::endl;
}
/*
@brief 析构函数
*/
Coroutine::~Coroutine()
{
    --s_coroutine_count;
    // 主协程由无参构造函数创建，没有对应的栈
    if (c_stack)
    {
        assert(c_state == TERM);
        free(c_stack);
    }
    else // 主协程
    {
        assert(!c_func);
        assert(c_state == RUNNING);
        Coroutine *cur = thread_coroutine;
        if (cur == this)
        {
            SetThis(nullptr);
        }
    }
    std::cout << "~Coroutine() id : " << c_id << std::endl;
}

/*
    @brief 返回当前线程正在执行的协程
    @details 如果当前线程还没有创建协程，则创建第一个协程，
    且该协程为当前线程的主协程，为调度协程，可以调度管理别的线程
    其他协程结束时，需要切回到主协程，由主协程选择别的协程resume
    @attention 线程如果要创建协程，需要首先执行GetThis函数，以此初始化主协程
    */
Coroutine::ptr Coroutine::GetThis()
{
    if (thread_coroutine)
    {
        return thread_coroutine->shared_from_this();
    }
    // 创建第一个协程
    Coroutine::ptr first_coroutine(new Coroutine);
    assert(thread_coroutine == first_coroutine.get());
    main_coroutine = first_coroutine;
    return thread_coroutine->shared_from_this();
}

/*
@brief 设置当前正在运行的协程，设置线程局部变量t_coroutine的值
*/
void Coroutine::SetThis(Coroutine *crt)
{
    thread_coroutine = crt;
}

/*
    @brief: resume，将当前协程切换到执行状态
    @details 当前协程和正在运行的协程进行交换，
    当前协程状态变为running，正在运行的协程状态变成ready
    */

void Coroutine::resume()
{
    // resume的协程不能是终止的或者是正在运行的
    assert(c_state != TERM && c_state != RUNNING);
    // 设置当前协程为this
    SetThis(this);
    c_state = RUNNING;
    // 由于实现的是非对称协程，所有协程只能由主协程进行调控，所以这里保存的上下文是main_coroutine的上下文
    if (swapcontext(&main_coroutine->c_context, &c_context))
    {
        perror("resume swapcontext failed...");
    }
}

/*
    @brief yield让出执行权
    @details 当前协程和上次退到后台的协程进行交换，
    当前协程状态变为ready，后台协程变为running
    */
void Coroutine::yield()
{
    // 运行完成后会自动yield此时为term状态
    assert(c_state == RUNNING || c_state == TERM);
    // 回到主协程
    SetThis(main_coroutine.get());
    if (c_state != TERM)
    {
        c_state = READY;
    }
    // 上下文切换
    if (swapcontext(&c_context, &main_coroutine->c_context))
    {
        perror("yield swapcontext failed...");
    }
}

/*
    @brief 协程⼊⼝函数
    */
void Coroutine::MainFunc()
{
    Coroutine::ptr cur = GetThis();
    assert(cur);

    cur->c_func(); // 这里进行协程的执行，真正的入口函数
    cur->c_func = nullptr;
    cur->c_state = TERM;
    //这里的解释
    /*
    这里为什么要使用裸指针调用yield()而不是使用cur来调用呢？
    如果使用cur的话，指针一直在栈上引用计数一直为1，无法正常进行释放
    */
    auto raw_ptr = cur.get();
    cur.reset();
    /*
    一个函数执行结束后这个ucontext_t会自动根据其中uclink寻找回去的路
    但是作为一个非对称的协程库，uclink其实不用设置（本项目中uclink都是nullptr）
    因为每个协程执行完都要回到主协程，所以直接调用yield()让他切回主协程就行了
    */
    raw_ptr->yield(); 
}
/*
    @brief: 重置协程状态和入口函数，复用已经存在的栈空间
    @param1 func
    */
void Coroutine::reset(std::function<void()> func)
{
    // 主协程是没有栈的，只复用子协程
    assert(c_stack);
    assert(c_state == TERM);
    c_func = func;
    if (getcontext(&c_context))
    {
        perror("reset getcontext failed...");
    }
    c_context.uc_link = nullptr;
    c_context.uc_stack.ss_sp = c_stack;
    c_context.uc_stack.ss_size = c_stacksize;

    makecontext(&c_context, &Coroutine::MainFunc, 0);

    c_state = READY;
}

uint64_t Coroutine::TotalCoroutines()
{

    return s_coroutine_count;
}
uint64_t Coroutine::GetCoroutineId()
{
    if (thread_coroutine)
    {
        return thread_coroutine->getId();
    }
    return 0;
}
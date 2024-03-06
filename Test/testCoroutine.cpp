/**
 * @file test_fiber.cc
 * @brief 协程测试
 * @version 0.1
 * @date 2021-06-15
 */
#include "../Coroutine/Coroutine.h"
#include <string>
#include <vector>


void run_in_fiber2() {
    printf("run_in_fiber2 begin\n");
    printf("run_in_fiber2 end\n");
}

void run_in_fiber() {
    printf("run_in_fiber begin\n");

    printf("before run_in_fiber yield\n");
    Coroutine::GetThis()->yield();
    printf("after run_in_fiber yield\n");

    printf("run_in_fiber end\n");
    // fiber结束之后会自动返回主协程运行
}

void test_fiber() {
    printf("test_fiber begin\n");

    // 初始化线程主协程
    Coroutine::GetThis();

    Coroutine::ptr fiber(new Coroutine(run_in_fiber, 0, false));
    printf("use_count: %d", fiber.use_count() );// 1

    printf("before test_fiber resume\n");
    fiber->resume();
    printf("after test_fiber resume\n");

    /** 
     * 关于fiber智能指针的引用计数为3的说明：
     * 一份在当前函数的fiber指针，一份在MainFunc的cur指针
     * 还有一份在在run_in_fiber的GetThis()结果的临时变量里
     */
    printf("use_count: %d", fiber.use_count() ); // 3

    printf(fiber->getState()); // READY

    printf("before test_fiber resume again\n");
    fiber->resume();
    printf("after test_fiber resume again\n");

    printf("use_count: %d", fiber.use_count() ); // 1
    printf"fiber status: " << fiber->getState(); // TERM

    fiber->reset(run_in_fiber2); // 上一个协程结束之后，复用其栈空间再创建一个新协程
    fiber->resume();

    printf("use_count: %d", fiber.use_count() ); // 1
    printf("test_fiber end\n");
}

int main(int argc, char *argv[]) {
    EnvMgr::GetInstance()->init(argc, argv);
    Config::LoadFromConfDir(EnvMgr::GetInstance()->getConfigPath());

    SetThreadName("main_thread");
    printf"main begin";

    std::vector<Thread::ptr> thrs;
    for (int i = 0; i < 2; i++) {
        thrs.push_back(Thread::ptr(
            new Thread(&test_fiber, "thread_" + std::to_string(i))));
    }

    for (auto i : thrs) {
        i->join();
    }

    printf"main end";
    return 0;
}
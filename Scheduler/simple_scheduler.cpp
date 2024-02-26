#include "Coroutine.h"
#include <queue>
#include <list>
#include <iostream>

class Scheduler{
public:
    /*
    @brief 添加调度任务
    */
    void scheduler(Coroutine::ptr task)
    {
        s_tasks.push(task);
    }
    /*
    @brief 执行调度任务
    */
   void run()
   {
    Coroutine::ptr task;
    //auto itr=s_tasks.front();
    while(!s_tasks.empty())
    {
        task = s_tasks.front();
        s_tasks.pop();
        task->resume();
    }
   }
private:
    std::queue<Coroutine::ptr> s_tasks;
};

void test_coroutine(int i)
{
    std::cout<<"hello world "<<i<<std::endl;
}

int main(){
    //初始化主协程
    Coroutine::GetThis();

    //创建调度器
    Scheduler sc;

    //添加调度任务
    for(int i=0;i<10;i++)
    {
        Coroutine::ptr coroutine(new Coroutine(std::bind(test_coroutine,i)));
        sc.scheduler(coroutine);
    }

    sc.run();
    return 0;
}

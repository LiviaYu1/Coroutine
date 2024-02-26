#include <stdio.h>
#include <ucontext.h>

void func1(void *arg)
{
    puts("1");
    puts("11");
    puts("111");
    puts("1111");

    setcontext((ucontext_t*)arg);
}

void context_test()
{
    char stack[1024 * 128];
    ucontext_t child, main;

    getcontext(&child);//获取当前的上下文
    child.uc_stack.ss_size = sizeof(stack);//指定栈信息
    child.uc_stack.ss_sp = stack;
    child.uc_stack.ss_flags = 0;

    child.uc_link = NULL;//指定child上下文结束后的下一步执行处，child结束后会回到main上下文

    makecontext(&child,(void (*)(void))func1,1,&main);//指定child上下文所要执行的函数

    swapcontext(&main,&child);//切换上下文，从main切换到child，并且保存当前信息到main
    

    puts("main");//回到当前上下文
}

int main()
{
    context_test();

    return 0;
}
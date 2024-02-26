#include <unistd.h>
#include <ucontext.h>
#include <stdio.h>

int main()
{
    ucontext_t context;

    getcontext(&context);
    puts("Hello World");
    sleep(1);
    setcontext(&context);
    return 0;
}
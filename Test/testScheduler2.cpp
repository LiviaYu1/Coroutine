#include "../Scheduler/Scheduler.h"
//#include "../Coroutine/Coroutine.h"
#include "../util.h"
#include <unistd.h>
#include <iostream>

using namespace std;


void test_fiber() {
    static int s_count = 5;
    cout<< "test in fiber s_count=" << s_count <<endl;

    sleep(1);
    if(--s_count >= 0) {
        Scheduler::GetThis()->schedule(&test_fiber, ybb::GetThreadId());
    }
}

int main(int argc, char** argv) {
    cout << "main"<<endl;
    Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    cout << "schedule"<<endl;
    sc.schedule(&test_fiber);
    sc.stop();
    cout<< "over";
    return 0;
}

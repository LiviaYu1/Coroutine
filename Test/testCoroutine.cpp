#include "../Coroutine/Coroutine.h"
#include <iostream>

using namespace std;


void runInCoroutine()
{
    cout<<"RunInCoroutine start"<<endl;
    Coroutine::GetThis()->yield();
    cout<<"RunInCoroutine end"<<endl;
}

int main()
{
    Coroutine::GetThis();
    cout<<"Main start"<<endl;
    Coroutine::ptr crt(new Coroutine(runInCoroutine));
    crt->resume();
    cout<<"Main after resume"<<endl;
    crt->resume();
    cout<<"Main end"<<endl;

}
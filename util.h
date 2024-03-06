#ifndef __UTIL_H__
#define __UTIL_H__

#include "Coroutine/Coroutine.h"
#include <cxxabi.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <iomanip>

namespace ybb{

static pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

static uint64_t GetCoroutineId() {
    return Coroutine::GetCoroutineId();
}
}

#endif

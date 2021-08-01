
#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#include <cstddef>
#include <csetjmp>
#include <bits/types/sigset_t.h>
#include <signal.h>
#include <iostream>

#include "uthreads.h"

#define SYS_ERR_PREFIX "system error: "
#define THREAD_ERR_PREFIX "thread library error: "

typedef void (* threadFunction)(void);

/*
 * Description: An object holding a thread's data, and it's running time
*/
class Thread
{
    size_t _tid;
    char _stack[STACK_SIZE];
    sigjmp_buf _env{};
    int _numQunatums;

public:
    bool mutexBlocked;

    Thread(int tid, threadFunction f);

    Thread();

    int getTid() const;

    int getQuants() const;

    void addQuantum();

    sigjmp_buf* getEnv();

    int setJmp();

};

static sigset_t blockedSignalsSet;

/*
 * Description: changes the state of the mask state of the current thread. If sig is SIG_BLOCK signal masking is
 * activated, if sig is SIG_UNBLOCK it is deactivated.
*/
static void sigChange(int sig)
{
    if (sigprocmask(sig, &blockedSignalsSet, nullptr))
    {
        std::cerr << SYS_ERR_PREFIX << "Signal Mask Couldn't be Changed\n";
        exit(1);
    }
}


#endif //EX2_THREAD_H

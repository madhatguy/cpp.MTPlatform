
#include "Thread.h"
#include <csignal>

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif

// Default constructor for main Thread
Thread::Thread()
{
    sigChange(SIG_BLOCK);
    _tid = 0;
    _numQunatums = 1;
    mutexBlocked = false;
    sigChange(SIG_UNBLOCK);
}


Thread::Thread(int tid, threadFunction f)
{
    sigChange(SIG_BLOCK);
    _tid = tid;
    _numQunatums = 0;
    mutexBlocked = false;
    address_t sp, pc;
    sp = (address_t) _stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t) f;
    if (sigsetjmp(_env, 1) == 0)
    {
        (_env->__jmpbuf)[JB_SP] = translate_address(sp);
        (_env->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&_env->__saved_mask);
    }
    sigChange(SIG_UNBLOCK);
}

int Thread::getQuants() const
{
    return _numQunatums;
}

void Thread::addQuantum()
{
    _numQunatums++;
}

sigjmp_buf* Thread::getEnv()
{
    return &_env;
}

int Thread::setJmp()
{
    return sigsetjmp(_env, 1);
}

int Thread::getTid() const
{
    return _tid;
}

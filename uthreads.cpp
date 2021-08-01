#include <iostream>
#include <unordered_map>
#include <list>
#include <queue>
#include <algorithm>
#include <vector>
#include <csignal>
#include <sys/time.h>
#include "uthreads.h"
#include "Thread.h"

#define ERR_RET -1
#define SUC_RET 0
#define MAIN_TID 0
#define NO_LOCK -1
#define MY_RETURN_FROM_JMP -2

typedef std::priority_queue<int, std::vector<int>, std::greater<int>> TidQueue;
typedef std::unordered_map<int, Thread*> ThreadMap;
typedef std::list<Thread*> ThreadList;
typedef std::_List_iterator<Thread*> ThreadListIter;

// Global variables:
TidQueue freeTids;
std::list<int> mutexBlocked;
ThreadMap blockedThreads;
ThreadList readyList;
int totalQuantums;
int runningTid;
struct sigaction sa;
struct itimerval timer;
int quantumUsecs;
int mutexKing;


/*
 * Description: This function performs a context switch to nextThread, incrementing totalQuantums by 1.
*/
void jumpSeq(Thread* nextThread);


/*
 * Description: This function gets the lowest free thread ID.
*/
int nextFreeTid()
{
    if (freeTids.empty())
    {
        std::cerr << THREAD_ERR_PREFIX << "too many threads you ****\n";
        return ERR_RET;  // I'm full
    }
    int tid = freeTids.top();
    freeTids.pop();
    return tid;
}


/*
 * Description: This function gets an iterator pointing to the thread with the ID tid.
*/
ThreadListIter getReadyByTid(int& tid)
{
    for (ThreadListIter iter = readyList.begin(); iter != readyList.end(); ++iter)
    {
        if ((*iter)->getTid() == tid)
        {
            return iter;
        }
    }
    return readyList.end();
}


/*
 * Description: This function returns 0 if the thread with tid is in a ready state, and -1 otherwise.
*/
int searchNotBlocked(int& tid)
{
    if (tid == runningTid)
    {
        return 1;
    }
    if (getReadyByTid(tid) != readyList.end())
    {
        return 0;
    }
    return -1;
}


/*
 * Description: Does a context switch between the running thread and the thread next in the running queue if sig is
 * SIGVTALRM.
*/
void switchHandler(int sig)
{
    if (sig == SIGVTALRM)
    {
        sigChange(SIG_BLOCK);
        if (sigsetjmp(*(*readyList.front()).getEnv(), 1) != 0)
        {
            return;
        }
        Thread* prev = readyList.front();
        readyList.pop_front();
        readyList.push_back(prev);
        sigChange(SIG_UNBLOCK);
        jumpSeq(readyList.front());
    }

}


/*
 * Description: The signal mask.
*/
void initBlockSet()
{
    if (sigemptyset(&blockedSignalsSet))
    {
        std::cerr << SYS_ERR_PREFIX << "Blocked Signals Set Couldn't be Emptied\n";
        exit(1);
    }
    if (sigaddset(&blockedSignalsSet, SIGVTALRM))
    {
        std::cerr << SYS_ERR_PREFIX << "Blocked Signals Set Couldn't be Initialized\n";
        exit(1);
    }
}


/*
 * Description: Initializes the round robin scheduler timer.
*/
void startTimer()
{
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << SYS_ERR_PREFIX << "setitimer error.\n";
        exit(1);
    }
}


/*
 * Description: Defines the round robin scheduler.
*/
void iniTimer(int& quantumLen)
{
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = switchHandler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << SYS_ERR_PREFIX << "sigaction error\n";
    }


    timer.it_value.tv_sec = (int) (quantumLen / 1e6);        // first time interval, seconds part
    timer.it_value.tv_usec = quantumLen % ((long) 1e6);        // first time interval, microseconds part

    timer.it_interval.tv_sec = (int) (quantumLen / 1e6);    // following time intervals, seconds part
    timer.it_interval.tv_usec = quantumLen % ((long) 1e6);    // following time intervals, microseconds part
    startTimer();
}


/*
 * Description: Returns a pointer to the thread with the ID tid, returns nullptr if such one doesn't exist.
*/
Thread* getThreadByTid(int& tid)
{
    auto inReady = getReadyByTid(tid);
    if (inReady != readyList.end())
    {
        return *inReady;
    }
    // Search in blocked:
    auto inBlocked = blockedThreads.find(tid);
    if (inBlocked != blockedThreads.end())
    {
        return (*inBlocked).second;
    }
    return nullptr;
}


/*
 * Description: Blocks the thread with ID tid. Jumps to the next thread if shouldSetJmp != 0 (i.e. if the blocked thread
 * is the running one).
*/
int blockThread(int& tid, bool shouldSetJmp)
{
    sigChange(SIG_BLOCK);
    if (tid == runningTid)
    {
        Thread* toBlock = readyList.front();
        if (shouldSetJmp)
        {
            if (sigsetjmp(*(*toBlock).getEnv(), 1))
            {
                sigChange(SIG_UNBLOCK);
                return MY_RETURN_FROM_JMP;
            }
        }
        readyList.pop_front();
        blockedThreads[(*toBlock).getTid()] = toBlock;
        startTimer();
        auto next = readyList.front();
        sigChange(SIG_UNBLOCK);
        jumpSeq(next);
    }
    if (blockedThreads.find(tid) != blockedThreads.end())
    {
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    ThreadListIter toBlock = getReadyByTid(tid);
    if (toBlock != readyList.end())
    {
        blockedThreads[tid] = *toBlock;
        readyList.erase(toBlock);
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    sigChange(SIG_UNBLOCK);
    std::cerr << THREAD_ERR_PREFIX << "no such thread\n";
    return ERR_RET;

}


/*
 * Description: This function performs a context switch to nextThread, incrementing totalQuantums by 1.
*/
void jumpSeq(Thread* nextThread)
{
    sigChange(SIG_BLOCK);
    runningTid = (*nextThread).getTid();
    (*nextThread).addQuantum();
    totalQuantums++;
    sigChange(SIG_UNBLOCK);
    siglongjmp(*(*nextThread).getEnv(), 1);
}


// ############################
// Public functions:
// ############################


/*
 * Description: This function initializes the Thread library.
 * You may assume that this function is called before any other Thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        std::cerr << THREAD_ERR_PREFIX << "quantum usecs is non-positive\n";
        return ERR_RET;
    }
    initBlockSet();
    readyList = ThreadList();
    blockedThreads = ThreadMap();
    quantumUsecs = quantum_usecs;
    freeTids = TidQueue();
    mutexBlocked = std::list<int>();
    mutexKing = NO_LOCK;
    for (int i = 1; i < MAX_THREAD_NUM; ++i)
    {
        freeTids.push(i);
    }
    totalQuantums = 1;
    runningTid = 0;
    readyList.push_front(new Thread()); // created main Thread and added it to the ready threads
    iniTimer(quantumUsecs);
    return SUC_RET;
}


/*
 * Description: This function creates a new Thread, whose entry point is the
 * function f with the signature void f(void). The Thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each Thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created Thread.
 * On failure, return -1.
*/
int uthread_spawn(threadFunction f)
{
    sigChange(SIG_BLOCK);
    const int tid = nextFreeTid();
    if (tid == ERR_RET)
    {
        sigChange(SIG_UNBLOCK);
        return ERR_RET;
    }
    readyList.push_back(new Thread(tid, f));
    sigChange(SIG_UNBLOCK);
    return tid;
}


/*
 * Description: This function terminates the Thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this Thread should be released. If no Thread with ID tid
 * exists it is considered an error. Terminating the main Thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the Thread was successfully
 * terminated and -1 otherwise. If a Thread terminates itself or the main
 * Thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    sigChange(SIG_BLOCK);
    if (tid == MAIN_TID)
    {
        Thread* toDelete = getThreadByTid(tid);
        delete toDelete;
        exit(0);
    }
    if (mutexKing == tid)
    {
        mutexKing = NO_LOCK;
        if (!mutexBlocked.empty())
        {
            auto nextMutexCanTid = mutexBlocked.front();
            mutexBlocked.pop_front();
            if (!blockedThreads[nextMutexCanTid]->mutexBlocked)
            {
                uthread_resume(nextMutexCanTid);
            }

        }
    }
    // If it is currently running:
    if (tid == runningTid)
    {
        Thread* toDelete = readyList.front();
        readyList.pop_front();
        freeTids.push(tid);
        delete toDelete;
        startTimer();
        auto next = readyList.front();
        sigChange(SIG_UNBLOCK);
        jumpSeq(next);
    }
    // Search Thread in ready threads:
    ThreadListIter iter = getReadyByTid(tid);
    if (iter != readyList.end())
    {
        Thread* toDelete = getThreadByTid(tid);
        readyList.erase(iter);
        freeTids.push(tid);
        delete toDelete;
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    // Search Thread in existing threads:
    Thread* toDelete = blockedThreads[tid];
    if (toDelete != nullptr)
    {
        blockedThreads.erase(tid);
        freeTids.push(tid);
        delete toDelete;

        // If it is mutex Blocked, remove it from mutes blocked list:
        for (auto it = mutexBlocked.begin(); it != mutexBlocked.end(); it++)
        {
            if (*it == tid)
            {
                mutexBlocked.erase(it);
            }
        }
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    std::cerr << THREAD_ERR_PREFIX << "no such thread\n";
    sigChange(SIG_UNBLOCK);
    return ERR_RET;
}


/*
 * Description: This function blocks the Thread with ID tid. The Thread may
 * be resumed later using uthread_resume. If no Thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main Thread (tid == 0). If a Thread blocks itself, a scheduling decision
 * should be made. Blocking a Thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    if (tid == MAIN_TID)
    {
        std::cerr << THREAD_ERR_PREFIX << "can't block the main thread\n";
        return ERR_RET;
    }
    int result = blockThread(tid, true);
    if (result == MY_RETURN_FROM_JMP)
    {
        return SUC_RET;
    }
    if (!result)
    {
        blockedThreads[tid]->mutexBlocked = true;
    }
    return result;
}


/*
 * Description: This function resumes a blocked Thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a Thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no Thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    sigChange(SIG_BLOCK);
    auto toResume = blockedThreads.find(tid);
    if (toResume != blockedThreads.end())
    {
        // If it is blocked because mutex - deny!
        for (int& it : mutexBlocked)
        {
            if (it == tid)
            {
                std::cout << THREAD_ERR_PREFIX << "Can't resume a Mutex-Blocked thread you\n";
                sigChange(SIG_UNBLOCK);
                return ERR_RET;
            }
        }
        readyList.push_back(toResume->second);
        blockedThreads.erase(toResume);
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    if (searchNotBlocked(tid) < 0)
    {
        sigChange(SIG_UNBLOCK);
        std::cerr << THREAD_ERR_PREFIX << "no such thread\n";
        return ERR_RET;
    }
    sigChange(SIG_UNBLOCK);
    return SUC_RET;
}


/*
 * Description: This function tries to acquire a mutex.
 * If the mutex is unlocked, it locks it and returns.
 * If the mutex is already locked by different Thread, the Thread moves to BLOCK state.
 * In the future when this Thread will be back to RUNNING state,
 * it will try again to acquire the mutex.
 * If the mutex is already locked by this Thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock()
{
    sigsetjmp(*(*readyList.front()).getEnv(), 1);
    sigChange(SIG_BLOCK);
    if (mutexKing == NO_LOCK)
    {
        mutexKing = runningTid;
        sigChange(SIG_UNBLOCK);
        return SUC_RET;
    }
    if (mutexKing == runningTid)
    {
        std::cerr << THREAD_ERR_PREFIX << "thread already has mutex\n";
        sigChange(SIG_UNBLOCK);
        return ERR_RET;
    }
    // Then it should be denied:
    mutexBlocked.push_back(runningTid);
    blockThread(runningTid, false);
    sigChange(SIG_UNBLOCK);
    return SUC_RET;
}


/*
 * Description: This function releases a mutex.
 * If there are blocked threads waiting for this mutex,
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock()
{
    if (mutexKing != runningTid)
    {
        std::cerr << THREAD_ERR_PREFIX << "thread already has mutex\n";
        return ERR_RET;
    }
    sigChange(SIG_BLOCK);
    if (mutexKing == NO_LOCK)
    {
        std::cerr << THREAD_ERR_PREFIX << "thread already has mutex\n";
        sigChange(SIG_UNBLOCK);
        return ERR_RET;
    }
    mutexKing = NO_LOCK;
    if (!mutexBlocked.empty())
    {
        int tidResume = *mutexBlocked.begin();
        mutexBlocked.pop_front();
        // But maybe it has been explicitly blocked!
        if (!blockedThreads[tidResume]->mutexBlocked)
        {
            uthread_resume(tidResume);
        }
    }
    sigChange(SIG_UNBLOCK);
    return SUC_RET;
}


/*
 * Description: This function returns the Thread ID of the calling Thread.
 * Return value: The ID of the calling Thread.
*/
int uthread_get_tid()
{
    return runningTid;
}


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return totalQuantums;
}


/*
 * Description: This function returns the number of quantums the Thread with
 * ID tid was in RUNNING state. On the first time a Thread runs, the function
 * should return 1. Every additional quantum that the Thread starts should
 * increase this value by 1 (so if the Thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * Thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the Thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    auto t = getThreadByTid(tid);
    if (t != nullptr)
    {
        return (*t).getQuants();
    }
    std::cerr << THREAD_ERR_PREFIX << "thread doesn't exist\n";
    return ERR_RET;
}

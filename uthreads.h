#ifndef _UTHREADS_H
#define _UTHREADS_H

/*
 * User-Level Threads Library (uthreads)
 * Author: OS, os@cs.huji.ac.il
 */

#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define STACK_SIZE 4096 /* stack size per Thread (in bytes) */

/* External interface */


/*
 * Description: This function initializes the Thread library.
 * You may assume that this function is called before any other Thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs);

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
int uthread_spawn(void (* f)(void));


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
int uthread_terminate(int tid);


/*
 * Description: This function blocks the Thread with ID tid. The Thread may
 * be resumed later using uthread_resume. If no Thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main Thread (tid == 0). If a Thread blocks itself, a scheduling decision
 * should be made. Blocking a Thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid);


/*
 * Description: This function resumes a blocked Thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a Thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no Thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid);


/*
 * Description: This function tries to acquire a mutex. 
 * If the mutex is unlocked, it locks it and returns. 
 * If the mutex is already locked by different Thread, the Thread moves to BLOCK state.
 * In the future when this Thread will be back to RUNNING state,
 * it will try again to acquire the mutex. 
 * If the mutex is already locked by this Thread, it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_lock();


/*
 * Description: This function releases a mutex. 
 * If there are blocked threads waiting for this mutex, 
 * one of them (no matter which one) moves to READY state.
 * If the mutex is already unlocked, it is considered an error. 
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_mutex_unlock();


/*
 * Description: This function returns the Thread ID of the calling Thread.
 * Return value: The ID of the calling Thread.
*/
int uthread_get_tid();


/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums();


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
int uthread_get_quantums(int tid);

#endif


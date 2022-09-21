/*
*   file:      threading.c
*   brief:     Creates a thread which sleeps a specified amount of milliseconds before acquiring a mutex, then sleeps a specified amount of milliseconds before releasing it
*   author:    Guruprashanth Krishnakumar, gukr5411@colorado.edu
*   date:      09/13/2022
*   refs:      Ch.7 of Linux System Programming by Robert Love, lecture slides of ECEN 5713 - Advanced Embedded Software Dev.
*/

/*
*   HEADER FILES
*/
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)
#define NSEC_PER_MSEC           (1000000)
#define MSEC_PER_SEC            (1000)

/*
*   FUNCTION DEFINITIONs
*/
/*
*  Wait, obtain mutex, wait, release mutex as described by thread_data structure
*  args:
*       thread_params: parameters required for thread execution including the mutex, sleeptimes etc
*  return:
        thread_param
*/
void* threadfunc(void* thread_param)
{

    //Sleep for specified amount of milliseconds, if the sleep is exited earlier due to any reason (like signals), repeat sleep until full sleep time is achieved
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct timespec sleep_time, remaining_time;
    sleep_time.tv_sec = thread_func_args->wait_before_obtain.tv_sec;
    sleep_time.tv_nsec = thread_func_args->wait_before_obtain.tv_nsec;
    int ret;
    do
    {
        ret=nanosleep(&sleep_time, &remaining_time);
        if(ret == 0)
        {
             break;
        }
        sleep_time.tv_sec = remaining_time.tv_sec;
        sleep_time.tv_nsec = remaining_time.tv_nsec;
    }
    while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0)));
    //acquire the mutex
    ret = pthread_mutex_lock(thread_func_args->mutex);
    //repeat sleep
    if(ret != 0)
    {
        //return with failed completion status if there's an issue with the invocation of the mutex_lock function
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }
    sleep_time.tv_sec = thread_func_args->wait_before_release.tv_sec;
    sleep_time.tv_nsec = thread_func_args->wait_before_release.tv_nsec;
    do
    {
        ret=nanosleep(&sleep_time, &remaining_time);
        if(ret == 0)
        {
             break;
        }
        sleep_time.tv_sec = remaining_time.tv_sec;
        sleep_time.tv_nsec = remaining_time.tv_nsec;
    }
    while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0)));
    //unlock the mutex
    ret = pthread_mutex_unlock(thread_func_args->mutex);
    if(ret != 0)
    {
        //return with failed completion status if there's an issue with the invocation of the mutex_unlock function
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }
    //set thread completion status and return
    thread_func_args->thread_complete_success = true;
    return thread_param;
}

/*
*  Allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread using threadfunc() as entry point.
*  args:
*       parameters required for creating the thread
*  return:
        true if thread created successfully, false if not
*/
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    //malloc the thread_data structure
    struct thread_data *thread_param;
    thread_param = malloc(sizeof(struct thread_data));
    if(!thread_param)
    {
        perror("malloc");
        return false;
    }
    //populate with data from args
    thread_param->mutex = mutex;
    thread_param->wait_before_obtain.tv_sec = (wait_to_obtain_ms/MSEC_PER_SEC);
    thread_param->wait_before_obtain.tv_nsec = ((wait_to_obtain_ms%MSEC_PER_SEC)*NSEC_PER_MSEC);
    thread_param->wait_before_release.tv_sec = (wait_to_release_ms/MSEC_PER_SEC);;
    thread_param->wait_before_release.tv_nsec = ((wait_to_release_ms%MSEC_PER_SEC)*NSEC_PER_MSEC);
    //create the thread
    int ret = pthread_create(&thread_param->threadid,
                             (void*)0,
                             threadfunc,
                             thread_param);
    if(ret !=0)
    {
        //return false if thread creation failed
        perror("Pthread Create");
        return false;
    }
    //return true if thread creation succeeded
    *thread = thread_param->threadid;
    return true;
}


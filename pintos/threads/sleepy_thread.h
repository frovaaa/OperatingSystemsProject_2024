#ifndef SLEEPY_THREADS
#define SLEEPY_THREADS

#include "../lib/kernel/list.h"

struct sleepy_thread
{
    struct list_elem elem;

    struct thread *thread_p;

    // awake_time represents the time in ticks of when the thread needs to wake up
    int64_t awake_time;
};

#endif
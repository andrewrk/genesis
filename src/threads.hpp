#ifndef THREADS_HPP
#define THREADS_HPP

#include "util.hpp"
#include "error.h"

#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

class Thread {
public:
    Thread() :
        _run(nullptr)
    {
    }
    ~Thread() {
        join();
    }

    int __attribute__((warn_unused_result)) start(void (*run)(void *), void *userdata) {
        if (_run)
            return GenesisErrorInvalidState;
        _run = run;
        _userdata = userdata;
        if (pthread_create(&_thread_id, NULL, static_start, this)) {
            _run = nullptr;
            _userdata = nullptr;
            return GenesisErrorNoMem;
        }
        if (high_priority) {
            int max_priority = sched_get_priority_max(SCHED_FIFO);
            assert(max_priority != -1);
            sched_param param;
            param.sched_priority = max_priority;
            int err;
            if ((err = pthread_setschedparam(_thread_id, SCHED_FIFO, &param))) {
                fprintf(stderr, "warning: unable to set high priority thread: %s\n", strerror(err));
            }
        }
        return 0;
    }

    void set_high_priority() {
        high_priority = true;
    }

    void join() {
        if (_run) {
            pthread_join(_thread_id, NULL);
            _run = nullptr;
        }
    }

    bool running() const {
        return _run != nullptr;
    }

    static int concurrency() {
        long cpu_core_count = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu_core_count <= 0)
            cpu_core_count = 1;
        return cpu_core_count;
    }
private:
    pthread_t _thread_id;
    void (*_run)(void *userdata);
    void *_userdata;
    bool high_priority;

    static void *static_start(void *userdata) {
        Thread *thread = static_cast<Thread *>(userdata);
        thread->_run(thread->_userdata);
        return nullptr;
    }

    Thread(const Thread &copy) = delete;
    Thread &operator=(const Thread &copy) = delete;
};

class Mutex {
public:
    Mutex() {
        _mutex_initialized = false;
        if (pthread_mutex_init(&_mutex, nullptr)) {
            _initialization_error = GenesisErrorNoMem;
            return;
        }
        _mutex_initialized = true;
        _initialization_error = 0;
    }
    ~Mutex() {
        if (_mutex_initialized)
            pthread_mutex_destroy(&_mutex);
    }

    int error() const {
        return _initialization_error;
    }

    void lock() {
        if (pthread_mutex_lock(&_mutex))
            panic("pthread_mutex_lock failure");
    }

    void unlock() {
        if (pthread_mutex_unlock(&_mutex))
            panic("pthread_mutex_lock failure");
    }

    pthread_mutex_t _mutex;
private:
    int _initialization_error;
    bool _mutex_initialized;

    Mutex(const Mutex &copy) = delete;
    Mutex &operator=(const Mutex &copy) = delete;
};

class MutexCond {
public:
    MutexCond() {
        _condattr_initialized = false;
        _cond_initialized = false;
        _initialization_error = 0;

        if (pthread_condattr_init(&_condattr)) {
            _initialization_error = GenesisErrorNoMem;
            return;
        }
        _condattr_initialized = true;
        if (pthread_condattr_setclock(&_condattr, CLOCK_MONOTONIC)) {
            _initialization_error = GenesisErrorNoMem;
            return;
        }
        if (pthread_cond_init(&_cond, &_condattr)) {
            _initialization_error = GenesisErrorNoMem;
            return;
        }
        _cond_initialized = true;

    }
    ~MutexCond() {
        if (_cond_initialized)
            pthread_cond_destroy(&_cond);
        if (_condattr_initialized)
            pthread_condattr_destroy(&_condattr);
    }

    int error() const {
        return _initialization_error;
    }

    void signal() {
        if (pthread_cond_signal(&_cond))
            panic("pthread_cond_signal failure");
    }

    void signal_broadcast() {
        if (pthread_cond_broadcast(&_cond))
            panic("pthread_cond_broadcast failure");
    }

    void timed_wait(Mutex *mutex, double seconds) {
        struct timespec tms;
        clock_gettime(CLOCK_MONOTONIC, &tms);
        tms.tv_nsec += (seconds * 1000000000L);
        pthread_cond_timedwait(&_cond, &mutex->_mutex, &tms);
    }

    void wait(Mutex *mutex) {
        pthread_cond_wait(&_cond, &mutex->_mutex);
    }

private:
    pthread_cond_t _cond;
    pthread_condattr_t _condattr;
    int _initialization_error;
    bool _cond_initialized;
    bool _condattr_initialized;

    MutexCond(const MutexCond &copy) = delete;
    MutexCond &operator=(const MutexCond &copy) = delete;
};

class MutexLocker {
public:
    MutexLocker(Mutex *_mutex) : mutex(_mutex) {
        mutex->lock();
    }

    ~MutexLocker() {
        mutex->unlock();
    }

    Mutex *mutex;
};

#endif

#ifndef THREADS_HPP
#define THREADS_HPP

#include <pthread.h>

class Thread {
public:
    Thread() :
        _run(nullptr)
    {
    }
    ~Thread() {
        join();
    }

    void start(void (*run)(void *), void *userdata) {
        if (_run)
            panic("already started");
        _run = run;
        _userdata = userdata;
        if (pthread_create(&_thread_id, NULL, static_start, this))
            panic("unable to create thread");
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
private:
    pthread_t _thread_id;
    void (*_run)(void *userdata);
    void *_userdata;

    static void *static_start(void *userdata) {
        Thread *thread = static_cast<Thread *>(userdata);
        thread->_run(thread->_userdata);
        thread->_run = nullptr;
        return nullptr;
    }

    Thread(const Thread &copy) = delete;
    Thread &operator=(const Thread &copy) = delete;
};

class Mutex {
public:
    Mutex() {
        if (pthread_mutex_init(&_mutex, NULL))
            panic("pthread_mutex_init failure");
    }
    ~Mutex() {
        if (pthread_mutex_destroy(&_mutex))
            panic("pthread_mutex_destroy failure");
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

    Mutex(const Mutex &copy) = delete;
    Mutex &operator=(const Mutex &copy) = delete;
};

class MutexCond {
public:
    MutexCond() {
        if (pthread_condattr_init(&_condattr))
            panic("pthread_condattr_init failure");
        if (pthread_condattr_setclock(&_condattr, CLOCK_MONOTONIC))
            panic("pthread_condattr_setclock failure");
        if (pthread_cond_init(&_cond, &_condattr))
            panic("pthread_cond_init failure");
    }
    ~MutexCond() {
        if (pthread_cond_destroy(&_cond))
            panic("pthread_cond_destroy failure");
        if (pthread_condattr_destroy(&_condattr))
            panic("pthread_condattr_destroy failure");
    }

    void signal() {
        if (pthread_cond_signal(&_cond))
            panic("pthread_cond_signal failure\n");
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

    MutexCond(const MutexCond &copy) = delete;
    MutexCond &operator=(const MutexCond &copy) = delete;
};

#endif

#ifndef THREAD_SAFE_QUEUE
#define THREAD_SAFE_QUEUE

#include "error.h"
#include "util.hpp"

#include <linux/futex.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/errno.h>

#include <atomic>
using std::atomic_int;
using std::atomic_flag;
#ifndef ATOMIC_INT_LOCK_FREE
#error "require atomic int to be lock free"
#endif

// if this is true then we can send the address of an atomic int to the futex syscall
static_assert(sizeof(int) == sizeof(atomic_int), "require atomic_int to be same size as int");

static inline int futex(int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3) {
    return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}

static inline int futex_wait(int *address, int val) {
    return futex(address, FUTEX_WAIT, val, nullptr, nullptr, 0) ? errno : 0;
}

static inline int futex_wake(int *address, int count) {
    return futex(address, FUTEX_WAKE, count, nullptr, nullptr, 0) ? errno : 0;
}

// many writer, many reader, fixed size, thread-safe, first-in-first-out queue
// lock-free except when a reader calls dequeue() and queue is empty, then it blocks
// must call resize before you can start using it
// size must be at least equal to the combined number of producers and consumers
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() {
        _items = nullptr;
        _size = 0;
    }
    ~ThreadSafeQueue() {
        destroy(_items, _size);
    }

    // this method not thread safe
    int __attribute__((warn_unused_result)) resize(int size) {
        if (size < 0)
            return GenesisErrorInvalidParam;

        if (size != _size) {
            T *new_items = reallocate_safe(_items, _size, size);
            if (!new_items)
                return GenesisErrorNoMem;

            _items = new_items;
            _size = size;
        }

        _queue_count = 0;
        _read_index = 0;
        _write_index = 0;
        _modulus_flag.clear();

        return 0;
    }

    // put an item on the queue. panics if you attempt to put an item into a
    // full queue. thread-safe.
    void enqueue(T item) {
        int my_write_index = _write_index.fetch_add(1);
        int in_bounds_index = my_write_index % _size;
        _items[in_bounds_index] = item;
        int my_queue_count = _queue_count.fetch_add(1);
        if (my_queue_count >= _size)
            panic("queue is full");
        if (my_queue_count <= 0)
            futex_wake(reinterpret_cast<int*>(&_queue_count), 1);
    }

    // get an item from the queue. blocks if the queue is empty. thread-safe.
    // if the queue has 4 items and 8 threads try to dequeue at the same time,
    // 4 threads will block and 4 threads will return queue items.
    T dequeue() {
outer:
        int my_queue_count = _queue_count.fetch_sub(1);
        if (my_queue_count <= 0) {
            // need to block because there are no items in the queue
            for (;;) {
                int err = futex_wait(reinterpret_cast<int*>(&_queue_count), my_queue_count - 1);
                if (err == EACCES || err == EINVAL || err == ENOSYS) {
                    panic("futex wait error");
                } else {
                    // one of these things happened:
                    //  * waiting failed because _queue_count changed.
                    //  * spurious wakeup
                    //  * normal wakeup
                    // in any case, release the changed state and then try again
                    _queue_count += 1;
                    goto outer;
                }
            }
        }

        int my_read_index = _read_index.fetch_add(1);
        int in_bounds_index = my_read_index % _size;
        // keep the index values in check
        if (my_read_index >= _size && !_modulus_flag.test_and_set()) {
            _read_index -= _size;
            _write_index -= _size;
            _modulus_flag.clear();
        }
        return _items[in_bounds_index];
    }

    // wakes up all blocking dequeue() operations. thread-safe.
    // after you call wakeup_all, the queue is in an invalid state and you
    // must call resize() to fix it. consumer_count is the total number of
    // threads that might call dequeue().
    void wakeup_all(int consumer_count) {
        int my_queue_count = _queue_count.fetch_add(consumer_count);
        int amount_to_wake = -my_queue_count;
        if (amount_to_wake > 0)
            futex_wake(reinterpret_cast<int*>(&_queue_count), amount_to_wake);
    }

private:
    T *_items;
    int _size;
    atomic_int _queue_count;
    atomic_int _read_index;
    atomic_int _write_index;
    atomic_flag _modulus_flag;
};

#endif

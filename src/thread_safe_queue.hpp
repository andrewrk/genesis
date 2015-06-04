#ifndef THREAD_SAFE_QUEUE
#define THREAD_SAFE_QUEUE

#include "error.h"
#include "util.hpp"

#include <linux/futex.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/errno.h>
#include <assert.h>

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
        _allocated_size = 0;
    }
    ~ThreadSafeQueue() {
        destroy(_items, _allocated_size);
    }

    // this method not thread safe
    int __attribute__((warn_unused_result)) resize(int size) {
        if (size < 0)
            return GenesisErrorInvalidParam;

        if (size > _allocated_size) {
            T *new_items = reallocate_safe<T>(_items, _allocated_size, size);
            if (!new_items)
                return GenesisErrorNoMem;

            _allocated_size = size;
            _items = new_items;
        }

        _size = size;
        _queue_count = 0;
        _avail_count = 0;
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
        _queue_count += 1;
        int my_avail_count = _avail_count.fetch_add(1);
        if (my_avail_count < 0) {
            futex_wake(reinterpret_cast<int*>(&_avail_count), _size);
        }
    }

    // get an item from the queue. blocks if the queue is empty. thread-safe.
    // if the queue has 4 items and 8 threads try to dequeue at the same time,
    // 4 threads will block and 4 threads will return queue items.
    T dequeue() {
        for (;;) {
            int my_avail_count = _avail_count.fetch_sub(1);
            int my_queue_count = _queue_count.fetch_sub(1);
            if (my_queue_count > 0)
                break;

            // need to block because there are no items in the queue
            _queue_count += 1;

            int err = futex_wait(reinterpret_cast<int*>(&_avail_count), my_avail_count - 1);
            assert(err != EACCES);
            assert(err != EINVAL);
            assert(err != ENOSYS);

            // one of these things happened:
            //  * waiting failed because _queue_count changed.
            //  * spurious wakeup
            //  * normal wakeup
            _avail_count += 1;
        }

        int my_read_index = _read_index.fetch_add(1);
        int in_bounds_index = my_read_index % _size;
        perform_index_modulus();
        return _items[in_bounds_index];
    }

    // try to get an item from the queue. item is set to the value, or NULL
    // if no item was returned from the queue.
    void try_dequeue(T *item) {
        _avail_count -= 1;
        int my_queue_count = _queue_count.fetch_sub(1);
        if (my_queue_count > 0) {
            int my_read_index = _read_index.fetch_add(1);
            int in_bounds_index = my_read_index % _size;
            perform_index_modulus();
            *item = _items[in_bounds_index];
        } else {
            *item = nullptr;

            _queue_count += 1;
            int my_avail_count = _avail_count.fetch_add(1);
            if (my_avail_count < -1) {
                futex_wake(reinterpret_cast<int*>(&_avail_count), _size);
            }
        }
    }

    // wakes up all blocking dequeue() operations. thread-safe.
    // after you call wakeup_all, the queue is in an invalid state and you
    // must call resize() to fix it.
    void wakeup_all() {
        _queue_count += _size;
        _avail_count += _size;
        futex_wake(reinterpret_cast<int*>(&_avail_count), _size);
    }

private:
    T *_items;
    int _size;
    int _allocated_size;
    atomic_int _queue_count;
    atomic_int _avail_count;
    atomic_int _read_index;
    atomic_int _write_index;
    atomic_flag _modulus_flag;

    void perform_index_modulus() {
        // keep the index values in check
        if (!_modulus_flag.test_and_set()) {
            if (_read_index > _size && _write_index > _size) {
                _read_index -= _size;
                _write_index -= _size;
                assert(_read_index >= 0);
                assert(_write_index >= 0);
            }
            _modulus_flag.clear();
        }
    }
};

#endif

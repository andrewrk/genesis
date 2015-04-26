#ifndef LOCKED_QUEUE_HPP
#define LOCKED_QUEUE_HPP

#include "threads.hpp"
#include "util.hpp"

template<typename T>
class LockedQueue {
public:
    LockedQueue() {
        _start = 0;
        _end = 0;
        _length = 0;
        _capacity = 0;
        _items = NULL;
        _shutdown = false;
    }

    ~LockedQueue() {
        destroy(_items, _capacity);
    }

    int error() const {
        return _mutex.error() || _cond.error();
    }

    // returns 0 on success or GenesisErrorNoMem
    int __attribute__((warn_unused_result)) push(T item) {
        MutexLocker locker(&_mutex);

        int err = ensure_capacity(_length + 1);
        if (err)
            return err;

        _length += 1;
        _items[_end] = item;
        _end = (_end + 1) % _capacity;
        _cond.signal();
        return 0;
    }

    // returns 0 on success or GenesisErrorAborted
    int shift(T *result) {
        MutexLocker locker(&_mutex);

        for (;;) {
            if (_shutdown)
                return GenesisErrorAborted;
            if (_length <= 0) {
                _cond.wait(&_mutex);
                continue;
            }
            _length -= 1;
            *result = _items[_start];
            _start = (_start + 1) % _capacity;
            return 0;
        }
    }

    void wakeup_all() {
        MutexLocker locker(&_mutex);
        _shutdown = true;
        _cond.signal();
    }

    int length() {
        MutexLocker locker(&_mutex);
        return _length;
    }

private:
    T * _items;
    int _start;
    int _end;
    int _length;
    int _capacity;
    Mutex _mutex;
    MutexCond _cond;
    bool _shutdown;

    int ensure_capacity(int new_capacity) {
        int better_capacity = max(_capacity, 16);
        while (better_capacity < new_capacity)
            better_capacity = better_capacity * 2;
        if (better_capacity == _capacity)
            return 0;

        T *new_items = reallocate_safe(_items, _capacity, better_capacity);
        if (!new_items)
            return GenesisErrorNoMem;
        _items = new_items;

        int start_until_capacity = _capacity - _start;
        for (int i = start_until_capacity; i < _length; i += 1) {
            int old_index = (_start + i) % _capacity;
            int new_index = (_start + i) % better_capacity;
            _items[new_index] = _items[old_index];
        }
        _end = (_start + _length) % better_capacity;

        _capacity = better_capacity;
        return 0;
    }

    LockedQueue(const LockedQueue &other) = delete;
    LockedQueue<T>& operator= (const LockedQueue<T> &other) = delete;
};

#endif

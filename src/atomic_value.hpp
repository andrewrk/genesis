#ifndef ATOMIC_VALUE_HPP
#define ATOMIC_VALUE_HPP

#include "atomics.hpp"

// single reader, single writer atomic value

template<typename T>
class AtomicValue {
public:
    AtomicValue() {
        _in_use_index = 0;
        _active_index = 0;
        _write_index = -1;
    }
    ~AtomicValue() {}

    T *write_begin() {
        assert(_write_index == -1);
        int in_use_index = _in_use_index.load();
        int active_index = _active_index.load();
        if (in_use_index != 0 && active_index != 0)
            _write_index = 0;
        else if (in_use_index != 1 && active_index != 1)
            _write_index = 1;
        else
            _write_index = 2;
        return &_values[_write_index];
    }

    void write_end() {
        assert(_write_index != -1);
        _active_index.store(_write_index);
        _write_index = -1;
    }

    T *get_read_ptr() {
        _in_use_index.store(_active_index.load());
        return &_values[_in_use_index];
    }

    T *write(const T &value) {
        T *ptr = write_begin();
        *ptr = value;
        write_end();
        return ptr;
    }

private:
    T _values[3];
    atomic_int _in_use_index;
    atomic_int _active_index;
    int _write_index;

    AtomicValue(const AtomicValue &copy) = delete;
    AtomicValue &operator=(const AtomicValue &copy) = delete;
};

#endif

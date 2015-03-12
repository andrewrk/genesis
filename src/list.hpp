#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

#include <string.h>

template<typename T>
class List {
public:
    List() {
        _length = 0;
        _capacity = 0;
        _items = NULL;
    }
    ~List() {
        destroy(_items, _capacity);
    }
    List(const List &other) {
        resize(other._length);
        for (long i = 0; i < _length; i += 1) {
            _items[i] = other._items[i];
        }
    }
    List<T>& operator= (const List<T> &other) {
        resize(other._length);
        for (long i = 0; i < _length; i += 1) {
            _items[i] = other._items[i];
        }
        return *this;
    }
    inline bool operator==(const List<T> &other) {
        if (_length != other._length)
            return false;

        for (long i = 0; i < _length; i += 1) {
            if (_items[i] != other._items[i])
                return false;
        }

        return true;
    }

    inline bool operator!=(const List<T> &other) {
        return !(*this == other);
    }

    void append(T item) {
        ensure_capacity(_length + 1);
        _items[_length++] = item;
    }
    // remember that the pointer to this item is invalid after you
    // modify the length of the list
    const T & at(long index) const {
        if (index < 0 || index >= _length)
            panic("list: const at index out of bounds");
        return _items[index];
    }
    T & at(long index) {
        if (index < 0 || index >= _length)
            panic("list: at index out of bounds");
        return _items[index];
    }
    long length() const {
        return _length;
    }
    T pop() {
        if (_length == 0)
            panic("pop empty list");
        return unchecked_pop();
    }

    void resize(long length) {
        if (length < 0)
            panic("list: resize negative length");
        ensure_capacity(length);
        _length = length;
    }

    T *raw() const {
        return _items;
    }

    T swap_remove(long index) {
        if (index < 0 || index >= _length)
            panic("list: swap_remove index out of bounds");
        return unchecked_swap_remove(index);
    }

    void remove_range(long start, long end) {
        if (!(0 <= start && start <= end && end <= _length))
            panic("bounds check");
        long del_count = end - start;
        long move_count = min(del_count, _length - end);
        for (long i = start; i < start + move_count; i += 1) {
            _items[i] = _items[i + del_count];
        }
        _length -= del_count;
    }

    void fill(T value) {
        for (long i = 0; i < _length; i += 1) {
            _items[i] = value;
        }
    }

    void clear() {
        _length = 0;
    }

    template<int(*Comparator)(T, T)>
    void sort() {
        insertion_sort<T, Comparator>(_items, _length);
    }

    template<bool(*filter_fn)(void *, T)>
    void filter_with_order_undefined(void *context) {
        for (long i = 0; i < _length;) {
            if (!filter_fn(context, _items[i]))
                unchecked_swap_remove(i);
            else
                i += 1;
        }
    }

    T unchecked_swap_remove(long index) {
        if (index == _length - 1)
            return unchecked_pop();

        T last = unchecked_pop();
        T item = _items[index];
        _items[index] = last;
        return item;
    }

    T unchecked_pop() {
        return _items[--_length];
    }
private:
    T * _items;
    long _length;
    long _capacity;

    void ensure_capacity(long new_capacity) {
        long better_capacity = max(_capacity, 16L);
        while (better_capacity < new_capacity)
            better_capacity = better_capacity * 2;
        if (better_capacity != _capacity) {
            _items = reallocate(_items, _capacity, better_capacity);
            _capacity = better_capacity;
        }
    }
};

#endif

#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"
#include "genesis.h"

#include <assert.h>

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

    bool operator==(const List<T> &other) {
        if (_length != other._length)
            return false;

        for (int i = 0; i < _length; i += 1) {
            if (_items[i] != other._items[i])
                return false;
        }

        return true;
    }

    inline bool operator!=(const List<T> &other) {
        return !(*this == other);
    }

    int __attribute__((warn_unused_result)) append(T item) {
        int err = ensure_capacity(_length + 1);
        if (err)
            return err;
        _items[_length++] = item;
        return 0;
    }
    // remember that the pointer to this item is invalid after you
    // modify the length of the list
    const T & at(int index) const {
        assert(index >= 0);
        assert(index < _length);
        return _items[index];
    }
    T & at(int index) {
        assert(index >= 0);
        assert(index < _length);
        return _items[index];
    }
    int length() const {
        return _length;
    }
    T pop() {
        assert(_length >= 1);
        return unchecked_pop();
    }

    int __attribute__((warn_unused_result)) resize(int length) {
        assert(length >= 0);
        int err = ensure_capacity(length);
        if (err)
            return err;
        _length = length;
        return 0;
    }

    T *raw() const {
        return _items;
    }

    T swap_remove(int index) {
        assert(index >= 0);
        assert(index < _length);
        return unchecked_swap_remove(index);
    }

    void remove_range(int start, int end) {
        assert(0 <= start);
        assert(start <= end);
        assert(end <= _length);
        int del_count = end - start;
        for (int i = start; i < _length - del_count; i += 1) {
            _items[i] = _items[i + del_count];
        }
        _length -= del_count;
    }

    int __attribute__((warn_unused_result)) insert_space(int pos, int size) {
        int old_length = _length;
        assert(pos >= 0 && pos <= old_length);
        int err = resize(old_length + size);
        if (err)
            return err;

        for (int i = old_length - 1; i >= pos; i -= 1) {
            _items[i + size] = _items[i];
        }

        return 0;
    }

    void fill(T value) {
        for (int i = 0; i < _length; i += 1) {
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
        for (int i = 0; i < _length;) {
            if (!filter_fn(context, _items[i]))
                unchecked_swap_remove(i);
            else
                i += 1;
        }
    }

    T unchecked_swap_remove(int index) {
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

    int capacity() const {
        return _capacity;
    }

    int __attribute__((warn_unused_result)) ensure_capacity(int new_capacity) {
        int better_capacity = max(_capacity, 16);
        while (better_capacity < new_capacity)
            better_capacity = better_capacity * 2;
        if (better_capacity != _capacity) {
            T *new_items = reallocate_safe(_items, _capacity, better_capacity);
            if (!new_items)
                return GenesisErrorNoMem;
            _items = new_items;
            _capacity = better_capacity;
        }
        return 0;
    }

    int allocated_size() const {
        return _capacity * sizeof(T);
    }

private:
    T * _items;
    int _length;
    int _capacity;


    List(const List &other) = delete;
    List<T>& operator= (const List<T> &other) = delete;
};

#endif

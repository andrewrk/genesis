#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"
#include "genesis.h"

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
        if (index < 0 || index >= _length)
            panic("list: const at index out of bounds");
        return _items[index];
    }
    T & at(int index) {
        if (index < 0 || index >= _length)
            panic("list: at index out of bounds");
        return _items[index];
    }
    int length() const {
        return _length;
    }
    T pop() {
        if (_length == 0)
            panic("pop empty list");
        return unchecked_pop();
    }

    int __attribute__((warn_unused_result)) resize(int length) {
        if (length < 0)
            panic("list: resize negative length");
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
        if (index < 0 || index >= _length)
            panic("list: swap_remove index out of bounds");
        return unchecked_swap_remove(index);
    }

    void remove_range(int start, int end) {
        if (!(0 <= start && start <= end && end <= _length))
            panic("bounds check");
        int del_count = end - start;
        int move_count = min(del_count, _length - end);
        for (int i = start; i < start + move_count; i += 1) {
            _items[i] = _items[i + del_count];
        }
        _length -= del_count;
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
private:
    T * _items;
    int _length;
    int _capacity;

    int ensure_capacity(int new_capacity) {
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

    List(const List &other) = delete;
    List<T>& operator= (const List<T> &other) = delete;
};

#endif

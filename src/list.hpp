#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

template<typename T>
class List {
public:
    List() {
        _length = 0;
        _capacity = 16;
        _items = allocate<T>(_capacity);
    }
    ~List() {
        free(_items);
    }
    List(List &other) = delete;
    List<T>& operator= (const List<T> &other) {
        resize(other._length);
        for (int i = 0; i < _length; i += 1) {
            _items[i] = other._items[i];
        }
        return *this;
    }
    void append(T item) {
        ensure_capacity(_length + 1);
        _items[_length++] = item;
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
        return _items[--_length];
    }

    void resize(int length) {
        if (length < 0)
            panic("negative resize");
        ensure_capacity(length);
        _length = length;
    }

    T *raw() const {
        return _items;
    }

    T swap_remove(int index) {
        T last = pop();
        if (index >= _length)
            panic("list: swap_remove index out of bounds");
        T item = _items[index];
        _items[index] = last;
        return item;
    }

    void fill(T value) {
        for (int i = 0; i < _length; i += 1) {
            _items[i] = value;
        }
    }

    void clear() {
        _length = 0;
    }

private:
    T * _items;
    int _length;
    int _capacity;

    void ensure_capacity(int new_capacity) {
        int better_capacity = _capacity;
        while (better_capacity < new_capacity)
            better_capacity *= 2;
        if (better_capacity != _capacity) {
            _items = reallocate(_items, better_capacity);
            _capacity = better_capacity;
        }
    }
};

#endif

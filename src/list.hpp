#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

template<typename T>
class List {
public:
    List() {
        _size = 0;
        _capacity = 16;
        _items = allocate<T>(_capacity);
    }
    ~List() {
        free(_items);
    }

    void add(T item) {
        ensure_capacity(_size + 1);
        _items[_size++] = item;
    }
    T & at(int index) {
        if (index < 0 || index >= _size)
            panic("list index out of bounds");
        return _items[index];
    }
    int size() const {
        return _size;
    }
    T pop() {
        if (_size == 0)
            panic("pop empty list");
        return _items[--_size];
    }

private:
    T * _items;
    int _size;
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

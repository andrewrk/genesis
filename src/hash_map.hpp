#ifndef HASH_MAP_HPP
#define HASH_MAP_HPP

#include "util.hpp"

#include <stdint.h>

template<typename K, typename V, uint32_t (*HashFunction)(const K &key)>
class HashMap {
public:
    HashMap() {
        init_capacity(32);
    }
    HashMap(int capacity) {
        init_capacity(capacity);
    }
    ~HashMap() {
        destroy(_entries, _capacity);
    }

    int size() const {
        return _size;
    }

    void put(K key, V value) {
        _modification_count += 1;
        internal_put(key, value);

        // if we get too full (80%), double the capacity
        if (_size * 5 >= _capacity * 4) {
            Entry *old_entries = _entries;
            int old_capacity = _capacity;
            init_capacity(_capacity * 2);
            // dump all of the old elements into the new table
            for (int i = 0; i < old_capacity; i += 1) {
                Entry *old_entry = &old_entries[i];
                if (old_entry->used)
                    internal_put(old_entry->key, old_entry->value);
            }
            destroy(old_entries, old_capacity);
        }
    }

    V get(K key) const {
        Entry *entry = internal_get(key);
        if (!entry)
            panic("key not found");
        return entry->value;
    }

    // if the return value is false, out_value will not be touched
    bool get(K key, V *out_value) const {
        Entry *entry = internal_get(key);
        if (!entry)
            return false;
        *out_value = entry->value;
        return true;
    }

    void remove(K key) {
        _modification_count += 1;
        int start_index = key_to_index(key);
        for (int roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            int index = (start_index + roll_over) % _capacity;
            Entry *entry = &_entries[index];

            if (!entry->used)
                panic("key not found");

            if (entry->key != key)
                continue;

            for (; roll_over < _capacity; roll_over += 1) {
                int next_index = (start_index + roll_over + 1) % _capacity;
                Entry *next_entry = &_entries[next_index];
                if (!next_entry->used || next_entry->distance_from_start_index == 0) {
                    entry->used = false;
                    _size -= 1;
                    return;
                }
                *entry = *next_entry;
                entry->distance_from_start_index -= 1;
                entry = next_entry;
            }
            panic("shifting everything in the table");
        }
        panic("key not found");
    }

    class Iterator {
    public:
        bool has_next() const {
            if (_inital_modification_count != _table->_modification_count)
                panic("concurrent modification");
            return _count < _table->size();
        }
        V next() {
            if (_inital_modification_count != _table->_modification_count)
                panic("concurrent modification");
            for (; _index < _table->_capacity; _index++) {
                Entry * entry = &_table->_entries[_index];
                if (entry->used) {
                    _index++;
                    _count++;
                    return entry->value;
                }
            }
            panic("no next item");
        }
    private:
        HashMap * _table;
        // how many items have we returned
        int _count = 0;
        // iterator through the entry array
        int _index = 0;
        // used to detect concurrent modification
        uint32_t _inital_modification_count;
        Iterator(HashMap * table) :
                _table(table), _inital_modification_count(table->_modification_count) {
        }
        friend HashMap;
    };
    // you must not modify the underlying HashMap while this iterator is still in use
    Iterator value_iterator() {
        return Iterator(this);
    }

private:

    struct Entry {
        bool used;
        int distance_from_start_index;
        K key;
        V value;
    };

    Entry *_entries;
    int _capacity;
    int _size;
    int _max_distance_from_start_index;
    // this is used to detect bugs where a hashtable is edited while an iterator is running.
    uint32_t _modification_count = 0;

    void init_capacity(int capacity) {
        _capacity = capacity;
        _entries = allocate<Entry>(_capacity);
        _size = 0;
        _max_distance_from_start_index = 0;
        for (int i = 0; i < _capacity; i += 1) {
            _entries[i].used = false;
        }
    }

    void internal_put(K key, V value) {
        int start_index = key_to_index(key);
        for (int roll_over = 0, distance_from_start_index = 0;
                roll_over < _capacity; roll_over += 1, distance_from_start_index += 1)
        {
            int index = (start_index + roll_over) % _capacity;
            Entry *entry = &_entries[index];

            if (entry->used && entry->key != key) {
                if (entry->distance_from_start_index < distance_from_start_index) {
                    // robin hood to the rescue
                    Entry tmp = *entry;
                    if (distance_from_start_index > _max_distance_from_start_index)
                        _max_distance_from_start_index = distance_from_start_index;
                    *entry = {
                        true,
                        distance_from_start_index,
                        key,
                        value,
                    };
                    key = tmp.key;
                    value = tmp.value;
                    distance_from_start_index = tmp.distance_from_start_index;
                }
                continue;
            }

            if (!entry->used) {
                // adding an entry. otherwise overwriting old value with
                // same key
                _size += 1;
            }

            if (distance_from_start_index > _max_distance_from_start_index)
                _max_distance_from_start_index = distance_from_start_index;
            *entry = {
                true,
                distance_from_start_index,
                key,
                value,
            };
            return;
        }
        panic("put into a full HashMap");
    }


    Entry *internal_get(K key) const {
        int start_index = key_to_index(key);
        for (int roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            int index = (start_index + roll_over) % _capacity;
            Entry *entry = &_entries[index];

            if (!entry->used)
                return NULL;

            if (entry->key == key)
                return entry;
        }
        return NULL;
    }

    int key_to_index(K key) const {
        return (int)(HashFunction(key) % ((uint32_t)_capacity));
    }
};

#endif

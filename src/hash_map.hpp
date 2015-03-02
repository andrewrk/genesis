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
    HashMap(long capacity) {
        init_capacity(capacity);
    }
    ~HashMap() {
        destroy(_entries, _capacity);
    }
    HashMap(const HashMap &copy) = delete;
    HashMap &operator=(const HashMap &copy) = delete;

    struct Entry {
        bool used;
        long distance_from_start_index;
        K key;
        V value;
    };

    void clear() {
        for (long i = 0; i < _capacity; i += 1) {
            _entries[i].used = false;
        }
        _size = 0;
        _max_distance_from_start_index = 0;
        _modification_count += 1;
    }

    long size() const {
        return _size;
    }

    void put(const K &key, const V &value) {
        _modification_count += 1;
        internal_put(key, value);

        // if we get too full (80%), double the capacity
        if (_size * 5 >= _capacity * 4) {
            Entry *old_entries = _entries;
            long old_capacity = _capacity;
            init_capacity(_capacity * 2);
            // dump all of the old elements into the new table
            for (long i = 0; i < old_capacity; i += 1) {
                Entry *old_entry = &old_entries[i];
                if (old_entry->used)
                    internal_put(old_entry->key, old_entry->value);
            }
            destroy(old_entries, old_capacity);
        }
    }

    const V &get(const K &key) const {
        Entry *entry = internal_get(key);
        if (!entry)
            panic("key not found");
        return entry->value;
    }

    Entry *maybe_get(const K &key) const {
        return internal_get(key);
    }

    void remove(const K &key) {
        _modification_count += 1;
        long start_index = key_to_index(key);
        for (long roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            long index = (start_index + roll_over) % _capacity;
            Entry *entry = &_entries[index];

            if (!entry->used)
                panic("key not found");

            if (entry->key != key)
                continue;

            for (; roll_over < _capacity; roll_over += 1) {
                long next_index = (start_index + roll_over + 1) % _capacity;
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
        Entry *next() {
            if (_inital_modification_count != _table->_modification_count)
                panic("concurrent modification");
            if (_count >= _table->size())
                return NULL;
            for (; _index < _table->_capacity; _index += 1) {
                Entry *entry = &_table->_entries[_index];
                if (entry->used) {
                    _index += 1;
                    _count += 1;
                    return entry;
                }
            }
            panic("no next item");
        }
    private:
        const HashMap * _table;
        // how many items have we returned
        long _count = 0;
        // iterator through the entry array
        long _index = 0;
        // used to detect concurrent modification
        uint32_t _inital_modification_count;
        Iterator(const HashMap * table) :
                _table(table), _inital_modification_count(table->_modification_count) {
        }
        friend HashMap;
    };

    // you must not modify the underlying HashMap while this iterator is still in use
    Iterator entry_iterator() const {
        return Iterator(this);
    }

private:

    Entry *_entries;
    long _capacity;
    long _size;
    long _max_distance_from_start_index;
    // this is used to detect bugs where a hashtable is edited while an iterator is running.
    uint32_t _modification_count = 0;

    void init_capacity(long capacity) {
        _capacity = capacity;
        _entries = allocate<Entry>(_capacity);
        _size = 0;
        _max_distance_from_start_index = 0;
        for (long i = 0; i < _capacity; i += 1) {
            _entries[i].used = false;
        }
    }

    void internal_put(K key, V value) {
        long start_index = key_to_index(key);
        for (long roll_over = 0, distance_from_start_index = 0;
                roll_over < _capacity; roll_over += 1, distance_from_start_index += 1)
        {
            long index = (start_index + roll_over) % _capacity;
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


    Entry *internal_get(const K &key) const {
        long start_index = key_to_index(key);
        for (long roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            long index = (start_index + roll_over) % _capacity;
            Entry *entry = &_entries[index];

            if (!entry->used)
                return NULL;

            if (entry->key == key)
                return entry;
        }
        return NULL;
    }

    long key_to_index(const K &key) const {
        return (long)(HashFunction(key) % ((uint32_t)_capacity));
    }
};

#endif

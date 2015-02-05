#ifndef HASH_MAP_HPP
#define HASH_MAP_HPP

#include "util.hpp"

template<typename K, typename V, uint32_t (*HashFunction)(K)>
class HashMap {
public:
    HashMap() {
        init_capacity(32);
    }
    HashMap(int capacity) {
        init_capacity(capacity);
    }
    ~HashMap() {
        free(_entries);
    }

    int size() const {
        return _size;
    }

    void put(K key, V value) {
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
            free(old_entries);
        }
    }

    V get(K key) const {
        Entry *entry = internal_get(key);
        if (!entry)
            panic("key not found");
        return entry->value;
    }

    V get(K key, V default_value) const {
        Entry *entry = internal_get(key);
        return entry ? entry->value : default_value;
    }

    void remove(K key) {
        int start_index = HashFunction(key) % _capacity;
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
        int start_index = HashFunction(key) % _capacity;
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
        int start_index = HashFunction(key) % _capacity;
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
};

#endif

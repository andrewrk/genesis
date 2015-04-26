#ifndef ORDERED_MAP_FILE_HPP
#define ORDERED_MAP_FILE_HPP

#include "genesis.h"
#include "threads.hpp"
#include "list.hpp"
#include "byte_buffer.hpp"
#include "locked_queue.hpp"
#include "hash_map.hpp"

#include <atomic>
using std::atomic_bool;

struct OrderedMapFile;

struct OrderedMapFileEntry {
    ByteBuffer key;
    int offset;
    int size;
};

struct OrderedMapFileBuffer {
    char *data;
    int size;
};

struct OrderedMapFilePut {
    OrderedMapFileBuffer *key;
    OrderedMapFileBuffer *value;
};

struct OrderedMapFileDel {
    OrderedMapFileBuffer *key;
};

struct OrderedMapFileBatch {
    OrderedMapFile *omf;
    List<OrderedMapFilePut> puts;
    List<OrderedMapFileDel> dels;
};

struct OrderedMapFile {
    Thread write_thread;
    ByteBuffer write_buffer;
    atomic_bool running;
    LockedQueue<OrderedMapFileBatch *> queue;
    FILE *file;
    long transaction_offset;

    // when you open the file, it's in read-only mode and list is populated.
    // when you're done reading, call ordered_map_file_done_reading which
    // sets this property to NULL and the file is in write-only mode
    List<OrderedMapFileEntry *> *list;

    // private
    HashMap<ByteBuffer, OrderedMapFileEntry *, ByteBuffer::hash> *map;
};

int ordered_map_file_open(const char *path, OrderedMapFile **omf);
void ordered_map_file_done_reading(OrderedMapFile *omf);
void ordered_map_file_close(OrderedMapFile *omf);

OrderedMapFileBatch *ordered_map_file_batch_create(OrderedMapFile *omf);
void ordered_map_file_batch_destroy(OrderedMapFileBatch *batch);
// transfers ownership of the batch
int ordered_map_file_batch_exec(OrderedMapFileBatch *batch);

OrderedMapFileBuffer *ordered_map_file_buffer_create(int size);
void ordered_map_file_buffer_destroy(OrderedMapFileBuffer *buffer);
// put and del transfer ownership of the buffers
int ordered_map_file_batch_put(OrderedMapFileBatch *batch, OrderedMapFileBuffer *key, OrderedMapFileBuffer *value);
int ordered_map_file_batch_del(OrderedMapFileBatch *batch, OrderedMapFileBuffer *key);


#endif

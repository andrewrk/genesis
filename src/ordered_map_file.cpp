#include "ordered_map_file.hpp"
#include "crc32.hpp"

static const int UUID_SIZE = 16;
static const char *UUID = "\xca\x2f\x5e\xf5\x00\xd8\xef\x0b\x80\x74\x18\xd0\xe4\x0b\x7a\x4f";

static const int TRANSACTION_METADATA_SIZE = 16;
static const int MAX_TRANSACTION_SIZE = 2147483640;

static int get_transaction_size(OrderedMapFileBatch *batch) {
    int total = TRANSACTION_METADATA_SIZE;
    for (int i = 0; i < batch->puts.length(); i += 1) {
        OrderedMapFilePut *put = &batch->puts.at(i);
        total += 8 + put->key->size + put->value->size;
    }
    for (int i = 0; i < batch->dels.length(); i += 1) {
        OrderedMapFileDel *del = &batch->dels.at(i);
        total += 4 + del->key->size;

    }
    return total;
}

static void run_write(void *userdata) {
    OrderedMapFile *omf = (OrderedMapFile *)userdata;

    for (;;) {
        OrderedMapFileBatch *batch = nullptr;
        omf->queue.shift(&batch);
        if (!batch || !omf->running)
            break;

        // compute transaction size
        int transaction_size = get_transaction_size(batch);
        omf->write_buffer.resize(transaction_size);

        uint8_t *transaction_ptr = (uint8_t*)omf->write_buffer.raw();
        write_uint32be(&transaction_ptr[4], transaction_size);
        write_uint32be(&transaction_ptr[8], batch->puts.length());
        write_uint32be(&transaction_ptr[12], batch->dels.length());

        int offset = TRANSACTION_METADATA_SIZE;
        for (int i = 0; i < batch->puts.length(); i += 1) {
            OrderedMapFilePut *put = &batch->puts.at(i);
            write_uint32be(&transaction_ptr[offset], put->key->size); offset += 4;
            write_uint32be(&transaction_ptr[offset], put->value->size); offset += 4;
            memcpy(&transaction_ptr[offset], put->key->data, put->key->size); offset += put->key->size;
            memcpy(&transaction_ptr[offset], put->value->data, put->value->size); offset += put->value->size;
        }
        for (int i = 0; i < batch->dels.length(); i += 1) {
            OrderedMapFileDel *del = &batch->dels.at(i);
            write_uint32be(&transaction_ptr[offset], del->key->size); offset += 4;
            memcpy(&transaction_ptr[offset], del->key->data, del->key->size); offset += del->key->size;
        }
        assert(offset == transaction_size);

        ordered_map_file_batch_destroy(batch);

        // compute crc32
        write_uint32be(&transaction_ptr[0], crc32(0, &transaction_ptr[4], transaction_size - 4));

        // append to file
        size_t amt_written = fwrite(transaction_ptr, 1, transaction_size, omf->file);
        if (amt_written != (size_t)transaction_size)
            panic("write to disk failed");

        os_mutex_lock(omf->mutex);
        os_cond_signal(omf->cond, omf->mutex);
        os_mutex_unlock(omf->mutex);
    }
}

static int write_header(OrderedMapFile *omf) {
    size_t amt_written = fwrite(UUID, 1, UUID_SIZE, omf->file);
    if (amt_written != UUID_SIZE)
        return GenesisErrorFileAccess;
    return 0;
}

static int read_header(OrderedMapFile *omf) {
    char uuid_buf[UUID_SIZE];
    int amt_read = fread(uuid_buf, 1, UUID_SIZE, omf->file);

    if (amt_read == 0)
        return GenesisErrorEmptyFile;

    if (amt_read != UUID_SIZE)
        return GenesisErrorInvalidFormat;

    if (memcmp(UUID, uuid_buf, UUID_SIZE) != 0)
        return GenesisErrorInvalidFormat;

    return 0;
}

static int compare_entries(OrderedMapFileEntry * a, OrderedMapFileEntry * b) {
    return ByteBuffer::compare(a->key, b->key);
}

static void destroy_map(OrderedMapFile *omf) {
    if (omf->map) {
        auto it = omf->map->entry_iterator();
        for (;;) {
            auto *map_entry = it.next();
            if (!map_entry)
                break;

            OrderedMapFileEntry *omf_entry = map_entry->value;
            destroy(omf_entry, 1);
        }
        destroy(omf->map, 1);
        omf->map = nullptr;
    }
}

int ordered_map_file_open(const char *path, OrderedMapFile **out_omf) {
    *out_omf = nullptr;
    OrderedMapFile *omf = create_zero<OrderedMapFile>();
    if (!omf) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }
    if (!(omf->cond = os_cond_create())) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }
    if (!(omf->mutex = os_mutex_create())) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }
    if (omf->queue.error()) {
        ordered_map_file_close(omf);
        return omf->queue.error();
    }
    omf->list = create_zero<List<OrderedMapFileEntry *>>();
    if (!omf->list) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }

    omf->map = create_zero<HashMap<ByteBuffer, OrderedMapFileEntry *, ByteBuffer::hash>>();
    if (!omf->map) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }

    omf->running = true;
    int err;
    if ((err = os_thread_create(run_write, omf, false, &omf->write_thread))) {
        ordered_map_file_close(omf);
        return err;
    }

    bool open_for_writing = false;
    omf->file = fopen(path, "rb+");
    if (omf->file) {
        int err = read_header(omf);
        if (err == GenesisErrorEmptyFile) {
            open_for_writing = true;
        } else if (err) {
            ordered_map_file_close(omf);
            return err;
        }
    } else {
        open_for_writing = true;
    }
    if (open_for_writing) {
        omf->file = fopen(path, "wb+");
        if (!omf->file) {
            ordered_map_file_close(omf);
            return GenesisErrorFileAccess;
        }
        int err = write_header(omf);
        if (err) {
            ordered_map_file_close(omf);
            return err;
        }
    }

    // read everything into list
    bool partial_transaction = false;
    omf->write_buffer.resize(TRANSACTION_METADATA_SIZE);
    omf->transaction_offset = UUID_SIZE;
    for (;;) {
        size_t amt_read = fread(omf->write_buffer.raw(), 1, TRANSACTION_METADATA_SIZE, omf->file);
        if (amt_read != TRANSACTION_METADATA_SIZE) {
            // partial transaction. ignore it and we're done.
            if (amt_read > 0)
                partial_transaction = true;
            break;
        }
        uint8_t *transaction_ptr = (uint8_t*)omf->write_buffer.raw();
        int transaction_size = read_uint32be(&transaction_ptr[4]);
        if (transaction_size < TRANSACTION_METADATA_SIZE ||
            transaction_size > MAX_TRANSACTION_SIZE)
        {
            // invalid value
            partial_transaction = true;
            break;
        }

        omf->write_buffer.resize(transaction_size);
        transaction_ptr = (uint8_t*)omf->write_buffer.raw();

        size_t amt_to_read = transaction_size - TRANSACTION_METADATA_SIZE;
        amt_read = fread(&transaction_ptr[TRANSACTION_METADATA_SIZE], 1, amt_to_read, omf->file);
        if (amt_read != amt_to_read) {
            // partial transaction. ignore it and we're done.
            partial_transaction = true;
            break;
        }
        uint32_t computed_crc = crc32(0, &transaction_ptr[4], transaction_size - 4);
        uint32_t crc_from_file = read_uint32be(&transaction_ptr[0]);
        if (computed_crc != crc_from_file) {
            // crc check failed. ignore this transaction and we're done.
            partial_transaction = true;
            break;
        }

        int put_count = read_uint32be(&transaction_ptr[8]);
        int del_count = read_uint32be(&transaction_ptr[12]);

        int offset = TRANSACTION_METADATA_SIZE;
        for (int i = 0; i < put_count; i += 1) {
            int key_size = read_uint32be(&transaction_ptr[offset]); offset += 4;
            int val_size = read_uint32be(&transaction_ptr[offset]); offset += 4;

            OrderedMapFileEntry *entry = create_zero<OrderedMapFileEntry>();
            if (!entry) {
                ordered_map_file_close(omf);
                return GenesisErrorNoMem;
            }

            entry->key = ByteBuffer((char*)&transaction_ptr[offset], key_size); offset += key_size;
            entry->offset = omf->transaction_offset + offset;
            entry->size = val_size;
            offset += val_size;

            auto old_hash_entry = omf->map->maybe_get(entry->key);
            if (old_hash_entry) {
                OrderedMapFileEntry *old_entry = old_hash_entry->value;
                destroy(old_entry, 1);
            }

            omf->map->put(entry->key, entry);
        }
        for (int i = 0; i < del_count; i += 1) {
            int key_size = read_uint32be(&transaction_ptr[offset]); offset += 4;
            ByteBuffer key((char*)&transaction_ptr[offset], key_size); offset += key_size;

            auto hash_entry = omf->map->maybe_get(key);
            if (hash_entry) {
                OrderedMapFileEntry *entry = hash_entry->value;
                omf->map->remove(key);
                destroy(entry, 1);
            }
        }

        omf->transaction_offset += transaction_size;

    }

    if (partial_transaction)
        fprintf(stderr, "Warning: Partial transaction found in project file.\n");

    // transfer map to list and sort
    auto it = omf->map->entry_iterator();
    if (omf->list->ensure_capacity(omf->map->size())) {
        ordered_map_file_close(omf);
        return GenesisErrorNoMem;
    }
    for (;;) {
        auto *map_entry = it.next();
        if (!map_entry)
            break;

        ok_or_panic(omf->list->append(map_entry->value));
    }
    omf->map->clear();
    destroy_map(omf);

    omf->list->sort<compare_entries>();

    *out_omf = omf;
    return 0;
}

static void destroy_list(OrderedMapFile *omf) {
    if (omf->list) {
        for (int i = 0; i < omf->list->length(); i += 1) {
            OrderedMapFileEntry *entry = omf->list->at(i);
            destroy(entry, 1);
        }
        destroy(omf->list, 1);
        omf->list = nullptr;
    }
}

void ordered_map_file_close(OrderedMapFile *omf) {
    if (omf) {
        if (omf->mutex && omf->cond && !omf->queue.error()) {
            ordered_map_file_flush(omf);
            omf->running = false;
            omf->queue.wakeup_all();
        }
        os_thread_destroy(omf->write_thread);
        if (omf->file)
            fclose(omf->file);
        destroy_list(omf);
        destroy_map(omf);
        destroy(omf, 1);
    }
}

OrderedMapFileBatch *ordered_map_file_batch_create(OrderedMapFile *omf) {
    OrderedMapFileBatch *batch = create_zero<OrderedMapFileBatch>();
    if (!batch) {
        ordered_map_file_batch_destroy(batch);
        return nullptr;
    }
    batch->omf = omf;
    return batch;
}

void ordered_map_file_batch_destroy(OrderedMapFileBatch *batch) {
    if (batch) {
        for (int i = 0; i < batch->puts.length(); i += 1) {
            OrderedMapFilePut *put = &batch->puts.at(i);
            ordered_map_file_buffer_destroy(put->key);
            ordered_map_file_buffer_destroy(put->value);
        }
        for (int i = 0; i < batch->dels.length(); i += 1) {
            OrderedMapFileDel *del = &batch->dels.at(i);
            ordered_map_file_buffer_destroy(del->key);
        }
        destroy(batch, 1);
    }
}

int ordered_map_file_batch_exec(OrderedMapFileBatch *batch) {
    return batch->omf->queue.push(batch);
}

OrderedMapFileBuffer *ordered_map_file_buffer_create(int size) {
    OrderedMapFileBuffer *buffer = create_zero<OrderedMapFileBuffer>();
    if (!buffer) {
        ordered_map_file_buffer_destroy(buffer);
        return nullptr;
    }

    buffer->size = size;
    buffer->data = allocate_nonzero<char>(size);
    if (!buffer->data) {
        ordered_map_file_buffer_destroy(buffer);
        return nullptr;
    }

    return buffer;
}

void ordered_map_file_buffer_destroy(OrderedMapFileBuffer *buffer) {
    if (buffer) {
        destroy(buffer->data, buffer->size);
        destroy(buffer, 1);
    }
}

int ordered_map_file_batch_put(OrderedMapFileBatch *batch,
        OrderedMapFileBuffer *key, OrderedMapFileBuffer *value)
{
    if (batch->puts.resize(batch->puts.length() + 1))
        return GenesisErrorNoMem;

    OrderedMapFilePut *put = &batch->puts.at(batch->puts.length() - 1);
    put->key = key;
    put->value = value;
    return 0;
}

int ordered_map_file_batch_del(OrderedMapFileBatch *batch,
        OrderedMapFileBuffer *key)
{
    if (batch->dels.resize(batch->dels.length() + 1))
        return GenesisErrorNoMem;

    OrderedMapFileDel *del = &batch->dels.at(batch->dels.length() - 1);
    del->key = key;
    return 0;
}

void ordered_map_file_done_reading(OrderedMapFile *omf) {
    destroy_list(omf);
    if (fseek(omf->file, omf->transaction_offset, SEEK_SET))
        panic("unable to seek in file");
}

template <bool prefix>
static int ordered_map_file_find_key_tpl(OrderedMapFile *omf, const ByteBuffer &key) {
    if (omf->list->length() == 0)
        return -1;

    // binary search
    int start = 0;
    int end = omf->list->length();
    while (start < end) {
        int middle = (start + end) / 2;
        OrderedMapFileEntry *entry = omf->list->at(middle);
        int cmp = prefix ? entry->key.cmp_prefix(key) : ByteBuffer::compare(entry->key, key);
        if (cmp < 0)
            start = middle + 1;
        else
            end = middle;
    }

    if (start != end || start == omf->list->length())
        return -1;

    OrderedMapFileEntry *entry = omf->list->at(start);
    int cmp = prefix ? entry->key.cmp_prefix(key) : ByteBuffer::compare(entry->key, key);
    if (cmp == 0)
        return start;

    return -1;
}

int ordered_map_file_find_prefix(OrderedMapFile *omf, const ByteBuffer &prefix) {
    return ordered_map_file_find_key_tpl<true>(omf, prefix);
}

int ordered_map_file_find_key(OrderedMapFile *omf, const ByteBuffer &key) {
    return ordered_map_file_find_key_tpl<false>(omf, key);
}

int ordered_map_file_get(OrderedMapFile *omf, int index, ByteBuffer **out_key, ByteBuffer &out_value) {
    OrderedMapFileEntry *entry = omf->list->at(index);
    if (out_key)
        *out_key = &entry->key;
    out_value.resize(entry->size);
    if (fseek(omf->file, entry->offset, SEEK_SET))
        return GenesisErrorFileAccess;
    size_t amt_read = fread(out_value.raw(), 1, out_value.length(), omf->file);
    if (amt_read != (size_t)out_value.length())
        return GenesisErrorFileAccess;
    return 0;
}

int ordered_map_file_count(OrderedMapFile *omf) {
    return omf->list->length();
}

void ordered_map_file_flush(OrderedMapFile *omf) {
    {
        OsMutexLocker locker(omf->mutex);
        while (omf->queue.length())
            os_cond_wait(omf->cond, omf->mutex);
    }

    int err;
    if (omf->file) {
        if ((err = os_flush_file(omf->file))) {
            panic("flush file fail: %s", genesis_strerror(err));
        }
    }
}

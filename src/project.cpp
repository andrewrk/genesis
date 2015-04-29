#include "project.hpp"
#include "debug.hpp"

#include <limits.h>

// modifying this structure affects project file backward compatibility
enum PropKey {
    PropKeyInvalid,
    PropKeyDelimiter,
    PropKeyProjectId,
    PropKeyTrack,
    PropKeyCommand,
    PropKeyUser,
    PropKeyUndoCommand,
};

static const int PROP_KEY_SIZE = 4;
static const int UINT256_SIZE = 32;

enum SerializableFieldKey {
    SerializableFieldKeyInvalid,
    SerializableFieldKeyId,
    SerializableFieldKeyUserId,
    SerializableFieldKeyRevision,
    SerializableFieldKeyName,
    SerializableFieldKeySortKey,
    SerializableFieldKeyTrackId,
    SerializableFieldKeyPayload,
    SerializableFieldKeyOtherCommandId,
    SerializableFieldKeyCmdType,
    SerializableFieldKeyCommandChild,
};

enum SerializableFieldType {
    SerializableFieldTypeInvalid,
    SerializableFieldTypeUInt32,
    SerializableFieldTypeUInt32AsInt,
    SerializableFieldTypeString,
    SerializableFieldTypeByteBuffer,
    SerializableFieldTypeUInt256,
    SerializableFieldTypeSortKey,
    SerializableFieldTypeCmdType,
    SerializableFieldTypeCommandChild,
};

template <typename T>
struct SerializableField {
    SerializableFieldKey key;
    SerializableFieldType type;
    void *(*get_field_ptr)(T *);
    void (*set_default_value)(T *);
};

static int deserialize_from_enum(void *ptr, SerializableFieldType type, const ByteBuffer &buffer, int *offset);

static const SerializableField<Track> *get_serializable_fields(Track *) {
    static const SerializableField<Track> fields[] = {
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](Track *track) -> void * {
                return &track->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeString,
            [](Track *track) -> void * {
                return &track->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<User> *get_serializable_fields(User *) {
    static const SerializableField<User> fields[] = {
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](User *user) -> void * {
                return &user->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<Command> *get_serializable_fields(Command *) {
    static const SerializableField<Command> fields[] = {
        {
            SerializableFieldKeyCmdType,
            SerializableFieldTypeCmdType,
            [](Command *cmd) -> void * {
                return cmd;
            },
            nullptr,
        },
        {
            SerializableFieldKeyId,
            SerializableFieldTypeUInt256,
            [](Command *cmd) -> void * {
                return &cmd->id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyUserId,
            SerializableFieldTypeUInt256,
            [](Command *cmd) -> void * {
                return &cmd->user_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyRevision,
            SerializableFieldTypeUInt32AsInt,
            [](Command *cmd) -> void * {
                return &cmd->revision;
            },
            nullptr,
        },
        {
            SerializableFieldKeyCommandChild,
            SerializableFieldTypeCommandChild,
            [](Command *cmd) -> void * {
                return cmd;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AddTrackCommand> *get_serializable_fields(AddTrackCommand *) {
    static const SerializableField<AddTrackCommand> fields[] = {
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeSortKey,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<DeleteTrackCommand> *get_serializable_fields(DeleteTrackCommand *) {
    static const SerializableField<DeleteTrackCommand> fields[] = {
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](DeleteTrackCommand *cmd) -> void * {
                return &cmd->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyPayload,
            SerializableFieldTypeByteBuffer,
            [](DeleteTrackCommand *cmd) -> void * {
                return &cmd->payload;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<UndoCommand> *get_serializable_fields(UndoCommand *) {
    static const SerializableField<UndoCommand> fields[] = {
        {
            SerializableFieldKeyOtherCommandId,
            SerializableFieldTypeUInt256,
            [](UndoCommand *cmd) -> void * {
                return &cmd->other_command_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<RedoCommand> *get_serializable_fields(RedoCommand *) {
    static const SerializableField<RedoCommand> fields[] = {
        {
            SerializableFieldKeyOtherCommandId,
            SerializableFieldTypeUInt256,
            [](RedoCommand *cmd) -> void * {
                return &cmd->other_command_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static void serialize_uint256(ByteBuffer &buf, const uint256 &val) {
    buf.resize(buf.length() + UINT256_SIZE);
    val.write_be(buf.raw() + buf.length() - UINT256_SIZE);
}

static void serialize_from_enum(void *ptr, SerializableFieldType type, ByteBuffer &buffer) {
    switch (type) {
    case SerializableFieldTypeInvalid:
        panic("invalid serialize field type");
    case SerializableFieldTypeUInt32:
        {
            uint32_t *value = static_cast<uint32_t *>(ptr);
            buffer.append_uint32be(*value);
            break;
        }
    case SerializableFieldTypeUInt32AsInt:
        {
            int *value = static_cast<int *>(ptr);
            buffer.append_uint32be(*value);
            break;
        }
    case SerializableFieldTypeUInt256:
        {
            uint256 *value = static_cast<uint256 *>(ptr);
            serialize_uint256(buffer, *value);
            break;
        }
    case SerializableFieldTypeByteBuffer:
        {
            ByteBuffer *value = static_cast<ByteBuffer *>(ptr);
            buffer.append_uint32be(value->length());
            buffer.append(*value);
            break;
        }
    case SerializableFieldTypeString:
        {
            String *value = static_cast<String *>(ptr);
            ByteBuffer encoded = value->encode();
            buffer.append_uint32be(encoded.length());
            buffer.append(encoded);
            break;
        }
    case SerializableFieldTypeSortKey:
        {
            SortKey *value = static_cast<SortKey *>(ptr);
            value->serialize(buffer);
            break;
        }
    case SerializableFieldTypeCommandChild:
        {
            Command *value = static_cast<Command *>(ptr);
            value->serialize(buffer);
            break;
        }
    case SerializableFieldTypeCmdType:
        {
            Command *value = static_cast<Command *>(ptr);
            buffer.append_uint32be(value->command_type());
            break;
        }
    }
}

template<typename T>
static void serialize_object(T *obj, ByteBuffer &buffer) {
    const SerializableField<T> *serializable_fields = get_serializable_fields(obj);

    // iterate once to count the fields
    int field_count = 0;
    const SerializableField<T> *it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        assert(it->get_field_ptr);
        field_count += 1;
        it += 1;
    }

    int start_offset = buffer.length();
    buffer.append_uint32be(field_count);

    // iterate again and serialize everything
    it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        int field_length_offset = buffer.length();
        buffer.resize(buffer.length() + 4);

        buffer.append_uint32be(it->key);
        serialize_from_enum(it->get_field_ptr(obj), it->type, buffer);

        int field_size = buffer.length() - start_offset;
        write_uint32be(buffer.raw() + field_length_offset, field_size);

        it += 1;
    }
}

static int deserialize_uint32be(uint32_t *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    *x = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    return 0;
}

static int deserialize_uint32be_as_int(int *x, const ByteBuffer &buffer, int *offset) {
    uint32_t unsigned_x;
    int err;
    if ((err = deserialize_uint32be(&unsigned_x, buffer, offset))) return err;
    if (unsigned_x > (uint32_t)INT_MAX) return GenesisErrorInvalidFormat;
    *x = unsigned_x;
    return 0;
}

static int deserialize_byte_buffer(ByteBuffer &out, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    int len = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    if (buffer.length() - *offset < len)
        return GenesisErrorInvalidFormat;

    out.clear();
    out.append(buffer.raw() + *offset, len);
    *offset += len;

    return 0;
}

static int deserialize_string(String &out, const ByteBuffer &buffer, int *offset) {
    ByteBuffer encoded;
    int err;
    if ((err = deserialize_byte_buffer(encoded, buffer, offset))) return err;

    bool ok;
    out = String(encoded, &ok);
    if (!ok)
        return GenesisErrorInvalidFormat;

    return 0;
}

static int deserialize_uint256(uint256 *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < UINT256_SIZE)
        return GenesisErrorInvalidFormat;

    *x = uint256::read_be(buffer.raw() + *offset);
    *offset += UINT256_SIZE;
    return 0;
}

template<typename T>
static int deserialize_object(T *obj, const ByteBuffer &buffer, int *offset) {
    const SerializableField<T> *serializable_fields = get_serializable_fields(obj);

    int err;
    struct Item {
        bool found;
        const SerializableField<T> *field;
    };
    List<Item> items;
    const SerializableField<T> *it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        if (items.append({false, it}))
            return GenesisErrorNoMem;
        it += 1;
    }

    int field_count;
    if ((err = deserialize_uint32be_as_int(&field_count, buffer, offset))) return err;
    for (int field_i = 0; field_i < field_count; field_i += 1) {
        int field_offset = *offset;

        int field_size;
        if ((err = deserialize_uint32be_as_int(&field_size, buffer, offset))) return err;

        int field_key;
        if ((err = deserialize_uint32be_as_int(&field_key, buffer, offset))) return err;

        bool found_this_field = false;
        for (int item_i = 0; item_i < items.length(); item_i += 1) {
            Item *item = &items.at(item_i);
            if (item->field->key == field_key) {
                found_this_field = true;
                item->found = true;
                if ((err = deserialize_from_enum(item->field->get_field_ptr(obj), item->field->type, buffer, offset)))
                    return err;
                break;
            }
        }
        if (!found_this_field) {
            // skip this field
            *offset = field_offset + field_size;
        }
    }

    // call default callbacks on unfound fields
    for (int item_i = 0; item_i < items.length(); item_i += 1) {
        Item *item = &items.at(item_i);
        if (!item->found) {
            if (!item->field->set_default_value)
                return GenesisErrorInvalidFormat;
            item->field->set_default_value(obj);
        }
    }

    return 0;
}

static int deserialize_from_enum(void *ptr, SerializableFieldType type, const ByteBuffer &buffer, int *offset) {
    switch (type) {
    case SerializableFieldTypeInvalid:
        panic("invalid serialize field type");
    case SerializableFieldTypeUInt32:
        {
            uint32_t *value = static_cast<uint32_t *>(ptr);
            return deserialize_uint32be(value, buffer, offset);
        }
    case SerializableFieldTypeUInt32AsInt:
        {
            int *value = static_cast<int *>(ptr);
            return deserialize_uint32be_as_int(value, buffer, offset);
        }
    case SerializableFieldTypeUInt256:
        {
            uint256 *value = static_cast<uint256 *>(ptr);
            return deserialize_uint256(value, buffer, offset);
        }
    case SerializableFieldTypeByteBuffer:
        {
            ByteBuffer *value = static_cast<ByteBuffer *>(ptr);
            return deserialize_byte_buffer(*value, buffer, offset);
        }
    case SerializableFieldTypeString:
        {
            String *value = static_cast<String *>(ptr);
            return deserialize_string(*value, buffer, offset);
        }
    case SerializableFieldTypeSortKey:
        {
            SortKey *value = static_cast<SortKey *>(ptr);
            return value->deserialize(buffer, offset);
        }
    case SerializableFieldTypeCommandChild:
        {
            Command *cmd = static_cast<Command *>(ptr);
            switch (cmd->command_type()) {
                case CommandTypeInvalid:
                    panic("invalid command type");
                case CommandTypeUndo:
                    return deserialize_object(reinterpret_cast<UndoCommand*>(cmd), buffer, offset);
                case CommandTypeRedo:
                    return deserialize_object(reinterpret_cast<RedoCommand*>(cmd), buffer, offset);
                case CommandTypeAddTrack:
                    return deserialize_object(reinterpret_cast<AddTrackCommand*>(cmd), buffer, offset);
                case CommandTypeDeleteTrack:
                    return deserialize_object(reinterpret_cast<DeleteTrackCommand*>(cmd), buffer, offset);
            }
            panic("unreachable");
        }
    case SerializableFieldTypeCmdType:
        // ignore
        return 0;
    }
    panic("unreachable");
}


static OrderedMapFileBuffer *byte_buffer_to_omf_buffer(const ByteBuffer &byte_buffer) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(byte_buffer.length());
    memcpy(buf->data, byte_buffer.raw(), byte_buffer.length());
    return buf;
}

template<typename T>
static OrderedMapFileBuffer *obj_to_omf_buf(T *obj) {
    ByteBuffer buffer;
    serialize_object(obj, buffer);
    return byte_buffer_to_omf_buffer(buffer);
}

static int compare_tracks(Track *a, Track *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? uint256::compare(a->id, b->id) : sort_key_cmp;
}

static int compare_users(User *a, User *b) {
    return uint256::compare(a->id, b->id);
}

static int compare_commands(Command *a, Command *b) {
    if (a->revision > b->revision)
        return 1;
    else if (a->revision < b->revision)
        return -1;
    else
        return uint256::compare(a->id, b->id);
}

template<typename T, int (*compare)(T, T)>
static void project_sort_item(List<T> &list, IdMap<T> &id_map) {
    list.clear();
    auto it = id_map.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        ok_or_panic(list.append(entry->value));
    }
    quick_sort<T, compare>(list.raw(), list.length());
}

static void project_sort_tracks(Project *project) {
    project_sort_item<Track *, compare_tracks>(project->track_list, project->tracks);
}

static void project_sort_users(Project *project) {
    project_sort_item<User *, compare_users>(project->user_list, project->users);
}

static void project_sort_commands(Project *project) {
    project_sort_item<Command *, compare_commands>(project->command_list, project->commands);
}

static void project_compute_indexes(Project *project) {
    if (project->track_list_dirty) {
        project->track_list_dirty = false;
        project_sort_tracks(project);
    }
    if (project->user_list_dirty) {
        project->user_list_dirty = false;
        project_sort_users(project);
    }
    if (project->command_list_dirty) {
        project->command_list_dirty = false;
        project_sort_commands(project);
    }
}

int project_get_next_revision(Project *project) {
    project_compute_indexes(project);

    if (project->command_list.length() == 0)
        return 0;

    Command *last_command = project->command_list.at(project->command_list.length() - 1);
    return last_command->revision + 1;
}

static OrderedMapFileBuffer *create_id_key(PropKey prop_key, const uint256 &id) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(PROP_KEY_SIZE + PROP_KEY_SIZE + UINT256_SIZE);
    if (!buf)
        panic("out of memory");
    write_uint32be(&buf->data[0], prop_key);
    write_uint32be(&buf->data[4], PropKeyDelimiter);
    id.write_be(&buf->data[8]);
    return buf;
}

static OrderedMapFileBuffer *create_user_key(const uint256 &id) {
    return create_id_key(PropKeyUser, id);
}

static OrderedMapFileBuffer *create_track_key(const uint256 &id) {
    return create_id_key(PropKeyTrack, id);
}

static OrderedMapFileBuffer *create_command_key(Command *command) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(PROP_KEY_SIZE + PROP_KEY_SIZE + 4);
    if (!buf)
        panic("out of memory");
    write_uint32be(&buf->data[0], PropKeyCommand);
    write_uint32be(&buf->data[4], PropKeyDelimiter);
    write_uint32be(&buf->data[8], command->revision);
    return buf;
}

static OrderedMapFileBuffer *create_basic_key(PropKey prop_key) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(4);
    if (!buf)
        panic("out of memory");
    write_uint32be(buf->data, prop_key);
    return buf;
}

static int deserialize_track(Project *project, const uint256 &id, const ByteBuffer &value) {
    Track *track = create_zero<Track>();
    if (!track)
        return GenesisErrorNoMem;

    track->id = id;

    int err;
    int offset = 0;
    if ((err = deserialize_object(track, value, &offset))) {
        destroy(track, 1);
        return err;
    }

    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    return 0;

}

static int deserialize_user(Project *project, const uint256 &id, const ByteBuffer &value) {
    User *user = create_zero<User>();
    if (!user)
        return GenesisErrorNoMem;

    user->id = id;

    int err;
    int offset = 0;
    if ((err = deserialize_object(user, value, &offset))) {
        destroy(user, 1);
        return err;
    }

    project->users.put(user->id, user);
    project->user_list_dirty = true;

    return 0;
}

static int deserialize_command(Project *project, const uint256 &id, const ByteBuffer &buffer) {
    int offset_data = 0;
    int *offset = &offset_data;
    int err;

    // look for the command type field first
    int field_count;
    if ((err = deserialize_uint32be_as_int(&field_count, buffer, offset))) return err;

    int fields_start_offset = *offset;
    int cmd_type = -1;
    for (int field_i = 0; field_i < field_count; field_i += 1) {
        int field_offset = *offset;

        int field_size;
        if ((err = deserialize_uint32be_as_int(&field_size, buffer, offset))) return err;

        int field_key;
        if ((err = deserialize_uint32be_as_int(&field_key, buffer, offset))) return err;

        if (field_key == SerializableFieldKeyCmdType) {
            if ((err = deserialize_uint32be_as_int(&cmd_type, buffer, offset))) return err;
            break;
        } else {
            *offset = field_offset + field_size;
        }
    }
    if (cmd_type == -1)
        return GenesisErrorInvalidFormat;

    *offset = fields_start_offset;

    Command *command = nullptr;
    switch ((CommandType)cmd_type) {
        case CommandTypeInvalid:
            return GenesisErrorInvalidFormat;
        case CommandTypeAddTrack:
            command = create_zero<AddTrackCommand>();
            break;
        case CommandTypeDeleteTrack:
            command = create_zero<DeleteTrackCommand>();
            break;
        case CommandTypeUndo:
            command = create_zero<UndoCommand>();
            break;
        case CommandTypeRedo:
            command = create_zero<RedoCommand>();
            break;
    }
    if (!command)
        return GenesisErrorNoMem;

    command->project = project;
    command->id = id;

    if ((err = deserialize_object(command, buffer, offset))) {
        destroy(command, 1);
        return err;
    }

    auto entry = project->users.maybe_get(command->user_id);
    if (!entry)
        return GenesisErrorInvalidFormat;
    command->user = entry->value;

    project->commands.put(command->id, command);
    project->command_list_dirty = true;

    return 0;
}

static int read_scalar_uint256(Project *project, PropKey prop_key, uint256 *out_value) {
    ByteBuffer buf;
    buf.resize(4);
    write_uint32be((uint8_t*)buf.raw(), prop_key);
    int key_index = ordered_map_file_find_key(project->omf, buf);
    if (key_index == -1)
        return GenesisErrorKeyNotFound;
    int err = ordered_map_file_get(project->omf, key_index, nullptr, buf);
    if (err)
        return err;
    if (buf.length() != UINT256_SIZE)
        return GenesisErrorInvalidFormat;
    *out_value = uint256::read_be(buf.raw());
    return 0;
}

static void put_uint256(OrderedMapFileBatch *batch, OrderedMapFileBuffer *key, const uint256 &x) {
    OrderedMapFileBuffer *value = ordered_map_file_buffer_create(UINT256_SIZE);
    if (!value)
        panic("out of memory");
    x.write_be(value->data);
    ordered_map_file_batch_put(batch, key, value);
}

static int iterate_thing(Project *project, PropKey prop_key,
        int (*got_one)(Project *, const uint256 &, const ByteBuffer &))
{
    ByteBuffer key_buf;
    key_buf.append_uint32be(PropKeyTrack);
    key_buf.append_uint32be(PropKeyDelimiter);
    int index = ordered_map_file_find_prefix(project->omf, key_buf);
    int key_count = ordered_map_file_count(project->omf);

    ByteBuffer *key;
    ByteBuffer value;
    while (index >= 0 && index < key_count) {
        int err = ordered_map_file_get(project->omf, index, &key, value);
        if (err)
            return err;

        if (key->cmp_prefix(key_buf) != 0)
            break;

        if (key->length() != PROP_KEY_SIZE + PROP_KEY_SIZE + UINT256_SIZE)
            return GenesisErrorInvalidFormat;

        uint256 id = uint256::read_be(key->raw() + 8);

        err = got_one(project, id, value);
        if (err)
            return err;

        index += 1;
    }

    return 0;
}

int project_open(const char *path, User *user, Project **out_project) {
    *out_project = nullptr;

    Project *project = create_zero<Project>();
    if (!project) {
        project_close(project);
        return GenesisErrorNoMem;
    }

    int err = ordered_map_file_open(path, &project->omf);
    if (err) {
        project_close(project);
        return err;
    }

    err = read_scalar_uint256(project, PropKeyProjectId, &project->id);
    if (err) {
        project_close(project);
        return err;
    }

    // read tracks
    err = iterate_thing(project, PropKeyTrack, deserialize_track);
    if (err) {
        project_close(project);
        return err;
    }

    // read users
    err = iterate_thing(project, PropKeyUser, deserialize_user);
    if (err) {
        project_close(project);
        return err;
    }

    // read command history (depends on users)
    err = iterate_thing(project, PropKeyCommand, deserialize_command);
    if (err) {
        project_close(project);
        return err;
    }

    project_compute_indexes(project);
    ordered_map_file_done_reading(project->omf);

    *out_project = project;
    return 0;
}

int project_create(const char *path, const uint256 &id, User *user, Project **out_project) {
    *out_project = nullptr;
    Project *project = create_zero<Project>();
    if (!project) {
        project_close(project);
        return GenesisErrorNoMem;
    }

    int err = ordered_map_file_open(path, &project->omf);
    if (err) {
        project_close(project);
        return err;
    }
    ordered_map_file_done_reading(project->omf);

    project->id = id;
    project->active_user = user;

    OrderedMapFileBatch *batch = ordered_map_file_batch_create(project->omf);
    put_uint256(batch, create_basic_key(PropKeyProjectId), project->id);
    ordered_map_file_batch_put(batch, create_user_key(user->id), obj_to_omf_buf(user));
    project_insert_track_batch(project, batch, nullptr, nullptr);
    err = ordered_map_file_batch_exec(batch);
    if (err) {
        project_close(project);
        return err;
    }
    project_compute_indexes(project);

    *out_project = project;
    return 0;
}

void project_close(Project *project) {
    if (project) {
        ordered_map_file_close(project->omf);
        destroy(project, 1);
    }
}

static void project_push_command(Project *project, Command *command) {
    ok_or_panic(project->command_list.append(command));
    project->commands.put(command->id, command);
}

void project_perform_command(Project *project, Command *command) {
    OrderedMapFileBatch *batch = ordered_map_file_batch_create(project->omf);
    project_perform_command_batch(project, batch, command);
    ok_or_panic(ordered_map_file_batch_exec(batch));

    project_push_command(project, command);
    project_compute_indexes(project);
}

void project_perform_command_batch(Project *project, OrderedMapFileBatch *batch, Command *command) {
    assert(project);
    assert(batch);
    assert(command);

    int next_revision = project_get_next_revision(project);
    if (next_revision != command->revision)
        panic("unimplemented: multi-user editing merge conflict");

    ordered_map_file_batch_put(batch, create_command_key(command), obj_to_omf_buf(command));
    command->redo(batch);
}

void project_insert_track(Project *project, const Track *before, const Track *after) {
    OrderedMapFileBatch *batch = ordered_map_file_batch_create(project->omf);
    AddTrackCommand *command = project_insert_track_batch(project, batch, before, after);
    ok_or_panic(ordered_map_file_batch_exec(batch));

    project_push_command(project, command);
    project_compute_indexes(project);
}

AddTrackCommand *project_insert_track_batch(Project *project, OrderedMapFileBatch *batch,
        const Track *before, const Track *after)
{
    const SortKey *low_sort_key = before ? &before->sort_key : nullptr;
    const SortKey *high_sort_key = after ? &after->sort_key : nullptr;
    SortKey sort_key = SortKey::single(low_sort_key, high_sort_key);

    AddTrackCommand *add_track = create<AddTrackCommand>(project, "Untitled Track", sort_key);
    project_perform_command_batch(project, batch, add_track);

    return add_track;
}

void project_delete_track(Project *project, Track *track) {
    DeleteTrackCommand *delete_track = create<DeleteTrackCommand>(project, track);
    project_perform_command(project, delete_track);
}

User *user_create(const uint256 &id, const String &name) {
    User *user = create_zero<User>();
    if (!user)
        panic("out of memory");

    user->id = id;
    user->name = name;

    return user;
}

void user_destroy(User *user) {
    destroy(user, 1);
}

AddTrackCommand::AddTrackCommand(Project *project, String name, const SortKey &sort_key) :
    Command(project),
    name(name),
    sort_key(sort_key)
{
    track_id = uint256::random();
}

void AddTrackCommand::undo(OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));

    destroy(track, 1);
}

void AddTrackCommand::redo(OrderedMapFileBatch *batch) {
    Track *track = create<Track>();
    track->id = track_id;
    track->name = name;
    track->sort_key = sort_key;
    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    ordered_map_file_batch_put(batch, create_track_key(track_id), obj_to_omf_buf(track));
}

void AddTrackCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int AddTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

DeleteTrackCommand::DeleteTrackCommand(Project *project, Track *track) :
    Command(project),
    track_id(track->id)
{
    serialize_object(track, payload);
}

void DeleteTrackCommand::undo(OrderedMapFileBatch *batch) {
    ok_or_panic(deserialize_track(project, track_id, payload));
    ordered_map_file_batch_put(batch, create_track_key(track_id), byte_buffer_to_omf_buffer(payload));
}

void DeleteTrackCommand::redo(OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));
    destroy(track, 1);
}

void DeleteTrackCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int DeleteTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

UndoCommand::UndoCommand(Project *project, Command *other_command) :
    Command(project),
    other_command(other_command)
{
}

void UndoCommand::undo(OrderedMapFileBatch *batch) {
    other_command->redo(batch);
}

void UndoCommand::redo(OrderedMapFileBatch *batch) {
    other_command->undo(batch);
}

void UndoCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int UndoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_object(this, buffer, offset))) return err;

    auto entry = project->commands.maybe_get(other_command_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    other_command = entry->value;

    return 0;
}


RedoCommand::RedoCommand(Project *project, Command *other_command) :
    Command(project),
    other_command(other_command)
{
}

void RedoCommand::undo(OrderedMapFileBatch *batch) {
    other_command->undo(batch);
}

void RedoCommand::redo(OrderedMapFileBatch *batch) {
    other_command->redo(batch);
}

void RedoCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int RedoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_object(this, buffer, offset))) return err;

    auto entry = project->commands.maybe_get(other_command_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    other_command = entry->value;

    return 0;
}

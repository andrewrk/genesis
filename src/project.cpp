#include "project.hpp"
#include "debug.hpp"

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

static OrderedMapFileBuffer *byte_buffer_to_omf_buffer(const ByteBuffer &byte_buffer) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(byte_buffer.length());
    memcpy(buf->data, byte_buffer.raw(), byte_buffer.length());
    return buf;
}

static void serialize_string(ByteBuffer &buf, const String &str) {
    ByteBuffer encoded_str = str.encode();
    buf.append_uint32be(encoded_str.length());
    buf.append(encoded_str);
}

static void serialize_uint256(ByteBuffer &buf, const uint256 &val) {
    buf.resize(buf.length() + UINT256_SIZE);
    val.write_be(buf.raw() + buf.length() - UINT256_SIZE);
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

static int deserialize_uint32be(uint32_t *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    *x = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    return 0;
}

static int deserialize_uint256(uint256 *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < UINT256_SIZE)
        return GenesisErrorInvalidFormat;

    *x = uint256::read_be(buffer.raw() + *offset);
    *offset += UINT256_SIZE;
    return 0;
}

static OrderedMapFileBuffer *serialize_command(Command *command) {
    ByteBuffer result;
    result.append_uint32be(command->command_type());
    serialize_uint256(result, command->user->id);
    result.append_uint32be(command->revision);
    command->serialize(result);
    return byte_buffer_to_omf_buffer(result);
}

static void serialize_track_to_byte_buffer(Track *track, ByteBuffer &buf) {
    serialize_string(buf, track->name);
    track->sort_key.serialize(buf);
}

static OrderedMapFileBuffer *serialize_track(Track *track) {
    ByteBuffer result;
    serialize_track_to_byte_buffer(track, result);
    return byte_buffer_to_omf_buffer(result);
}

static int deserialize_track(Project *project, const uint256 &id, const ByteBuffer &value) {
    Track *track = create_zero<Track>();
    if (!track)
        return GenesisErrorNoMem;

    int err;
    int offset = 0;
    track->id = id;
    if ((err = deserialize_string(track->name, value, &offset))) goto cleanup;
    if ((err = track->sort_key.deserialize(value, &offset))) goto cleanup;

    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    return 0;

cleanup:
    destroy(track, 1);
    return err;
}

static void serialize_user_to_byte_buffer(User *user, ByteBuffer &buf) {
    serialize_string(buf, user->name);
}

static OrderedMapFileBuffer *serialize_user(User *user) {
    ByteBuffer result;
    serialize_user_to_byte_buffer(user, result);
    return byte_buffer_to_omf_buffer(result);
}

static int deserialize_user(Project *project, const uint256 &id, const ByteBuffer &value) {
    User *user = create_zero<User>();
    if (!user)
        return GenesisErrorNoMem;

    int err;
    int offset = 0;
    user->id = id;
    if ((err = deserialize_string(user->name, value, &offset))) goto cleanup;

    project->users.put(user->id, user);
    project->user_list_dirty = true;

    return 0;

cleanup:
    destroy(user, 1);
    return err;
}

static int deserialize_command(Project *project, const uint256 &id, const ByteBuffer &buffer) {
    int offset = 0;
    int err;
    uint32_t command_type;
    if ((err = deserialize_uint32be(&command_type, buffer, &offset))) return err;

    Command *command = nullptr;
    switch ((CommandType)command_type) {
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

    uint256 user_id;
    if ((err = deserialize_uint256(&user_id, buffer, &offset))) { destroy(command, 1); return err; }
    auto entry = project->users.maybe_get(user_id);
    if (!entry)
        return GenesisErrorInvalidFormat;
    command->user = entry->value;

    uint32_t revision;
    if ((err = deserialize_uint32be(&revision, buffer, &offset))) { destroy(command, 1); return err; }
    command->revision = revision;

    if ((err = command->deserialize(buffer, &offset))) { destroy(command, 1); return err; }

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
    ordered_map_file_batch_put(batch, create_user_key(user->id), serialize_user(user));
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

    ordered_map_file_batch_put(batch, create_command_key(command), serialize_command(command));
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

    ordered_map_file_batch_put(batch, create_track_key(track_id), serialize_track(track));
}

void AddTrackCommand::serialize(ByteBuffer &buf) const {
    serialize_uint256(buf, track_id);
    serialize_string(buf, name);
    sort_key.serialize(buf);
}

int AddTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_uint256(&track_id, buffer, offset))) return err;
    if ((err = deserialize_string(name, buffer, offset))) return err;
    if ((err = sort_key.deserialize(buffer, offset))) return err;
    return 0;
}

DeleteTrackCommand::DeleteTrackCommand(Project *project, Track *track) :
    Command(project),
    track_id(track->id)
{
    serialize_track_to_byte_buffer(track, payload);
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

void DeleteTrackCommand::serialize(ByteBuffer &buf) const {
    serialize_uint256(buf, track_id);
    buf.append_uint32be(payload.length());
    buf.append(payload);
}

int DeleteTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_uint256(&track_id, buffer, offset))) return err;
    if ((err = deserialize_byte_buffer(payload, buffer, offset))) return err;
    return 0;
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

void UndoCommand::serialize(ByteBuffer &buf) const {
    serialize_uint256(buf, other_command->id);
}

int UndoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    uint256 other_command_id;
    if ((err = deserialize_uint256(&other_command_id, buffer, offset))) return err;

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

void RedoCommand::serialize(ByteBuffer &buf) const {
    serialize_uint256(buf, other_command->id);
}

int RedoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    uint256 other_command_id;
    if ((err = deserialize_uint256(&other_command_id, buffer, offset))) return err;

    auto entry = project->commands.maybe_get(other_command_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    other_command = entry->value;

    return 0;
}

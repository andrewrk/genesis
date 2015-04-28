#include "project.hpp"
#include "debug.hpp"

// modifying this structure affects project file backward compatibility
enum PropKey {
    PropKeyInvalid,
    PropKeyDelimiter,
    PropKeyProjectId,
    PropKeyTrack,
};

static const int PROP_KEY_SIZE = 4;
static const int UINT256_SIZE = 32;

static int get_next_revision(Project *project) {
    if (project->command_stack.length() == 0)
        return 0;

    Command *last_command = project->command_stack.at(project->command_stack.length() - 1);
    return last_command->revision + 1;
}

static int compare_tracks(Track *a, Track *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? uint256::compare(a->id, b->id) : sort_key_cmp;
}

static void project_sort_tracks(Project *project) {
    project->track_list.clear();
    auto it = project->tracks.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        Track *track = entry->value;
        ok_or_panic(project->track_list.append(track));
    }
    insertion_sort<Track *, compare_tracks>(project->track_list.raw(), project->track_list.length());
}

static void project_compute_indexes(Project *project) {
    if (project->track_list_dirty) {
        project->track_list_dirty = false;
        project_sort_tracks(project);
    }
}

static OrderedMapFileBuffer *create_track_key(uint256 id) {
    OrderedMapFileBuffer *buf = ordered_map_file_buffer_create(PROP_KEY_SIZE + PROP_KEY_SIZE + UINT256_SIZE);
    if (!buf)
        panic("out of memory");
    write_uint32be(&buf->data[0], PropKeyTrack);
    write_uint32be(&buf->data[4], PropKeyDelimiter);
    id.write_be(&buf->data[8]);
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

static int deserialize_string(String &out, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    int len = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    if (buffer.length() - *offset < len)
        return GenesisErrorInvalidFormat;

    bool ok;
    out = String(ByteBuffer(buffer.raw() + *offset, len), &ok);
    *offset += len;
    if (!ok)
        return GenesisErrorInvalidFormat;

    return 0;
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
    if ((err = deserialize_string(track->name, value, &offset))) return err;
    if ((err = track->sort_key.deserialize(value, &offset))) return err;

    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

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
    char key_buf[PROP_KEY_SIZE + PROP_KEY_SIZE];
    write_uint32be(&key_buf[0], PropKeyTrack);
    write_uint32be(&key_buf[4], PropKeyDelimiter);
    int index = ordered_map_file_find_key(project->omf, key_buf);
    int key_count = ordered_map_file_count(project->omf);

    ByteBuffer *key;
    ByteBuffer value;
    while (index >= 0 && index < key_count) {
        int err = ordered_map_file_get(project->omf, index, &key, value);
        if (err)
            return err;

        if (!key->starts_with(key_buf, sizeof(key_buf)))
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


void project_perform_command(Project *project, Command *command) {
    OrderedMapFileBatch *batch = ordered_map_file_batch_create(project->omf);
    project_perform_command_batch(project, batch, command);
    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
}

void project_perform_command_batch(Project *project, OrderedMapFileBatch *batch, Command *command) {
    assert(project);
    assert(batch);
    assert(command);

    int next_revision = get_next_revision(project);
    if (next_revision != command->revision)
        panic("unimplemented: multi-user editing merge conflict");

    ok_or_panic(project->command_stack.append(command));
    command->redo(project, batch);
}

void project_insert_track(Project *project, const Track *before, const Track *after) {
    OrderedMapFileBatch *batch = ordered_map_file_batch_create(project->omf);
    project_insert_track_batch(project, batch, before, after);
    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
}

void project_insert_track_batch(Project *project, OrderedMapFileBatch *batch,
        const Track *before, const Track *after)
{
    const SortKey *low_sort_key = before ? &before->sort_key : nullptr;
    const SortKey *high_sort_key = after ? &after->sort_key : nullptr;
    SortKey sort_key = SortKey::single(low_sort_key, high_sort_key);

    AddTrackCommand *add_track = create<AddTrackCommand>(
        project->active_user, get_next_revision(project), "Untitled Track", sort_key);
    project_perform_command_batch(project, batch, add_track);
}

void project_delete_track(Project *project, Track *track) {
    DeleteTrackCommand *delete_track = create<DeleteTrackCommand>(
            project->active_user, get_next_revision(project), track);
    project_perform_command(project, delete_track);
}

AddTrackCommand::AddTrackCommand(User *user, int revision,
        String name, const SortKey &sort_key) :
    Command(user, revision),
    name(name),
    sort_key(sort_key)
{
    track_id = uint256::random();
}

void AddTrackCommand::undo(Project *project, OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));

    destroy(track, 1);
}

void AddTrackCommand::redo(Project *project, OrderedMapFileBatch *batch) {
    Track *track = create<Track>();
    track->id = track_id;
    track->name = name;
    track->sort_key = sort_key;
    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    OrderedMapFileBuffer *key = create_track_key(track->id);
    OrderedMapFileBuffer *value = serialize_track(track);
    ordered_map_file_batch_put(batch, key, value);
}

User *user_create(const String &name) {
    User *user = create_zero<User>();
    if (!user)
        panic("out of memory");

    user->id = uint256::random();
    user->name = name;

    return user;
}

DeleteTrackCommand::DeleteTrackCommand(User *user, int revision, Track *track) :
    Command(user, revision),
    track_id(track->id)
{
    serialize_track_to_byte_buffer(track, payload);
}

void DeleteTrackCommand::undo(Project *project, OrderedMapFileBatch *batch) {
    ok_or_panic(deserialize_track(project, track_id, payload));
    ordered_map_file_batch_put(batch, create_track_key(track_id), byte_buffer_to_omf_buffer(payload));
}

void DeleteTrackCommand::redo(Project *project, OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));
    destroy(track, 1);
}

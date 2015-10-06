#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "genesis.h"
#include "uint256.hpp"
#include "id_map.hpp"
#include "sort_key.hpp"
#include "ordered_map_file.hpp"
#include "event_dispatcher.hpp"
#include "midi_hardware.hpp"
#include "atomic_value.hpp"
#include "atomic_double.hpp"
#include "atomics.hpp"
#include "device_id.hpp"

class Command;
struct AudioClipSegment;
struct Project;
struct SettingsFile;

struct EventList {
    List<GenesisMidiEvent> events;
};

struct AudioAsset {
    // canonical data
    uint256 id;
    ByteBuffer path;
    ByteBuffer sha256sum;

    // prepared view of data
    GenesisAudioFile *audio_file;
};

struct AudioClip {
    // canonical data
    uint256 id;
    uint256 audio_asset_id;
    String name;

    // prepared view of the data
    AudioAsset *audio_asset;

    // transient state
    Project *project;
    GenesisNodeDescriptor *node_descr;
    GenesisNode *node;
    GenesisNodeDescriptor *event_node_descr;
    GenesisNode *event_node;
    GenesisNode *resample_node;
    AtomicValue<List<GenesisMidiEvent>> events;
    List<GenesisMidiEvent> *events_write_ptr;
};

struct Track {
    // canonical data
    uint256 id;
    String name;
    SortKey sort_key;

    // prepared view of the data
    List<AudioClipSegment *> audio_clip_segments;
};

struct AudioClipSegment {
    // canonical data
    uint256 id;
    uint256 audio_clip_id;
    uint256 track_id;
    long start;
    long end;
    double pos;

    // prepared view of the data
    AudioClip *audio_clip;
    Track *track;
};

struct User {
    uint256 id;
    String name;
};

struct Effect;

struct MixerLine {
    uint256 id;
    String name;
    SortKey sort_key;
    bool solo;
    float volume;

    // prepared view of data
    List<Effect *> effects;
};

// modifying this structure affects project file backward compatibility
enum EffectSendType {
    EffectSendTypeDevice,
};

struct EffectSendDevice {
    int device_id; // see enum DeviceId
};

struct EffectSend {
    float gain;
    int send_type; // see enum EffectSendType
    union {
        EffectSendDevice device;
    } send;
};

enum EffectType {
    EffectTypeSend,
};

struct Effect {
    uint256 id;
    uint256 mixer_line_id;
    SortKey sort_key;
    int effect_type; // see enum EffectType
    union {
        EffectSend send;
    } effect;

    // prepared view of data
    MixerLine *mixer_line;
};

struct PlayChannelContext {
    struct GenesisAudioFileIterator iter;
    long offset;
};

struct Project {
    /////////// canonical data, shared among all users
    uint256 id;
    uint256 master_mixer_line_id;
    IdMap<AudioClipSegment *> audio_clip_segments;
    IdMap<AudioClip *> audio_clips;
    IdMap<AudioAsset *> audio_assets;
    IdMap<Track *> tracks;
    IdMap<User *> users;
    IdMap<MixerLine *> mixer_lines;
    IdMap<Effect *> effects;
    // this represents the true history of the project. you can create the
    // entire project data structure just from this data
    // this grows forever and never shrinks
    IdMap<Command *> commands;

    ///////////// state which is specific to this file, not shared among users
    // this is a subset of command_stack. it points to commands that are in
    // active_user's undo stack
    List<Command *> undo_stack;
    int undo_stack_index;

    /////////////// prepared view of the data
    List<Track *> track_list;
    bool track_list_dirty;

    List<User *> user_list;
    bool user_list_dirty;

    List<Command *> command_list;
    bool command_list_dirty;

    List<AudioAsset *> audio_asset_list;
    HashMap<ByteBuffer, AudioAsset *, ByteBuffer::hash> audio_assets_by_digest;
    bool audio_asset_list_dirty;

    List<AudioClip *> audio_clip_list;
    bool audio_clip_list_dirty;

    bool audio_clip_segments_dirty;
    bool effects_dirty;

    List<MixerLine *> mixer_line_list;
    bool mixer_line_list_dirty;

    ////////// transient state
    User *active_user; // the user that is running this instance of genesis
    OrderedMapFile *omf;
    EventDispatcher events;
    ByteBuffer path; // path to the project file

    GenesisContext *genesis_context;
    SettingsFile *settings_file;
    GenesisNodeDescriptor *resample_descr;
    GenesisNodeDescriptor *audio_file_descr;
    GenesisNodeDescriptor *spy_descr;
    GenesisNodeDescriptor *mixer_descr;
    GenesisNode *audio_file_node;
    GenesisNode *resample_node;
    GenesisNode *mixer_node;
    GenesisNode *spy_node;
    GenesisNode *playback_node;
    GenesisAudioFile *audio_file;
    GenesisPortDescriptor *audio_file_port_descr;
    long audio_file_frame_count;
    long audio_file_frame_index;
    PlayChannelContext audio_file_channel_context[GENESIS_MAX_CHANNELS];
    bool preview_audio_file_is_asset;

    double start_play_head_pos;
    AtomicDouble play_head_pos;
    atomic_bool is_playing;
    atomic_flag play_head_changed_flag;
};

int project_get_next_revision(Project *project);

enum CommandType {
    CommandTypeInvalid,
    CommandTypeUndo,
    CommandTypeRedo,
    CommandTypeAddTrack,
    CommandTypeDeleteTrack,
    CommandTypeAddAudioClip,
    CommandTypeAddAudioClipSegment,
};

class Command {
public:
    Command() {}
    Command(Project *p) {
        project = p;
        user = project->active_user;
        user_id = user->id;
        revision = project_get_next_revision(project);
        id = uint256::random();
    }
    virtual ~Command() {}
    virtual void undo(OrderedMapFileBatch *batch) = 0;
    virtual void redo(OrderedMapFileBatch *batch) = 0;
    virtual String description() const = 0;
    virtual int allocated_size() const = 0;
    virtual void serialize(ByteBuffer &buf) = 0;
    virtual int deserialize(const ByteBuffer &buf, int *offset) = 0;
    virtual CommandType command_type() const = 0;

    // serialized
    uint256 id;
    uint256 user_id;
    int revision;

    // transient state
    Project *project;
    User *user;
};

class AddTrackCommand : public Command {
public:
    AddTrackCommand(Project *project, String name, const SortKey &sort_key);
    AddTrackCommand() {}
    ~AddTrackCommand() override {}

    String description() const override {
        return "Insert Track";
    }
    int allocated_size() const override {
        return sizeof(AddTrackCommand) + name.allocated_size() + sort_key.allocated_size();
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeAddTrack; }

    uint256 track_id;
    String name;
    SortKey sort_key;
};

class DeleteTrackCommand : public Command {
public:
    DeleteTrackCommand(Project *project, Track *track);
    DeleteTrackCommand() {}
    ~DeleteTrackCommand() override {}

    String description() const override {
        return "Delete Track";
    }
    int allocated_size() const override {
        return sizeof(DeleteTrackCommand) + payload.allocated_size();
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeDeleteTrack; }

    uint256 track_id;
    ByteBuffer payload;
};

class AddAudioClipCommand : public Command {
public:
    AddAudioClipCommand(Project *project, AudioAsset *audio_asset, const String &name);
    AddAudioClipCommand() {}
    ~AddAudioClipCommand() override {}

    String description() const override {
        return "Add Audio Clip";
    }
    int allocated_size() const override {
        return sizeof(AddAudioClipCommand) + name.allocated_size();
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeAddAudioClip; }

    uint256 audio_clip_id;
    uint256 audio_asset_id;
    String name;
};

class AddAudioClipSegmentCommand : public Command {
public:
    AddAudioClipSegmentCommand(Project *project, AudioClip *audio_clip, Track *track,
            long start, long end, double pos);
    AddAudioClipSegmentCommand() {}
    ~AddAudioClipSegmentCommand() override {}

    String description() const override {
        return "Add Audio Clip Segment";
    }
    int allocated_size() const override {
        return sizeof(AddAudioClipSegmentCommand);
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeAddAudioClipSegment; }

    uint256 audio_clip_segment_id;
    uint256 audio_clip_id;
    uint256 track_id;
    long start;
    long end;
    double pos;
};

class UndoCommand : public Command {
public:
    UndoCommand(Project *project, Command *other_command);
    UndoCommand() {}
    ~UndoCommand() override {}

    String description() const override {
        return "Undo";
    }
    int allocated_size() const override {
        return sizeof(UndoCommand);
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeUndo; }

    // serialized state
    uint256 other_command_id;

    // transient state
    Command *other_command;
};

class RedoCommand : public Command {
public:
    RedoCommand(Project *project, Command *other_command);
    RedoCommand() {}
    ~RedoCommand() override {}

    String description() const override {
        return "Redo";
    }
    int allocated_size() const override {
        return sizeof(RedoCommand);
    }

    void undo(OrderedMapFileBatch *batch) override;
    void redo(OrderedMapFileBatch *batch) override;
    void serialize(ByteBuffer &buf) override;
    int deserialize(const ByteBuffer &buf, int *offset) override;
    CommandType command_type() const override { return CommandTypeRedo; }

    // serialized state
    uint256 other_command_id;

    // transient state
    Command *other_command;
};

User *user_create(const uint256 &id, const String &name);
void user_destroy(User *user);

int project_open(const char *path, GenesisContext *genesis_context, SettingsFile *settings_file,
        User *user, Project **out_project);
int project_create(const char *path, GenesisContext *genesis_context, SettingsFile *settings_file,
        const uint256 &id, User *user, Project **out_project);
void project_close(Project *project);

void project_undo(Project *project);
void project_redo(Project *project);

void project_perform_command(Project *project, Command *command);
void project_perform_command_batch(Project *project, OrderedMapFileBatch *batch, Command *command);

void project_insert_track(Project *project, const Track *before, const Track *after);
AddTrackCommand * project_insert_track_batch(Project *project, OrderedMapFileBatch *batch,
        const Track *before, const Track *after);

void project_delete_track(Project *project, Track *track);

int project_add_audio_asset(Project *project, const ByteBuffer &full_path, AudioAsset **audio_asset);
void project_add_audio_clip(Project *project, AudioAsset *audio_asset);
void project_add_audio_clip_segment(Project *project, AudioClip *audio_clip, Track *track,
        long start, long end, double pos);

int project_ensure_audio_asset_loaded(Project *project, AudioAsset *audio_asset);
long project_audio_clip_frame_count(Project *project, AudioClip *audio_clip);
int project_audio_clip_sample_rate(Project *project, AudioClip *audio_clip);

void project_get_effect_string(Project *project, Effect *effect, String &result);

#endif

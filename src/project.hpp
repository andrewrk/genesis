#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "genesis.h"
#include "uint256.hpp"
#include "id_map.hpp"
#include "sort_key.hpp"
#include "ordered_map_file.hpp"

struct Command;
struct AudioClipSegment;

struct AudioAsset {
    uint256 id;
    ByteBuffer path;
    GenesisAudioFile *audio_file;
};

struct AudioClip {
    uint256 id;
    AudioAsset *audio_asset;
    GenesisNode *node;
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
    uint256 id;

    AudioClip *audio_clip;
    long start;
    long end;

    Track *track;
    double pos;
};

struct User {
    uint256 id;
    String name;
    int undo_top;
};

struct Project {
    // canonical data
    uint256 id;
    IdMap<AudioClipSegment *> audio_clip_segments;
    IdMap<AudioClip *> audio_clips;
    IdMap<AudioAsset *> audio_assets;
    IdMap<Track *> tracks;
    IdMap<User *> users;

    // this represents the true history of the project. you can create the
    // entire project data structure just from this data
    // this grows forever and never shrinks
    List<Command *> command_stack;

    // prepared view of the data
    // before you use any of this state, you must call project_compute_indexes
    List<Track *> track_list;
    bool track_list_dirty;

    // transient state
    User *active_user; // the user that is running this instance of genesis
    OrderedMapFile *omf;
};

class Command {
public:
    Command(User *user, int revision) : user(user), revision(revision) {}
    virtual ~Command() {}
    virtual void undo(Project *project, OrderedMapFileBatch *batch) = 0;
    virtual void redo(Project *project, OrderedMapFileBatch *batch) = 0;
    virtual String description() const = 0;
    virtual int allocated_size() const = 0;

    User *user;
    int revision;
};

class AddTrackCommand : public Command {
public:
    AddTrackCommand(User *user, int revision, String name, const SortKey &sort_key);
    ~AddTrackCommand() override {}

    String description() const override {
        return "Insert Track";
    }
    int allocated_size() const override {
        return sizeof(AddTrackCommand) + name.allocated_size() + sort_key.allocated_size();
    }

    void undo(Project *project, OrderedMapFileBatch *batch) override;
    void redo(Project *project, OrderedMapFileBatch *batch) override;

    uint256 track_id;
    String name;
    SortKey sort_key;
};

class DeleteTrackCommand : public Command {
public:
    DeleteTrackCommand(User *user, int revision, Track *track);
    ~DeleteTrackCommand() override {}

    String description() const override {
        return "Delete Track";
    }
    int allocated_size() const override {
        return sizeof(DeleteTrackCommand) + payload.allocated_size();
    }

    void undo(Project *project, OrderedMapFileBatch *batch) override;
    void redo(Project *project, OrderedMapFileBatch *batch) override;

    uint256 track_id;
    ByteBuffer payload;
};


User *user_create(const String &name);

int project_open(const char *path, User *user, Project **out_project);
int project_create(const char *path, const uint256 &id, User *user, Project **out_project);
void project_close(Project *project);

void project_perform_command(Project *project, Command *command);
void project_perform_command_batch(Project *project, OrderedMapFileBatch *batch, Command *command);

void project_insert_track(Project *project, const Track *before, const Track *after);
void project_insert_track_batch(Project *project, OrderedMapFileBatch *batch,
        const Track *before, const Track *after);

void project_delete_track(Project *project, Track *track);

#endif

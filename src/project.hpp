#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "genesis.h"
#include "hash_map.hpp"
#include "sort_key.hpp"

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

struct AudioClipSegment;
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
};

struct Project;
class Command {
public:
    Command(User *user, int revision) : user(user), revision(revision) {}
    virtual ~Command() {}
    virtual void undo(Project *project) = 0;
    virtual void redo(Project *project) = 0;
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

    void undo(Project *project) override;
    void redo(Project *project) override;

    uint256 track_id;
    String name;
    SortKey sort_key;
    List<AudioClipSegment *> audio_clip_segments;
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
    List<Command *> command_stack;

    // prepared view of the data
    List<Track *> track_list;

    // transient state
    User *active_user; // the user that is running this instance of genesis
    // pointer into command_stack, the top of active_user's undo stack
    // points to the top item in the stack
    int active_user_undo_top;
};

User *user_create(const String &name);

Project *project_create(User *user);
void project_perform_command(Project *project, Command *command);
void project_insert_track(Project *project, const Track *before, const Track *after);

#endif

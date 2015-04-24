#include "project.hpp"
#include "debug.hpp"

static int get_next_revision(Project *project) {
    if (project->command_stack.length() == 0)
        return 0;

    Command *last_command = project->command_stack.at(project->command_stack.length() - 1);
    return last_command->revision + 1;
}

static int compare_tracks(Track *a, Track *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? compare_uint256(a->id, b->id) : sort_key_cmp;
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

Project *project_create(User *user) {
    Project *project = create<Project>();

    project->id = random_uint256();
    project->active_user = user;

    project_insert_track(project, nullptr, nullptr);

    return project;
}

void project_perform_command(Project *project, Command *command) {
    int next_revision = get_next_revision(project);
    if (next_revision != command->revision)
        panic("unimplemented: multi-user editing merge conflict");

    ok_or_panic(project->command_stack.append(command));
    command->redo(project);
}

void project_insert_track(Project *project, const Track *before, const Track *after) {
    const SortKey *low_sort_key = before ? &before->sort_key : nullptr;
    const SortKey *high_sort_key = after ? &after->sort_key : nullptr;
    SortKey sort_key = SortKey::single(low_sort_key, high_sort_key);

    AddTrackCommand *add_track = create<AddTrackCommand>(
        project->active_user, get_next_revision(project), "Untitled Track", sort_key);
    project_perform_command(project, add_track);
}

AddTrackCommand::AddTrackCommand(User *user, int revision,
        String name, const SortKey &sort_key) :
    Command(user, revision),
    name(name),
    sort_key(sort_key)
{
    track_id = random_uint256();
}

void AddTrackCommand::undo(Project *project) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);

    project_sort_tracks(project);
}

void AddTrackCommand::redo(Project *project) {
    Track *track = create<Track>();
    track->id = track_id;
    track->name = name;
    track->sort_key = sort_key;
    project->tracks.put(track->id, track);

    project_sort_tracks(project);
}

User *user_create(const String &name) {
    User *user = create<User>();

    user->id = random_uint256();
    user->name = name;

    return user;
}

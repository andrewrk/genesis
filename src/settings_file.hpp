#ifndef SETTINGS_FILE_HPP
#define SETTINGS_FILE_HPP

#include "byte_buffer.hpp"
struct LaxJsonContext;

enum SettingsFileState {
    SettingsFileStateStart,
    SettingsFileStateReadyForProp,
    SettingsFileStateOpenProjectFile,
};

struct SettingsFile {
    // settings you can directly manipulate
    ByteBuffer open_project_file;

    // private state
    ByteBuffer path;
    SettingsFileState state;
    LaxJsonContext *json;
};

SettingsFile *settings_file_open(const ByteBuffer &path);
void settings_file_close(SettingsFile *sf);

// atomically update settings file on disk
int settings_file_commit(SettingsFile *sf);

#endif

#ifndef SETTINGS_FILE_HPP
#define SETTINGS_FILE_HPP

#include "byte_buffer.hpp"
#include "string.hpp"
#include "uint256.hpp"
struct LaxJsonContext;

enum SettingsFileState {
    SettingsFileStateStart,
    SettingsFileStateReadyForProp,
    SettingsFileStateOpenProjectFile,
    SettingsFileStateUserName,
    SettingsFileStateUserId,
};

struct SettingsFile {
    // settings you can directly manipulate
    uint256 open_project_id;
    String user_name;
    uint256 user_id;

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

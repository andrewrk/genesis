#ifndef SETTINGS_FILE_HPP
#define SETTINGS_FILE_HPP

#include "byte_buffer.hpp"
#include "string.hpp"
#include "uint256.hpp"

struct LaxJsonContext;

enum SettingsFileState {
    SettingsFileStateStart,
    SettingsFileStateEnd,
    SettingsFileStateReadyForProp,
    SettingsFileStateOpenProjectFile,
    SettingsFileStateUserName,
    SettingsFileStateUserId,
    SettingsFileStateLatency,
    SettingsFileStateExpectSampleDirs,
    SettingsFileStateSampleDirsItem,
    SettingsFileStatePerspectives,
    SettingsFileStatePerspectivesItem,
    SettingsFileStatePerspectivesItemProp,
    SettingsFileStatePerspectivesItemPropName,
    SettingsFileStatePerspectivesItemPropDock,
    SettingsFileStateDockItemProp,
    SettingsFileStateDockItemPropType,
    SettingsFileStateDockItemPropSplitRatio,
    SettingsFileStateDockItemPropChildA,
    SettingsFileStateDockItemPropChildB,
    SettingsFileStateDockItemPropTabs,
    SettingsFileStateTabName,
    SettingsFileStateOpenWindows,
    SettingsFileStateOpenWindowItem,
    SettingsFileStateOpenWindowItemProp,
    SettingsFileStateOpenWindowPerspectiveIndex,
    SettingsFileStateOpenWindowLeft,
    SettingsFileStateOpenWindowTop,
    SettingsFileStateOpenWindowWidth,
    SettingsFileStateOpenWindowHeight,
    SettingsFileStateOpenWindowMaximized,
    SettingsFileStateOpenWindowAlwaysShowTabs,
};

enum SettingsFileDockType {
    SettingsFileDockTypeTabs,
    SettingsFileDockTypeHoriz,
    SettingsFileDockTypeVert,
};

struct SettingsFileDock {
    SettingsFileDockType dock_type;
    float split_ratio;
    SettingsFileDock *child_a;
    SettingsFileDock *child_b;
    List<String> tabs;
};

struct SettingsFilePerspective {
    String name;
    SettingsFileDock dock;
};

struct SettingsFileOpenWindow {
    int perspective_index;
    int left;
    int top;
    int width;
    int height;
    bool maximized;
    bool always_show_tabs;
};

struct SettingsFile {
    // settings you can directly manipulate
    uint256 open_project_id;
    String user_name;
    uint256 user_id;
    List<SettingsFileOpenWindow> open_windows;
    List<SettingsFilePerspective> perspectives;
    List<ByteBuffer> sample_dirs;
    double latency;

    // private state
    ByteBuffer path;
    SettingsFileState state;
    LaxJsonContext *json;
    SettingsFilePerspective *current_perspective;
    SettingsFileDock *current_dock;
    List<SettingsFileDock *> dock_stack;
    SettingsFileOpenWindow *current_open_window;
};

SettingsFile *settings_file_open(const ByteBuffer &path);
void settings_file_close(SettingsFile *sf);

// atomically update settings file on disk
int settings_file_commit(SettingsFile *sf);

void settings_file_clear_dock(SettingsFileDock *dock);

#endif

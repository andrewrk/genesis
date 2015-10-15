#ifndef SETTINGS_FILE_HPP
#define SETTINGS_FILE_HPP

#include "byte_buffer.hpp"
#include "string.hpp"
#include "uint256.hpp"
#include "device_id.hpp"
#include "event_dispatcher.hpp"
#include "audio_file.hpp"

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
    SettingsFileStateDeviceDesignations,
    SettingsFileStateDeviceDesignationProp,
    SettingsFileStateDeviceDesignationValue,
    SettingsFileStateDeviceDesignationIdProp,
    SettingsFileStateDeviceDesignationIdBackend,
    SettingsFileStateDeviceDesignationIdDevice,
    SettingsFileStateDeviceDesignationIdRaw,
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

struct SettingsFileDeviceId {
    SoundIoBackend backend;
    ByteBuffer device_id;
    bool is_raw;
};

struct SettingsFile {
    // use this to announce when you change a setting (if anyone cares)
    EventDispatcher events;

    // settings you can directly manipulate
    uint256 open_project_id;
    String user_name;
    uint256 user_id;
    List<SettingsFileOpenWindow> open_windows;
    List<SettingsFilePerspective> perspectives;
    List<ByteBuffer> sample_dirs;
    // index is DeviceId. if backend_name is NULL then that DeviceId is unspecified
    List<SettingsFileDeviceId> device_designations;
    double latency;
    RenderFormatType default_render_format;
    SoundIoFormat default_render_sample_formats[RenderFormatTypeCount];
    int default_render_bit_rates[RenderFormatTypeCount];

    // private state
    ByteBuffer path;
    SettingsFileState state;
    LaxJsonContext *json;
    SettingsFilePerspective *current_perspective;
    SettingsFileDock *current_dock;
    List<SettingsFileDock *> dock_stack;
    SettingsFileOpenWindow *current_open_window;
    DeviceId current_device_id;
    SettingsFileDeviceId *current_sf_device_id;
};

SettingsFile *settings_file_open(const ByteBuffer &path);
void settings_file_close(SettingsFile *sf);

// atomically update settings file on disk
int settings_file_commit(SettingsFile *sf);

void settings_file_clear_dock(SettingsFileDock *dock);

// you still need to commit after calling these
void settings_file_set_default_render_format(SettingsFile *sf, RenderFormatType format_type);

void settings_file_set_default_render_sample_format(SettingsFile *sf,
        RenderFormatType format_type, SoundIoFormat sample_format);

void settings_file_set_default_render_bit_rate(SettingsFile *sf,
        RenderFormatType format_type, int bit_rate);

#endif

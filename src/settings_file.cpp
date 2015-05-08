#include "settings_file.hpp"
#include "os.hpp"
#include "path.hpp"

#include <errno.h>
#include <laxjson.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int parse_error(SettingsFile *sf, const char *msg) {
    panic("Line %d, column %d: %s", sf->json->line, sf->json->column, msg);
}

static int on_string(struct LaxJsonContext *json,
    enum LaxJsonType type, const char *value_raw, int length)
{
    SettingsFile *sf = (SettingsFile *) json->userdata;
    ByteBuffer value = ByteBuffer(value_raw, length);
    switch (sf->state) {
        case SettingsFileStateReadyForProp:
            {
                if (ByteBuffer::compare(value, "open_project_id") == 0) {
                    sf->state = SettingsFileStateOpenProjectFile;
                } else if (ByteBuffer::compare(value, "user_name") == 0) {
                    sf->state = SettingsFileStateUserName;
                } else if (ByteBuffer::compare(value, "user_id") == 0) {
                    sf->state = SettingsFileStateUserId;
                } else if (ByteBuffer::compare(value, "perspectives") == 0) {
                    sf->state = SettingsFileStatePerspectives;
                } else if (ByteBuffer::compare(value, "open_windows") == 0) {
                    sf->state = SettingsFileStateOpenWindows;
                } else {
                    return parse_error(sf, "invalid setting name");
                }
                break;
            }
        case SettingsFileStateOpenProjectFile:
            sf->open_project_id = uint256::parse(value);
            sf->state = SettingsFileStateReadyForProp;
            break;
        case SettingsFileStateUserId:
            sf->user_id = uint256::parse(value);
            sf->state = SettingsFileStateReadyForProp;
            break;
        case SettingsFileStateUserName:
            {
                bool ok;
                sf->user_name = String(value, &ok);
                if (!ok)
                    return parse_error(sf, "invalid UTF-8");
                sf->state = SettingsFileStateReadyForProp;
                break;
            }
        case SettingsFileStatePerspectivesItemProp:
            if (ByteBuffer::compare(value, "name") == 0) {
                sf->state = SettingsFileStatePerspectivesItemPropName;
            } else if (ByteBuffer::compare(value, "dock") == 0) {
                sf->state = SettingsFileStatePerspectivesItemPropDock;
                sf->current_dock = &sf->current_perspective->dock;
            } else {
                return parse_error(sf, "invalid perspective property");
            }
            break;
        case SettingsFileStatePerspectivesItemPropName:
            {
                bool ok;
                sf->current_perspective->name = String(value, &ok);
                if (!ok)
                    return parse_error(sf, "invalid UTF-8");
                sf->state = SettingsFileStatePerspectivesItemProp;
                break;
            }
        case SettingsFileStateDockItemProp:
            if (ByteBuffer::compare(value, "dock_type") == 0) {
                sf->state = SettingsFileStateDockItemPropType;
            } else if (ByteBuffer::compare(value, "split_ratio") == 0) {
                sf->state = SettingsFileStateDockItemPropSplitRatio;
            } else if (ByteBuffer::compare(value, "child_a") == 0) {
                sf->state = SettingsFileStateDockItemPropChildA;
            } else if (ByteBuffer::compare(value, "child_b") == 0) {
                sf->state = SettingsFileStateDockItemPropChildB;
            } else if (ByteBuffer::compare(value, "tabs") == 0) {
                sf->state = SettingsFileStateDockItemPropTabs;
            } else {
                return parse_error(sf, "invalid dock item property name");
            }
            break;
        case SettingsFileStateDockItemPropType:
            if (ByteBuffer::compare(value, "Horiz") == 0) {
                sf->current_dock->dock_type = SettingsFileDockTypeHoriz;
            } else if (ByteBuffer::compare(value, "Vert") == 0) {
                sf->current_dock->dock_type = SettingsFileDockTypeVert;
            } else if (ByteBuffer::compare(value, "Tabs") == 0) {
                sf->current_dock->dock_type = SettingsFileDockTypeTabs;
            } else {
                return parse_error(sf, "invalid dock_type value");
            }
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateTabName:
            {
                bool ok;
                String title = String(value, &ok);
                if (!ok)
                    return parse_error(sf, "invalid UTF-8");
                ok_or_panic(sf->current_dock->tabs.append(title));
                break;
            }
        case SettingsFileStateOpenWindowItemProp:
            if (ByteBuffer::compare(value, "perspective") == 0) {
                sf->state = SettingsFileStateOpenWindowPerspectiveIndex;
            } else if (ByteBuffer::compare(value, "left") == 0) {
                sf->state = SettingsFileStateOpenWindowLeft;
            } else if (ByteBuffer::compare(value, "top") == 0) {
                sf->state = SettingsFileStateOpenWindowTop;
            } else if (ByteBuffer::compare(value, "width") == 0) {
                sf->state = SettingsFileStateOpenWindowWidth;
            } else if (ByteBuffer::compare(value, "height") == 0) {
                sf->state = SettingsFileStateOpenWindowHeight;
            } else if (ByteBuffer::compare(value, "maximized") == 0) {
                sf->state = SettingsFileStateOpenWindowMaximized;
            } else if (ByteBuffer::compare(value, "always_show_tabs") == 0) {
                sf->state = SettingsFileStateOpenWindowAlwaysShowTabs;
            } else {
                return parse_error(sf, "invalid open window property name");
            }
            break;
        default:
            return parse_error(sf, (type == LaxJsonTypeProperty) ? "unexpected property" : "unexpected string");
    }
    return 0;
}

static int on_number(struct LaxJsonContext *json, double x) {
    SettingsFile *sf = (SettingsFile *) json->userdata;
    switch (sf->state) {
        default:
            return parse_error(sf, "unexpected number");
        case SettingsFileStateDockItemPropSplitRatio:
            sf->current_dock->split_ratio = x;
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateOpenWindowLeft:
            sf->current_open_window->left = x;
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        case SettingsFileStateOpenWindowTop:
            sf->current_open_window->top = x;
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        case SettingsFileStateOpenWindowWidth:
            sf->current_open_window->width = x;
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        case SettingsFileStateOpenWindowHeight:
            sf->current_open_window->height = x;
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        case SettingsFileStateOpenWindowPerspectiveIndex:
            sf->current_open_window->perspective_index = (int)x;
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
    }
    return 0;
}

static int on_primitive(struct LaxJsonContext *json, enum LaxJsonType type) {
    SettingsFile *sf = (SettingsFile *) json->userdata;
    switch (sf->state) {
        case SettingsFileStateOpenWindowAlwaysShowTabs:
            if (type != LaxJsonTypeTrue && type != LaxJsonTypeFalse)
                return parse_error(sf, "expected boolean");
            sf->current_open_window->always_show_tabs = (type == LaxJsonTypeTrue);
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        case SettingsFileStateOpenWindowMaximized:
            if (type != LaxJsonTypeTrue && type != LaxJsonTypeFalse)
                return parse_error(sf, "expected boolean");
            sf->current_open_window->maximized = (type == LaxJsonTypeTrue);
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
        default:
            return parse_error(sf, "unexpected primitive");
    }
    return 0;
}

static int on_begin(struct LaxJsonContext *json, enum LaxJsonType type) {
    SettingsFile *sf = (SettingsFile *) json->userdata;
    switch (sf->state) {
        default:
            return parse_error(sf, (type == LaxJsonTypeObject) ? "unexpected object" : "unexpected array");
        case SettingsFileStateStart:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected object");
            sf->state = SettingsFileStateReadyForProp;
            break;
        case SettingsFileStatePerspectives:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected array");
            sf->state = SettingsFileStatePerspectivesItem;
            break;
        case SettingsFileStatePerspectivesItem:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected object");
            ok_or_panic(sf->perspectives.add_one());
            sf->current_perspective = &sf->perspectives.last();
            sf->state = SettingsFileStatePerspectivesItemProp;
            break;
        case SettingsFileStatePerspectivesItemPropDock:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected object");
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateDockItemPropTabs:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected array");
            sf->state = SettingsFileStateTabName;
            break;
        case SettingsFileStateDockItemPropChildA:
            ok_or_panic(sf->dock_stack.append(sf->current_dock));
            sf->current_dock->child_a = ok_mem(create_zero<SettingsFileDock>());
            sf->current_dock = sf->current_dock->child_a;
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateDockItemPropChildB:
            ok_or_panic(sf->dock_stack.append(sf->current_dock));
            sf->current_dock->child_b = ok_mem(create_zero<SettingsFileDock>());
            sf->current_dock = sf->current_dock->child_b;
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateOpenWindows:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected array");
            sf->state = SettingsFileStateOpenWindowItem;
            break;
        case SettingsFileStateOpenWindowItem:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected object");
            ok_or_panic(sf->open_windows.add_one());
            sf->current_open_window = &sf->open_windows.last();
            sf->state = SettingsFileStateOpenWindowItemProp;
            break;
    }
    return 0;
}

static int on_end(struct LaxJsonContext *json, enum LaxJsonType type) {
    SettingsFile *sf = (SettingsFile *) json->userdata;
    switch (sf->state) {
        default:
            return parse_error(sf, "unexpected end");
        case SettingsFileStateReadyForProp:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected end object");
            sf->state = SettingsFileStateEnd;
            break;
        case SettingsFileStatePerspectivesItemProp:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected end object");
            sf->state = SettingsFileStatePerspectivesItem;
            break;
        case SettingsFileStatePerspectivesItem:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected end array");
            sf->state = SettingsFileStateReadyForProp;
            break;
        case SettingsFileStateTabName:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected end array");
            if (sf->current_dock->tabs.length() == 0)
                return parse_error(sf, "tabs array cannot be empty");
            sf->state = SettingsFileStateDockItemProp;
            break;
        case SettingsFileStateDockItemProp:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected end object");
            if (sf->dock_stack.length() > 0) {
                sf->current_dock = sf->dock_stack.pop();
            } else {
                sf->current_dock = nullptr;
                sf->state = SettingsFileStatePerspectivesItemProp;
            }
            break;
        case SettingsFileStateOpenWindowItem:
            if (type != LaxJsonTypeArray)
                return parse_error(sf, "expected array");
            sf->state = SettingsFileStateReadyForProp;
            break;
        case SettingsFileStateOpenWindowItemProp:
            if (type != LaxJsonTypeObject)
                return parse_error(sf, "expected end object");
            sf->state = SettingsFileStateOpenWindowItem;
            break;
    }
    return 0;
}

static const char *dock_type_to_str(SettingsFileDockType dock_type) {
    switch (dock_type) {
        case SettingsFileDockTypeTabs:
            return "Tabs";
        case SettingsFileDockTypeVert:
            return "Vert";
        case SettingsFileDockTypeHoriz:
            return "Horiz";
    }
    panic("bad dock type");
}

static void do_indent(FILE *f, int indent) {
    for (int i = 0; i < indent; i += 1)
        fprintf(f, " ");
}

static void json_inline_str(FILE *f, const ByteBuffer &value) {
    fprintf(f, "\"");
    for (int i = 0; i < value.length(); i += 1) {
        char c = value.at(i);
        switch (c) {
            case '\n':
                fprintf(f, "\\n");
                break;
            case '\r':
                fprintf(f, "\\r");
                break;
            case '\f':
                fprintf(f, "\\f");
                break;
            case '\t':
                fprintf(f, "\\t");
                break;
            case '\b':
                fprintf(f, "\\b");
                break;
            case '"':
                fprintf(f, "\\\"");
                break;
            case '\\':
                fprintf(f, "\\\\");
                break;
            default:
                fprintf(f, "%c", c);
                break;
        }
    }
    fprintf(f, "\"");
}

static void json_line_str(FILE *f, int indent, const char *key, const ByteBuffer &value) {
    do_indent(f, indent);
    fprintf(f, "%s: ", key);
    json_inline_str(f, value);
    fprintf(f, ",\n");
}

static void json_line_uint256(FILE *f, int indent, const char *key, const uint256 &value) {
    do_indent(f, indent);
    fprintf(f, "%s: \"%s\",\n", key, value.to_string().raw());
}

static void json_line_int(FILE *f, int indent, const char *key, int value) {
    do_indent(f, indent);
    fprintf(f, "%s: %d,\n", key, value);
}

static void json_line_float(FILE *f, int indent, const char *key, float value) {
    do_indent(f, indent);
    fprintf(f, "%s: %f,\n", key, value);
}

static void json_line_bool(FILE *f, int indent, const char *key, bool value) {
    do_indent(f, indent);
    fprintf(f, "%s: %s,\n", key, value ? "true" : "false");
}

static void json_line_comment(FILE *f, int indent, const char *comment) {
    do_indent(f, indent);
    fprintf(f, "// %s\n", comment);
}

static void json_line_indent(FILE *f, int *indent, const char *brace) {
    fprintf(f, "%s\n", brace);
    *indent += 2;
}

static void json_line_outdent(FILE *f, int *indent, const char *brace) {
    *indent -= 2;
    do_indent(f, *indent);
    fprintf(f, "%s\n", brace);
}

static void json_line_dock(FILE *f, int indent, const char *key, const SettingsFileDock *dock) {
    assert(dock);

    do_indent(f, indent);
    fprintf(f, "%s: ", key);
    json_line_indent(f, &indent, "{");

    json_line_str(f, indent, "dock_type", dock_type_to_str(dock->dock_type));

    switch (dock->dock_type) {
        case SettingsFileDockTypeTabs:
            do_indent(f, indent);
            fprintf(f, "tabs: [");
            for (int i = 0; i < dock->tabs.length(); i += 1) {
                json_inline_str(f, dock->tabs.at(i).encode());
                if (i < dock->tabs.length() - 1)
                    fprintf(f, ", ");
            }
            fprintf(f, "],\n");
            break;
        case SettingsFileDockTypeHoriz:
        case SettingsFileDockTypeVert:
            json_line_float(f, indent, "split_ratio", dock->split_ratio);
            json_line_dock(f, indent, "child_a", dock->child_a);
            json_line_dock(f, indent, "child_b", dock->child_b);
            break;
    }

    json_line_outdent(f, &indent, "},");
}

static void json_line_open_windows(FILE *f, int indent, const char *key,
        const List<SettingsFileOpenWindow> &open_windows)
{
    do_indent(f, indent);
    fprintf(f, "%s: ", key);
    json_line_indent(f, &indent, "[");

    for (int i = 0; i < open_windows.length(); i += 1) {
        const SettingsFileOpenWindow *open_window = &open_windows.at(i);

        do_indent(f, indent);
        json_line_indent(f, &indent, "{");

        json_line_comment(f, indent, "which perspective this window uses");
        json_line_int(f, indent, "perspective", open_window->perspective_index);
        fprintf(f, "\n");

        json_line_comment(f, indent, "position of the window on the screen");
        json_line_int(f, indent, "left", open_window->left);
        json_line_int(f, indent, "top", open_window->top);
        json_line_int(f, indent, "width", open_window->width);
        json_line_int(f, indent, "height", open_window->height);
        json_line_bool(f, indent, "maximized", open_window->maximized);
        fprintf(f, "\n");

        json_line_comment(f, indent, "whether to show dockable pane tabs when there is only one");
        json_line_bool(f, indent, "always_show_tabs", open_window->always_show_tabs);
        fprintf(f, "\n");

        json_line_outdent(f, &indent, "},");
    }
    json_line_outdent(f, &indent, "],");
}

static void json_line_perspectives(FILE *f, int indent, const char *key,
        const List<SettingsFilePerspective> &perspectives)
{
    do_indent(f, indent);
    fprintf(f, "%s: ", key);
    json_line_indent(f, &indent, "[");

    for (int i = 0; i < perspectives.length(); i += 1) {
        const SettingsFilePerspective *perspective = &perspectives.at(i);

        do_indent(f, indent);
        json_line_indent(f, &indent, "{");

        json_line_str(f, indent, "name", perspective->name.encode());
        json_line_dock(f, indent, "dock", &perspective->dock);

        json_line_outdent(f, &indent, "},");
    }
    json_line_outdent(f, &indent, "],");
}

static void handle_parse_error(SettingsFile *sf, LaxJsonError err) {
    if (err) {
        fprintf(stderr, "Error parsing config file: %s\n", sf->path.raw());
        panic("Line %d, column %d: %s", sf->json->line, sf->json->column, lax_json_str_err(err));
    }
}

SettingsFile *settings_file_open(const ByteBuffer &path) {
    SettingsFile *sf = ok_mem(create_zero<SettingsFile>());

    sf->path = path;

    // default settings
    sf->open_project_id = uint256::zero();
    sf->user_name = "";
    sf->user_id = uint256::zero();

    FILE *f = fopen(path.raw(), "rb");
    if (!f) {
        if (errno == ENOENT) {
            // no settings file, leave everything at default
            return sf;
        }
        panic("unable to open settings file: %s", strerror(errno));
    }

    sf->state = SettingsFileStateStart;

    LaxJsonContext *json = lax_json_create();
    if (!json)
        panic("out of memory");
    sf->json = json;

    json->userdata = sf;
    json->string = on_string;
    json->number = on_number;
    json->primitive = on_primitive;
    json->begin = on_begin;
    json->end = on_end;

    struct stat st;
    if (fstat(fileno(f), &st))
        panic("fstat failed");

    ByteBuffer buf;
    buf.resize(st.st_size);

    int amt_read = fread(buf.raw(), 1, buf.length(), f);

    if (fclose(f))
        panic("fclose error");

    if (amt_read != buf.length())
        panic("error reading settings file");

    handle_parse_error(sf, lax_json_feed(json, amt_read, buf.raw()));
    handle_parse_error(sf, lax_json_eof(json));

    for (int i = 0; i < sf->open_windows.length(); i += 1) {
        SettingsFileOpenWindow *open_window = &sf->open_windows.at(i);
        if (open_window->perspective_index < 0 || open_window->perspective_index >= sf->perspectives.length()) {
            panic("window %d perspective index out of bounds: %d", i + 1, open_window->perspective_index);
        }
    }

    sf->json = nullptr;
    lax_json_destroy(json);

    return sf;
}

void settings_file_close(SettingsFile *sf) {
    for (int i = 0; i < sf->perspectives.length(); i += 1) {
        SettingsFilePerspective *perspective = &sf->perspectives.at(i);
        settings_file_clear_dock(&perspective->dock);
    }
    destroy(sf, 1);
}

void settings_file_clear_dock(SettingsFileDock *dock) {
    ok_or_panic(dock->tabs.resize(0));
    switch (dock->dock_type) {
        case SettingsFileDockTypeTabs:
            break;
        case SettingsFileDockTypeHoriz:
        case SettingsFileDockTypeVert:
            if (dock->child_a) {
                settings_file_clear_dock(dock->child_a);
                destroy(dock->child_a, 1);
                dock->child_a = nullptr;
            }
            if (dock->child_b) {
                settings_file_clear_dock(dock->child_b);
                destroy(dock->child_b, 1);
                dock->child_b = nullptr;
            }
            break;
    }
    dock->dock_type = SettingsFileDockTypeTabs;
}

int settings_file_commit(SettingsFile *sf) {
    OsTempFile tmp_file;
    int err = os_create_temp_file(path_dirname(sf->path).raw(), &tmp_file);
    if (err)
        return err;

    FILE *f = tmp_file.file;

    int indent = 0;
    json_line_comment(f, indent, "Genesis DAW configuration file");
    json_line_comment(f, indent, "This config file format is a superset of JSON. See");
    json_line_comment(f, indent, "https://github.com/andrewrk/liblaxjson for more details.");
    do_indent(f, indent);
    json_line_indent(f, &indent, "{");

    json_line_comment(f, indent, "your display name");
    json_line_str(f, indent, "user_name", sf->user_name.encode());
    fprintf(f, "\n");

    json_line_comment(f, indent, "your user id");
    json_line_uint256(f, indent, "user_id", sf->user_id);
    fprintf(f, "\n");

    json_line_comment(f, indent, "open this project on startup");
    json_line_uint256(f, indent, "open_project_id", sf->open_project_id);
    fprintf(f, "\n");

    json_line_comment(f, indent, "these perspectives are available for the user to choose from");
    json_line_perspectives(f, indent, "perspectives", sf->perspectives);
    fprintf(f, "\n");

    json_line_comment(f, indent, "open these windows on startup");
    json_line_open_windows(f, indent, "open_windows", sf->open_windows);
    fprintf(f, "\n");

    json_line_outdent(f, &indent, "}");

    if (fclose(f))
        return GenesisErrorFileAccess;

    err = os_rename_clobber(tmp_file.path.raw(), sf->path.raw());
    if (err)
        return err;

    return 0;
}

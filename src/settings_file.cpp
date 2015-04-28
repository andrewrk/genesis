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
                if (ByteBuffer::compare(value, "open_project_file") == 0) {
                    sf->state = SettingsFileStateOpenProjectFile;
                } else {
                    return parse_error(sf, "invalid setting name");
                }
                break;
            }
        case SettingsFileStateOpenProjectFile:
            sf->open_project_file = value;
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
    }
    return 0;
}

static int on_primitive(struct LaxJsonContext *json, enum LaxJsonType type) {
    SettingsFile *sf = (SettingsFile *) json->userdata;
    switch (sf->state) {
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
    }
    return 0;
}

static int on_end(struct LaxJsonContext *json, enum LaxJsonType type) {
    return 0;
}

static void json_line_str(FILE *f, int indent, const char *key, const ByteBuffer &value) {
    for (int i = 0; i < indent; i += 1)
        fprintf(f, " ");
    fprintf(f, "%s: \"", key);
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
    fprintf(f, "\",\n");
}

SettingsFile *settings_file_open(const ByteBuffer &path) {
    SettingsFile *sf = create_zero<SettingsFile>();
    if (!sf)
        panic("out of memory");

    sf->path = path;

    // default settings
    sf->open_project_file = "";

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

    lax_json_feed(json, amt_read, buf.raw());
    lax_json_eof(json);

    sf->json = nullptr;
    lax_json_destroy(json);

    return sf;
}

void settings_file_close(SettingsFile *sf) {
    destroy(sf, 1);
}

int settings_file_commit(SettingsFile *sf) {
    OsTempFile tmp_file;
    int err = os_create_temp_file(path_dirname(sf->path).raw(), &tmp_file);
    if (err)
        return err;

    FILE *f = tmp_file.file;

    fprintf(f, "// Genesis DAW configuration file\n");
    fprintf(f, "{\n");
    fprintf(f, "  // which file to load on startup\n");
    json_line_str(f, 2, "open_project_file", sf->open_project_file);
    fprintf(f, "}\n");

    if (fclose(f))
        return GenesisErrorFileAccess;

    err = os_rename_clobber(tmp_file.path.raw(), sf->path.raw());
    if (err)
        return err;

    return 0;
}

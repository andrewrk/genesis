#include "util.hpp"
#include "byte_buffer.hpp"

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

static const uint32_t whitespace[] = {9, 10, 11, 12, 13, 32, 133, 160, 5760,
    8192, 8193, 8194, 8195, 8196, 8197, 8198, 8199, 8200, 8201, 8202, 8232,
    8233, 8239, 8287, 12288};

struct UnicodeCharacter {
    uint32_t lower;
    uint32_t upper;
};

int main(int argc, char *argv[]) {
    char *unicode_data_text_path = argv[1];
    char *out_header_path = argv[2];

    FILE *out = fopen(out_header_path, "w");
    if (!out)
        panic("unable to write open %s", out_header_path);

    int fd = open(unicode_data_text_path, O_RDONLY);
    if (fd == -1)
        panic("unable to read open %s", unicode_data_text_path);

    struct stat st;
    if (fstat(fd, &st))
        panic("fstat failed");

    size_t file_size = st.st_size;

    FILE *f = fdopen(fd, "r");

    if (!f)
        panic("unable to open fd");

    ByteBuffer buffer;
    buffer.resize(file_size);

    size_t amt_read = fread(buffer.raw(), 1, file_size, f);
    if (amt_read != file_size)
        panic("unable to read");

    if (fclose(f))
        panic("unable to fclose in");

    List<ByteBuffer> lines;
    buffer.split("\n", lines);

    uint32_t max = 0;
    List<UnicodeCharacter> case_info_list;

    for (uint32_t i = 0; i < (uint32_t)lines.length(); i += 1) {
        List<ByteBuffer> line_fields;
        lines.at(i).split(";", line_fields);
        if (line_fields.length() < 15)
            break;

        uint32_t codepoint;
        sscanf(line_fields.at(0).raw(), "%X", &codepoint);

        while (codepoint > (uint32_t)case_info_list.length()) {
            case_info_list.append({ 0, 0 });
        }

        ByteBuffer upper_str = line_fields.at(12);
        ByteBuffer lower_str = line_fields.at(13);
        uint32_t lower = codepoint;
        uint32_t upper = codepoint;
        if (lower_str.length() > 0 || upper_str.length() > 0) {
            max = codepoint;
            sscanf(lower_str.raw(), "%X", &lower);
            sscanf(upper_str.raw(), "%X", &upper);
        }
        case_info_list.append({ lower, upper });
    }

    fprintf(out, "// This file is auto-generated.\n");
    fprintf(out, "#ifndef UNICODE_HPP\n");
    fprintf(out, "#define UNICODE_HPP\n");
    fprintf(out, "#include <stdint.h>\n");

    fprintf(out, "static const uint32_t whitespace[] = {\n");
    for (size_t i = 0; i < array_length(whitespace); i += 1) {
        fprintf(out, "  0x%x,\n", whitespace[i]);
    }
    fprintf(out, "};\n");

    fprintf(out, "struct UnicodeCharacter {\n");
    fprintf(out, "    uint32_t lower;\n");
    fprintf(out, "    uint32_t upper;\n");
    fprintf(out, "};\n");
    fprintf(out, "static const UnicodeCharacter unicode_characters[] = {\n");

    for (uint32_t i = 0; i < max; i += 1) {
        UnicodeCharacter *unicode_char = &case_info_list.at(i);
        fprintf(out, "  {0x%x, 0x%x},\n", unicode_char->lower, unicode_char->upper);
    }

    fprintf(out, "};\n");
    fprintf(out, "#endif\n");

    if (fclose(out))
        panic("unable to fclose out");

    return 0;
}

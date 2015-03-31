#include "debug.hpp"

void dump_rgba_img(const ByteBuffer &img, int w, int h, const char *filename) {
    const int header1_size = 14;
    const int header2_size = 40;
    const int channels = 4;
    const int bmp_size = channels * w * h;
    const int expected_size = header1_size + header2_size + bmp_size;

    ByteBuffer out_img;

    // header 1
    out_img.append("BM");
    out_img.append_uint32le(expected_size);
    out_img.append_uint16le(0);
    out_img.append_uint16le(0);
    out_img.append_uint32le(header1_size + header2_size);

    if (out_img.length() != header1_size)
        panic("header 1 size wrong");

    // header 2
    out_img.append_uint32le(header2_size);
    out_img.append_uint32le(w);
    out_img.append_uint32le(h);
    out_img.append_uint16le(1); // color planes
    out_img.append_uint16le(channels * 8); // bits per pixel
    out_img.append_uint32le(3); // no compression, channel bit masks
    out_img.append_uint32le(bmp_size);
    out_img.append_uint32le(2835); // horiz resolution
    out_img.append_uint32le(2835); // vert resolution
    out_img.append_uint32le(0); // number of palette colors
    out_img.append_uint32le(0); // important colors

    if (out_img.length() != header1_size + header2_size)
        panic("header 2 expected size: %d  actual size: %d", header2_size, out_img.length());

    // bmp files are stored upside down
    for (int y = h - 1; y >= 0; y -= 1) {
        for (int x = 0; x < w; x += 1) {
            int source_index = (y * w + x) * channels;
            uint8_t red   = img.read_uint8(source_index);
            uint8_t green = img.read_uint8(source_index + 1);
            uint8_t blue  = img.read_uint8(source_index + 2);
            uint8_t alpha = img.read_uint8(source_index + 3);
            out_img.append_uint8(blue);
            out_img.append_uint8(green);
            out_img.append_uint8(red);
            out_img.append_uint8(alpha);
        }
    }

    if (out_img.length() != expected_size)
        panic("expected size: %d  actual size: %d", expected_size, out_img.length());

    FILE *f = fopen(filename, "w");
    fwrite(out_img.raw(), 1, out_img.length(), f);
    fclose(f);
}


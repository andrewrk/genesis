#include "png_image.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include <png.h>

struct PngIo {
    long index;
    unsigned char *buffer;
    long size;
};

void read_png_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    PngIo *png_io = reinterpret_cast<PngIo*>(png_get_io_ptr(png_ptr));
    long new_index = png_io->index + length;
    if (new_index > png_io->size)
        panic("libpng trying to read beyond buffer");
    memcpy(data, png_io->buffer + png_io->index, length);
    png_io->index = new_index;
}

PngImage::PngImage(const ByteBuffer &compressed_bytes) {
    if (png_sig_cmp((png_bytep)compressed_bytes.raw(), 0, 8))
        panic("not png file");

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        panic("unable to create png read struct");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        panic("unable to create png info struct");

    // don't call any png_* functions outside of this function D:
    if (setjmp(png_jmpbuf(png_ptr)))
        panic("libpng has jumped the shark");

    png_set_sig_bytes(png_ptr, 8);

    PngIo png_io = {8, (unsigned char *)compressed_bytes.raw(), compressed_bytes.length()};
    png_set_read_fn(png_ptr, &png_io, read_png_data);

    png_read_info(png_ptr, info_ptr);

    _width  = png_get_image_width(png_ptr, info_ptr);
    _height = png_get_image_height(png_ptr, info_ptr);

    if (_width <= 0 || _height <= 0)
        panic("spritesheet image has no pixels");

    // bits per channel (not per pixel)
    int bits_per_channel = png_get_bit_depth(png_ptr, info_ptr);
    if (bits_per_channel != 8)
        panic("expected 8 bits per channel");

    int channel_count = png_get_channels(png_ptr, info_ptr);
    if (channel_count != 4)
        panic("expected 4 channels");

    int color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type != PNG_COLOR_TYPE_RGBA)
        panic("expected RGBA");

    _pitch = _width * bits_per_channel * channel_count / 8;
    _image_data.resize(_height * _pitch);
    png_bytep *row_ptrs = allocate<png_bytep>(_height);

    for (int i = 0; i < _height; i++) {
        png_uint_32 q = (_height - i - 1) * _pitch;
        row_ptrs[i] = (png_bytep)_image_data.raw() + q;
    }

    png_read_image(png_ptr, row_ptrs);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    destroy(row_ptrs, _height);
}

void PngImage::gl_pixel_store_alignment() const {
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
}

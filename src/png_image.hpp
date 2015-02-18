#ifndef PNG_IMAGE_HPP
#define PNG_IMAGE_HPP

#include "byte_buffer.hpp"

class PngImage {
public:
    // panics if the image could not be loaded
    PngImage(const ByteBuffer &compressed_bytes);
    ~PngImage() {}

    int _width;
    int _height;
    int _pitch;

    void gl_pixel_store_alignment() const;

    const char *raw() const {
        return _image_data.raw();
    }

private:
    ByteBuffer _image_data;
};

#endif

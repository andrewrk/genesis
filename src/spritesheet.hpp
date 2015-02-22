#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "shader_program.hpp"
#include "byte_buffer.hpp"
#include "hash_map.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class Gui;
class Spritesheet {
public:
    Spritesheet(Gui *gui, const ByteBuffer &key);
    ~Spritesheet();
    Spritesheet(const Spritesheet &copy) = delete;
    Spritesheet &operator=(const Spritesheet &copy) = delete;

    struct ImageInfo {
        int x;
        int y;
        int width;
        int height;
        float anchor_x;
        float anchor_y;
        bool r90;
        GLuint vertex_array;
        GLuint vertex_buffer;
        GLuint tex_coord_buffer;
    };

    const ImageInfo *get_image_info(const ByteBuffer &key) const;
    void draw(const ImageInfo *image, const glm::mat4 &mvp) const;

private:
    Gui *_gui;
    GLuint _texture_id;

    HashMap<ByteBuffer, ImageInfo, ByteBuffer::hash> _info_dict;
};

#endif

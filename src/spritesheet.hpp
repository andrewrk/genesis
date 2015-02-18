#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "shader_program.hpp"
#include "resource_bundle.hpp"
#include "byte_buffer.hpp"
#include "hash_map.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class Spritesheet {
public:
    Spritesheet(const ResourceBundle *bundle, const ByteBuffer &key);
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
    ShaderProgram _texture_shader_program;
    GLint _texture_attrib_tex_coord;
    GLint _texture_attrib_position;
    GLint _texture_uniform_mvp;
    GLint _texture_uniform_tex;

    GLuint _texture_id = 0;


    HashMap<ByteBuffer, ImageInfo, ByteBuffer::hash> _info_dict;
};

#endif

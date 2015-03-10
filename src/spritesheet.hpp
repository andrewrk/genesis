#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "shader_program.hpp"
#include "byte_buffer.hpp"
#include "hash_map.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class VertexArray;
class Spritesheet;
struct SpritesheetImage {
    int x;
    int y;
    int width;
    int height;
    float anchor_x;
    float anchor_y;
    bool r90;
    GLuint vertex_buffer;
    GLuint tex_coord_buffer;
    Spritesheet *spritesheet;
    VertexArray *vertex_array;
};

class Gui;
class GuiWindow;
class Spritesheet {
public:
    Spritesheet(Gui *gui, const ByteBuffer &key);
    ~Spritesheet();
    Spritesheet(const Spritesheet &copy) = delete;
    Spritesheet &operator=(const Spritesheet &copy) = delete;

    const SpritesheetImage *get_image_info(const ByteBuffer &key) const;
    void draw(GuiWindow *window, const SpritesheetImage *image, const glm::mat4 &mvp) const;

private:
    Gui *_gui;
    GLuint _texture_id;

    void init_vertex_array(SpritesheetImage *img);

    static void static_init_vertex_array(void *userdata) {
        SpritesheetImage *img = static_cast<SpritesheetImage *>(userdata);
        img->spritesheet->init_vertex_array(img);
    }

    HashMap<ByteBuffer, SpritesheetImage*, ByteBuffer::hash> _info_dict;
};

#endif

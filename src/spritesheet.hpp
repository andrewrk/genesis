#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "shader_program.hpp"
#include "byte_buffer.hpp"
#include "hash_map.hpp"
#include "glfw.hpp"

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

    HashMap<ByteBuffer, SpritesheetImage*, ByteBuffer::hash> _info_dict;
};

#endif

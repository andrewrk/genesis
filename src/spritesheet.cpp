#include "spritesheet.hpp"
#include "png_image.hpp"
#include "gui.hpp"

#include <rucksack/rucksack.h>

Spritesheet::Spritesheet(Gui *gui, const ByteBuffer &key) :
    _gui(gui),
    _texture_id(0)
{
    RuckSackBundle *bundle = gui->_resource_bundle->_bundle;

    RuckSackFileEntry *entry = rucksack_bundle_find_file(bundle, key.raw(), key.length());
    if (!entry)
        panic("Could not find resource %s in bundle", key.raw());

    RuckSackTexture *texture;
    int err = rucksack_file_open_texture(entry, &texture);
    if (err)
        panic("Unable to read '%s' as texture: %s", key.raw(), rucksack_err_str(err));

    // read the texture image
    ByteBuffer compressed_bytes;
    long size = rucksack_texture_size(texture);
    compressed_bytes.resize(size);
    err = rucksack_texture_read(texture, (unsigned char *)compressed_bytes.raw());
    if (err)
        panic("Unable to read texture '%s': %s", key.raw(), rucksack_err_str(err));

    PngImage tex_image(compressed_bytes);

    // make the opengl texture for it
    glGenTextures(1, &_texture_id);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex_image.gl_pixel_store_alignment();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
            tex_image._width, tex_image._height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, tex_image.raw());

    // read the images metadata
    List<RuckSackImage*> images;
    ok_or_panic(images.resize(rucksack_texture_image_count(texture)));
    rucksack_texture_get_images(texture, images.raw());
    float full_width = tex_image._width; 
    float full_height = tex_image._height;
    for (int i = 0; i < images.length(); i += 1) {
        RuckSackImage *image = images.at(i);

        SpritesheetImage *img = create<SpritesheetImage>();
        img->x = image->x;
        img->y = image->y;
        img->width = image->width;
        img->height = image->height;
        img->anchor_x = image->anchor_x;
        img->anchor_y = image->anchor_y;
        img->r90 = (image->r90 == 1);
        glGenBuffers(1, &img->vertex_buffer);
        glGenBuffers(1, &img->tex_coord_buffer);
        img->spritesheet = this;

        {
            GLfloat vertexes[4][3];
            if (image->r90) {
                vertexes[0][0] = image->width;
                vertexes[0][1] = image->height;
                vertexes[0][2] = 0.0f;

                vertexes[1][0] = 0.0f;
                vertexes[1][1] = image->height;
                vertexes[1][2] = 0.0f;

                vertexes[2][0] = image->width;
                vertexes[2][1] = 0.0f;
                vertexes[2][2] = 0.0f;

                vertexes[3][0] = 0.0f;
                vertexes[3][1] = 0.0f;
                vertexes[3][2] = 0.0f;
            } else {
                vertexes[0][0] = 0.0f;
                vertexes[0][1] = 0.0f;
                vertexes[0][2] = 0.0f;

                vertexes[1][0] = 0.0f;
                vertexes[1][1] = image->height;
                vertexes[1][2] = 0.0f;

                vertexes[2][0] = image->width;
                vertexes[2][1] = 0.0f;
                vertexes[2][2] = 0.0f;

                vertexes[3][0] = image->width;
                vertexes[3][1] = image->height;
                vertexes[3][2] = 0.0f;
            }
            glBindBuffer(GL_ARRAY_BUFFER, img->vertex_buffer);
            glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), vertexes, GL_STATIC_DRAW);
        }

        {
            GLfloat coords[4][2];
            coords[0][0] = image->x / full_width;
            coords[0][1] = (image->y + image->height) / full_height;

            coords[1][0] = image->x / full_width;
            coords[1][1] = image->y / full_height;

            coords[2][0] = (image->x + image->width) / full_width;
            coords[2][1] = (image->y + image->height) / full_height;

            coords[3][0] = (image->x + image->width) / full_width;
            coords[3][1] = image->y / full_height;
            glBindBuffer(GL_ARRAY_BUFFER, img->tex_coord_buffer);
            glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), coords, GL_STATIC_DRAW);
        }

        _info_dict.put(image->key, img);
    }

    rucksack_texture_close(texture);
}

Spritesheet::~Spritesheet() {
    auto it = _info_dict.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;
        destroy(entry->value, 1);
    }
}

void Spritesheet::draw(GuiWindow *window, const SpritesheetImage *image, const glm::mat4 &mvp) const {
    _gui->_shader_program_manager._texture_shader_program.bind();

    _gui->_shader_program_manager._texture_shader_program.set_uniform(
            _gui->_shader_program_manager._texture_uniform_mvp, mvp);

    _gui->_shader_program_manager._texture_shader_program.set_uniform(
            _gui->_shader_program_manager._texture_uniform_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, image->vertex_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager._texture_attrib_position);
    glVertexAttribPointer(_gui->_shader_program_manager._texture_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindBuffer(GL_ARRAY_BUFFER, image->tex_coord_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager._texture_attrib_tex_coord);
    glVertexAttribPointer(_gui->_shader_program_manager._texture_attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Spritesheet::draw_color(GuiWindow *window, const SpritesheetImage *image,
        const glm::mat4 &mvp, const glm::vec4 &color) const
{
    _gui->_shader_program_manager.texture_color_program.bind();

    _gui->_shader_program_manager.texture_color_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_mvp, mvp);

    _gui->_shader_program_manager.texture_color_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_tex, 0);

    _gui->_shader_program_manager.texture_color_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_color, color);

    glBindBuffer(GL_ARRAY_BUFFER, image->vertex_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager.texture_color_attrib_position);
    glVertexAttribPointer(_gui->_shader_program_manager.texture_color_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindBuffer(GL_ARRAY_BUFFER, image->tex_coord_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager.texture_color_attrib_tex_coord);
    glVertexAttribPointer(_gui->_shader_program_manager.texture_color_attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

const SpritesheetImage *Spritesheet::get_image_info(const ByteBuffer &key) const {
    return _info_dict.get(key);
}

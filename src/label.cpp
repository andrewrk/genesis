#include "label.hpp"
#include "gui.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>


static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

Label::Label(Gui *gui) :
    _gui(gui),
    _width(0),
    _height(0),
    _text("Label"),
    _color(1.0f, 1.0f, 1.0f, 1.0f),
    _font_size(16)
{

    glGenTextures(1, &_texture_id);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenVertexArrays(1, &_vertex_array);
    glBindVertexArray(_vertex_array);

    glGenBuffers(1, &_vertex_buffer);
    glGenBuffers(1, &_tex_coord_buffer);


    // send dummy vertex data - real data happens at update()
    GLfloat vertexes[4][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), vertexes, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(_gui->_attrib_position);
    glVertexAttribPointer(_gui->_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);


    GLfloat coords[4][2] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1},
    };
    glBindBuffer(GL_ARRAY_BUFFER, _tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), coords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(_gui->_attrib_tex_coord);
    glVertexAttribPointer(_gui->_attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    assert_no_gl_error();
}

Label::~Label() {
    glDeleteBuffers(1, &_tex_coord_buffer);
    glDeleteBuffers(1, &_vertex_buffer);
    glDeleteVertexArrays(1, &_vertex_array);
    glDeleteTextures(1, &_texture_id);
}

void Label::draw(const glm::mat4 &mvp) {
    _gui->_text_shader_program.bind();

    _gui->_text_shader_program.set_uniform(_gui->_uniform_color, _color);
    _gui->_text_shader_program.set_uniform(_gui->_uniform_tex, 0);
    _gui->_text_shader_program.set_uniform(_gui->_uniform_mvp, mvp);

    glBindVertexArray(_vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Label::update() {
    // one pass to determine width and height		
    // pen_x and pen_y are on the baseline. the char can go lower than it		
    float pen_x = 0.0f;
    float pen_y = 0.0f;
    int previous_glyph_index = 0;
    bool first = true;
    float above_size = 0.0f; // pixel count above the baseline
    float below_size = 0.0f; // pixel count below the baseline
    float bounding_width = 0.0f;
    for (int i = 0; i < _text.length(); i += 1) {
        uint32_t ch = _text.at(i);
        FontCacheKey key = {_font_size, ch};
        FontCacheValue entry = _gui->font_cache_entry(key);
        if (!first) {
            FT_Face face = _gui->_default_font_face;
            FT_Vector kerning;
            ft_ok(FT_Get_Kerning(face, previous_glyph_index, entry.glyph_index,
                        FT_KERNING_DEFAULT, &kerning));
            pen_x += ((float)kerning.x) / 64.0f;
        }
        first = false;

        float bmp_start_left = (float)entry.bitmap_glyph->left;
        float bmp_start_top = (float)entry.bitmap_glyph->top;
        FT_Bitmap bitmap = entry.bitmap_glyph->bitmap;
        float bmp_width = bitmap.width;
        float bmp_height = bitmap.rows;
        float right = ceilf(pen_x + bmp_start_left + bmp_width);
        float this_above_size = pen_y + bmp_start_top;
        float this_below_size = bmp_height - this_above_size;
        above_size = (this_above_size > above_size) ? this_above_size : above_size;
        below_size = (this_below_size > below_size) ? this_below_size : below_size;
        bounding_width = right;

        previous_glyph_index = entry.glyph_index;
        pen_x += ((float)entry.glyph->advance.x) / 65536.0f;
        pen_y += ((float)entry.glyph->advance.y) / 65536.0f;

    }

    float bounding_height = ceilf(above_size + below_size);

    panic("bounding_width: %d  bounding_height: %d", (int)bounding_width, (int)bounding_height);

    panic("TODO");
    /*
        let bounding_height = (above_size + below_size).ceil();		
		
        self.texture = glium::texture::Texture2d::new_empty(&self.renderer.display,		
            UncompressedFloatFormat::U8U8U8U8, bounding_width as u32, bounding_height as u32);		
        self.texture.as_surface().clear_color(0.0, 0.0, 0.0, 0.0);		
		
        self.vertex_buffer = glium::VertexBuffer::new(&self.renderer.display, vec![		
            Vertex { position: [ 0.0,            0.0,             0.0], tex_coords: [0.0, 0.0] },		
            Vertex { position: [ 0.0,            bounding_height, 0.0], tex_coords: [0.0, 1.0] },		
            Vertex { position: [ bounding_width, 0.0,             0.0], tex_coords: [1.0, 0.0] },		
            Vertex { position: [ bounding_width, bounding_height, 0.0], tex_coords: [1.0, 1.0] }		
        ]);		
		
        // second pass to render to texture		
        pen_x = 0.0;		
        pen_y = 0.0;		
        previous_glyph_index = 0;		
        first = true;		
        let projection = Matrix4::ortho(0.0, bounding_width, 0.0, bounding_height);		
        for ch in self.text.chars() {		
            let key = CacheKey {		
                face_index: self.face_index,		
                size: self.size,		
                ch: ch,		
            };		
            let cache_entry = self.renderer.get_cache_entry(key);		
		
            if first {		
                first = false;		
                let face_list = self.renderer.face_list.borrow();		
                let face = &face_list[self.face_index];		
                let kerning = face.get_kerning(previous_glyph_index,		
                                                       cache_entry.glyph_index,		
                                                       KerningDefault).unwrap();		
                pen_x += (kerning.x as f32) / 64.0;		
            }		
		
            let bmp_start_left = cache_entry.bitmap_glyph.left() as f32;		
            let bmp_start_top = cache_entry.bitmap_glyph.top() as f32;		
            let bitmap = cache_entry.bitmap_glyph.bitmap();		
            let bmp_width = bitmap.width() as f32;		
            let bmp_height = bitmap.rows() as f32;		
            let left = pen_x + bmp_start_left;		
            let top = above_size - bmp_start_top;		
            let model = Matrix4::identity().translate(left, top, 0.0);		
            let mvp = projection.mult(&model);		
            let texture = &cache_entry.texture;		
            let uniforms = Uniforms {		
                matrix: *mvp.as_array(),		
                texture: texture,		
                color: [0.0, 0.0, 0.0, 1.0],		
            };		
            let vertex_buffer = glium::VertexBuffer::new(&self.renderer.display, vec![		
                Vertex { position: [ 0.0,        0.0,           0.0], tex_coords: [0.0, 0.0] },		
                Vertex { position: [ 0.0,        bmp_height,    0.0], tex_coords: [0.0, 1.0] },		
                Vertex { position: [ bmp_width,  0.0,           0.0], tex_coords: [1.0, 0.0] },		
                Vertex { position: [ bmp_width,  bmp_height,    0.0], tex_coords: [1.0, 1.0] }		
            ]);		
		
            self.texture.as_surface().draw(&vertex_buffer, &self.renderer.index_buffer,		
                                           &self.renderer.program_gray, uniforms,		
                                           &Default::default()).ok().unwrap();		
		
            previous_glyph_index = cache_entry.glyph_index;		
            pen_x += (cache_entry.glyph.advance_x() as f32) / 65536.0;		
            pen_y += (cache_entry.glyph.advance_y() as f32) / 65536.0;		
        }		
*/
}

extern crate freetype;
extern crate glium;

use std::option::Option;
use std::collections::HashMap;
use std::rc::Rc;
use std::vec::Vec;
use std::default::Default;
use nalgebra::Mat4;

#[uniforms]
struct Uniforms<'a> {
    matrix: [[f32; 4]; 4],
    texture: &'a glium::texture::Texture2d,
}

struct CacheKey {
    face: &freetype::Face,
    size: i32,
    ch: char,
}

struct CacheValue {
    texture: glium::texture::Texture2D,
    glyph: freetype::glyph::Glyph,
    bitmap_glyph: freetype::glyph::BitmapGlyph,
}

#[vertex_format]
#[derive(Copy)]
struct Vertex {
    position: [f32; 2],
    tex_coords: [f32; 2],
}

struct TextRenderer {
    library: freetype::Library,
    display: &glium::Display,
    cache: HashMap<CacheKey, CacheValue>,
    vertex_buffer: glium::VertexBuffer<Vertex>,
    index_buffer: glium::IndexBuffer,
    program: glium::Program,
}

impl TextRenderer {
    fn new(&display) -> TextRenderer {
        let vertex_buffer = {
            glium::VertexBuffer::new(&display, 
                vec![
                    Vertex { position: [-1.0, -1.0], tex_coords: [0.0, 0.0] },
                    Vertex { position: [-1.0,  1.0], tex_coords: [0.0, 1.0] },
                    Vertex { position: [ 1.0,  1.0], tex_coords: [1.0, 1.0] },
                    Vertex { position: [ 1.0, -1.0], tex_coords: [1.0, 0.0] }
                ]
            )
        };

        let index_buffer = glium::IndexBuffer::new(&display,
            glium::index_buffer::TriangleStrip(vec![1 as u16, 2, 0, 3]));

        let program = glium::Program::from_source(&display, r"
            #version 110
            uniform mat4 matrix;
            attribute vec2 position;
            attribute vec2 tex_coords;
            varying vec2 v_tex_coords;
            void main() {
                gl_Position = matrix * vec4(position, 0.0, 1.0);
                v_tex_coords = tex_coords;
            }
        ", r"
            #version 110
            uniform sampler2D texture;
            varying vec2 v_tex_coords;
            void main() {
                gl_FragColor = texture2D(texture, v_tex_coords);
            }
        ", None).unwrap();

        let library = freetype::Library::init().unwrap();
        TextRenderer {
            library: library,
            display: display,
            vertex_buffer: vertex_buffer,
            index_buffer: index_buffer,
            program: program,
        }
    }

    fn load_face(&self, path: &Path) -> Option<freetype::Face> {
        self.library.new_face(path.as_str().unwrap(), 0)
    }

    fn create_label(&self, face: &freetype::Face, size: i32, text: &str) -> Label {
        let texture = glium::texture::Texture2d::new_empty(self.display,
            UncompressedFloatFormat::U8U8U8U8, 16, 16);
        Label {
            renderer: self,
            face: face,
            text: text,
            size: size,
            texture: texture,
        }
    }

    fn get_cache_entry(&self) -> CacheValue {
        match self.cache.get(&key) {
            Option::Some(entry) => entry,
            Option::None => {
                let glyph_index = self.font.face.get_char_index(ch);
                self.font.face.load_glyph(glyph_index, freetype::face::DEFAULT).unwrap();
                let glyph = self.font.face.glyph().get_glyph().unwrap();
                let bitmap_glyph = glyph.to_bitmap(freetype::render_mode::Normal,
                                                   Option::None).unwrap();
                let bitmap = bitmap_glyph.bitmap();
                let texture = glium::texture::Texture2D::new(self.font.renderer.display, bitmap);
                let cache_value = CacheValue {
                    texture: texture,
                    glyph: glyph,
                    bitmap_glyph: bitmap_glyph,
                };
                self.cache.insert(key, cache_value);
                cache_value
            },
        };
    }
}

struct CharAndPos {
    texture: glium::Texture,
    ch: char,
    /// x position of the character start, including possible whitespace
    glyph_left: f32,
    /// x position of the part you can see left
    char_left: f32,
    /// x position of the part you can see right
    char_right: f32,
    /// x position of the character end, including possible whitespace
    glyph_right: f32,

    glyph_top: f32,
    char_top: f32,
    char_bottom: f32,
    glyph_bottom: f32,
}

struct Label {
    renderer: &TextRenderer,
    text: &str,
    face: &freetype::Face,
    size: i32,
    uniforms: glium::uniforms::UniformsStorage,
    texture: glium::texture::Texture2D,
}

impl Label {
    fn set_face(&self, face: &freetype::Face) {
        self.face = face;
    }

    fn set_text(&self, text: &str) {
        self.text = text;
    }

    fn set_font_size(&self, size: i32) {
        self.size = size;
    }

    fn update(&self) {
        self.face.set_char_size(0, self.size * 64, 0, 0).unwrap();

        // one pass to determine width and height
        let mut x: f32 = 0;
        let mut y: f32 = 0;
        for ch in self.text.chars() {
            let key = CacheKey {
                font: self.font,
                size: self.size,
                ch: ch,
            };
            let cache_entry = self.renderer.get_cache_entry(&key);
            x += (cache_entry.glyph.advance().x >> 6);
            y += (cache_entry.glyph.advance().y >> 6);
        }
        let width = x;
        let height = y;
        self.texture = glium::texture::Texture2D::new_empty(self.renderer.display,
            UncompressedFloatFormat:U8U8U8U8, width, height);

        // second pass to render to texture
        x = 0;
        y = 0;
        for ch in self.text.chars() {
            let cache_entry = renderer.cache.get(&key).unwrap();
            let char_left = cache_entry.bitmap_glyph.left();
            let char_top = cache_entry.bitmap_glyph.top();

            let identity = Mat4::one();
            let translation = Pnt4::new(x + char_left, y - char_top, 0, 1);
            let matrix = nalgebra::translate(&identity, &translation);
            let uniforms = Uniforms {
                matrix: matrix,
                texture: cache_entry.texture,
            };

            self.texture.as_surface().draw(self.renderer.vertex_buffer, self.renderer.index_buffer,
                                          self.renderer.program, uniforms,
                                          &Default::default()).ok().unwrap();

            x += (cache_entry.glyph.advance().x >> 6);
            y += (cache_entry.glyph.advance().y >> 6);
        }
    }

    fn draw(&self, frame: &glium::Frame, matrix: Matrix) {
        let uniforms = Uniforms {
            matrix: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0f32]
                    ],
            texture: self.texture,
        };
        frame.draw(self.renderer.vertex_buffer, self.renderer.index_buffer,
                   self.renderer.program, uniforms,
                   &Default::default()).ok().unwrap();
    }
}

extern crate freetype;

use ::glium;
use ::glium_macros;

use std::option::Option;
use std::collections::HashMap;
use std::rc::Rc;
use std::vec::Vec;
use std::boxed::Box;
use std::default::Default;
use std::cmp::{Eq, PartialEq};
use std::hash::Hash;
use std::collections::hash_map::{Hasher, Entry};
use std::num::SignedInt;

use math3d::{Vector3, Matrix4};

use glium::texture::UncompressedFloatFormat;
use glium::texture::ClientFormat;
use glium::uniforms::IntoUniformValue;
use glium::Surface;

use self::freetype::glyph::Glyph;
use self::freetype::bitmap_glyph::BitmapGlyph;
use self::freetype::bitmap::PixelMode;
use self::freetype::bitmap::Bitmap;
use self::freetype::render_mode::RenderMode;

#[uniforms]
struct Uniforms<'a> {
    matrix: [[f32; 4]; 4],
    texture: &'a glium::texture::Texture2d,
}

#[derive(Eq, PartialEq, Hash, Copy)]
struct CacheKey<'a> {
    face: &'a freetype::Face,
    size: isize,
    ch: char,
}

struct CacheValue {
    texture: glium::texture::Texture2d,
    glyph: Glyph,
    bitmap_glyph: BitmapGlyph,
}

#[vertex_format]
#[derive(Copy)]
struct Vertex {
    position: [f32; 2],
    tex_coords: [f32; 2],
}

pub struct TextRenderer<'a> {
    library: freetype::Library,
    display: &'a glium::Display,
    cache: HashMap<CacheKey<'a>, Rc<Box<CacheValue>>>,
    vertex_buffer: glium::VertexBuffer<Vertex>,
    index_buffer: glium::IndexBuffer,
    program: glium::Program,
}

impl<'a> TextRenderer<'a> {
    pub fn new(display: &'a glium::Display) -> TextRenderer<'a> {
        let vertex_buffer = glium::VertexBuffer::new(display, vec![
                    Vertex { position: [-1.0, -1.0], tex_coords: [0.0, 0.0] },
                    Vertex { position: [-1.0,  1.0], tex_coords: [0.0, 1.0] },
                    Vertex { position: [ 1.0,  1.0], tex_coords: [1.0, 1.0] },
                    Vertex { position: [ 1.0, -1.0], tex_coords: [1.0, 0.0] }
                ]);

        let index_buffer = glium::IndexBuffer::new(display,
            glium::index_buffer::TriangleStrip(vec![1 as u16, 2, 0, 3]));

        let program = glium::Program::from_source(display, r"
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
            cache: HashMap::new(),
        }
    }

    pub fn load_face(&self, path: &Path) -> Result<freetype::Face, freetype::error::Error> {
        self.library.new_face(path, 0)
    }

    pub fn create_label(&'a mut self, face: &'a freetype::Face, size: isize, text: &'a str) -> Label {
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

    fn get_cache_entry(&mut self, key: CacheKey<'a>) -> Rc<Box<CacheValue>> {
        match self.cache.entry(key) {
            Entry::Occupied(entry) => entry.into_mut().clone(),
            Entry::Vacant(entry) => {
                let glyph_index = key.face.get_char_index(key.ch as usize);
                key.face.load_glyph(glyph_index, freetype::face::DEFAULT).unwrap();
                let glyph = key.face.glyph().get_glyph().unwrap();
                let bitmap_glyph = glyph.to_bitmap(RenderMode::Normal,
                                                   Option::None).unwrap();
                let bitmap = TexturableBitmap::new(bitmap_glyph.bitmap());
                let texture = glium::texture::Texture2d::new(self.display, bitmap);
                let cache_value = Rc::new(Box::new(CacheValue {
                    texture: texture,
                    glyph: glyph,
                    bitmap_glyph: bitmap_glyph,
                }));
                entry.insert(cache_value).clone()
            },
        }
    }
}

pub struct Label<'a> {
    renderer: &'a mut TextRenderer<'a>,
    text: &'a str,
    face: &'a freetype::Face,
    size: isize,
    texture: glium::texture::Texture2d,
}

impl<'a> Label<'a> {
    pub fn set_face(&'a mut self, face: &'a freetype::Face) {
        self.face = face;
    }

    pub fn set_text(&'a mut self, text: &'a str) {
        self.text = text;
    }

    pub fn set_font_size(&mut self, size: isize) {
        self.size = size;
    }

    pub fn update(&mut self) {
        self.face.set_char_size(0, self.size * 64, 0, 0).unwrap();

        // one pass to determine width and height
        let mut x: i32 = 0;
        let mut y: i32 = 0;
        let mut width: u32 = 0;
        let mut height: u32 = 0;
        for ch in self.text.chars() {
            let key = CacheKey {
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = self.renderer.get_cache_entry(key);
            let maybe_new_width = (x + cache_entry.bitmap_glyph.left()) as u32;
            let maybe_new_height = (y + cache_entry.bitmap_glyph.top()) as u32;
            width = if maybe_new_width > width {maybe_new_width} else {width};
            height = if maybe_new_height > height {maybe_new_height} else {height};
            x += (cache_entry.glyph.advance_x() >> 16) as i32;
            y += (cache_entry.glyph.advance_y() >> 16) as i32;
        }
        self.texture = glium::texture::Texture2d::new_empty(self.renderer.display,
            UncompressedFloatFormat::U8U8U8U8, width, height);

        // second pass to render to texture
        x = 0;
        y = 0;
        for ch in self.text.chars() {
            let key = CacheKey {
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = self.renderer.get_cache_entry(key);
            let char_left = cache_entry.bitmap_glyph.left();
            let char_top = cache_entry.bitmap_glyph.top();
            let advance_x = (cache_entry.glyph.advance_x() >> 16) as i32;
            let advance_y = (cache_entry.glyph.advance_x() >> 16) as i32;
            let texture = &cache_entry.texture;

            let mut matrix = Matrix4::identity();
            matrix.translate((x + char_left) as f32, (y - char_top) as f32, 0.0);
            let uniforms = Uniforms {
                matrix: *matrix.as_array(),
                texture: texture,
            };

            self.texture.as_surface().draw(&self.renderer.vertex_buffer, &self.renderer.index_buffer,
                                          &self.renderer.program, uniforms,
                                          &Default::default()).ok().unwrap();

            x += advance_x;
            y += advance_y;
        }
    }

    pub fn draw(&self, frame: &mut glium::Frame, matrix: &Matrix4) {
        let uniforms = Uniforms {
            matrix: *matrix.as_array(),
            texture: &self.texture,
        };
        frame.draw(&self.renderer.vertex_buffer, &self.renderer.index_buffer,
                   &self.renderer.program, uniforms,
                   &Default::default()).ok().unwrap();
    }
}

struct TexturableBitmap {
    bitmap: Bitmap,
}

impl TexturableBitmap {
    fn new(bitmap: Bitmap) -> Self {
        TexturableBitmap {
            bitmap: bitmap,
        }
    }
}

impl glium::texture::Texture2dData for TexturableBitmap {
    type Data = u8;

    fn get_format() -> ClientFormat {
        ClientFormat::U8
    }

    fn get_dimensions(&self) -> (u32, u32) {
        (self.bitmap.width() as u32, self.bitmap.rows() as u32)
    }

    fn into_vec(self) -> Vec<u8> {
        let signed_pitch = self.bitmap.pitch();
        enum Flow {
            Down,
            Up,
        }
        let flow = if signed_pitch.is_positive() {Flow::Down} else {Flow::Up};
        let pitch = signed_pitch.abs();
        match self.bitmap.pixel_mode() {
            PixelMode::Gray => {
                match flow {
                    Flow::Down => {
                        if pitch == self.bitmap.width() {
                            return self.bitmap.buffer().to_vec();
                        } else {
                            panic!("unsupported pitch != width");
                        }
                    },
                    Flow::Up => panic!("flow up unsupported"),
                }
            },
            PixelMode::Bgra => panic!("Bgra pixel mode"),
            _ => panic!("unexpected pixel mode: {:?}", self.bitmap.pixel_mode()),
        }
    }

    fn from_vec(buffer: Vec<u8>, width: u32) -> Self {
        panic!("why do we need from_vec?");
    }
}

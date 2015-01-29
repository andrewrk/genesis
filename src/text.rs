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
use std::num::{SignedInt, Float};

use math3d::{Vector3, Matrix4};

use glium::texture::UncompressedFloatFormat;
use glium::texture::ClientFormat;
use glium::uniforms::IntoUniformValue;
use glium::Surface;
use glium::index_buffer::TriangleStrip;

use self::freetype::glyph::Glyph;
use self::freetype::bitmap_glyph::BitmapGlyph;
use self::freetype::bitmap::PixelMode;
use self::freetype::bitmap::Bitmap;
use self::freetype::render_mode::RenderMode;
use self::freetype::face::KerningMode::KerningDefault;

#[uniforms]
struct Uniforms<'a> {
    matrix: [[f32; 4]; 4],
    texture: &'a glium::texture::Texture2d,
    color: [f32; 4],
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
    glyph_index: u32,
}

#[vertex_format]
#[derive(Copy)]
struct Vertex {
    position: [f32; 3],
    tex_coords: [f32; 2],
}

pub struct TextRenderer<'a> {
    library: freetype::Library,
    display: &'a glium::Display,
    cache: HashMap<CacheKey<'a>, Rc<Box<CacheValue>>>,
    index_buffer: glium::IndexBuffer,
    program: glium::Program,
}


impl<'a> TextRenderer<'a> {
    pub fn new(display: &'a glium::Display) -> TextRenderer<'a> {
        let index_buffer = glium::IndexBuffer::new(display, TriangleStrip(vec![0 as u16, 1, 2, 3]));

        let program = glium::Program::from_source(display, r"
            #version 110
            uniform mat4 matrix;
            attribute vec3 position;
            attribute vec2 tex_coords;
            varying vec2 v_tex_coords;
            void main() {
                gl_Position = matrix * vec4(position, 1.0);
                v_tex_coords = tex_coords;
            }
        ", r"
            #version 110
            uniform sampler2D texture;
            uniform vec4 color;
            varying vec2 v_tex_coords;
            void main() {
                gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(texture, v_tex_coords).a) * color;
            }
        ", None).unwrap();

        let library = freetype::Library::init().unwrap();
        TextRenderer {
            library: library,
            display: display,
            program: program,
            cache: HashMap::new(),
            index_buffer: index_buffer,
        }
    }

    pub fn load_face(&self, path: &Path) -> Result<freetype::Face, freetype::error::Error> {
        self.library.new_face(path, 0)
    }

    pub fn create_label(&'a mut self, face: &'a freetype::Face) -> Label {
        let texture = glium::texture::Texture2d::new_empty(self.display,
            UncompressedFloatFormat::U8U8U8U8, 16, 16);
        let vertex_buffer = glium::VertexBuffer::new(self.display, vec![
            Vertex { position: [ 0.0,          0.0,           0.0], tex_coords: [0.0, 0.0] },
            Vertex { position: [ 0.0,          16.0 as f32, 0.0], tex_coords: [0.0, 1.0] },
            Vertex { position: [ 16.0 as f32, 1.0,           0.0], tex_coords: [1.0, 0.0] },
            Vertex { position: [ 16.0 as f32, 16.0 as f32, 0.0], tex_coords: [1.0, 1.0] }
        ]);
        Label {
            renderer: self,
            face: face,
            text: String::from_str("label"),
            size: 16,
            texture: texture,
            color: [1.0, 1.0, 1.0, 1.0],
            vertex_buffer: vertex_buffer,
        }
    }

    fn get_cache_entry(&mut self, key: CacheKey<'a>) -> Rc<Box<CacheValue>> {
        match self.cache.entry(key) {
            Entry::Occupied(entry) => entry.into_mut().clone(),
            Entry::Vacant(entry) => {
                key.face.set_char_size(0, key.size * 64, 0, 0).unwrap();
                let glyph_index = key.face.get_char_index(key.ch as usize);
                key.face.load_glyph(glyph_index, freetype::face::RENDER).unwrap();
                let glyph_slot = key.face.glyph();
                let glyph = glyph_slot.get_glyph().unwrap();
                let bitmap_glyph = glyph.to_bitmap(RenderMode::Normal, Option::None).unwrap();
                let texturable_bitmap = TexturableBitmap::new(bitmap_glyph.bitmap());
                let texture = glium::texture::Texture2d::new(self.display, texturable_bitmap);
                let cache_value = Rc::new(Box::new(CacheValue {
                    texture: texture,
                    glyph: glyph,
                    bitmap_glyph: bitmap_glyph,
                    glyph_index: glyph_index,
                }));
                entry.insert(cache_value).clone()
            },
        }
    }
}

pub struct Label<'a> {
    renderer: &'a mut TextRenderer<'a>,
    text: String,
    face: &'a freetype::Face,
    size: isize,
    texture: glium::texture::Texture2d,
    color: [f32; 4],
    vertex_buffer: glium::VertexBuffer<Vertex>,
}

impl<'a> Label<'a> {
    pub fn set_face(&'a mut self, face: &'a freetype::Face) {
        self.face = face;
    }

    pub fn set_text(&mut self, text: String) {
        self.text = text;
    }

    pub fn set_font_size(&mut self, size: isize) {
        self.size = size;
    }

    pub fn set_color(&mut self, red: f32, green: f32, blue: f32, alpha: f32) {
        self.color = [red, green, blue, alpha];
    }

    pub fn update(&mut self) {
        // one pass to determine width and height
        let mut pen_x: f32 = 0.0;
        let mut pen_y: f32 = 0.0;
        let mut previous_glyph_index = 0;
        let mut first = true;
        let mut width: f32 = 0.0;
        let mut height: f32 = 0.0;
        for ch in self.text.chars() {
            let key = CacheKey {
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = self.renderer.get_cache_entry(key);

            if first {
                first = false;
                let kerning = self.face.get_kerning(previous_glyph_index,
                                                       cache_entry.glyph_index,
                                                       KerningDefault).unwrap();
                pen_x += (kerning.x as f32) / 64.0;
            }

            let bmp_start_left = cache_entry.bitmap_glyph.left() as f32;
            let bmp_start_top = cache_entry.bitmap_glyph.top() as f32;
            let bitmap = cache_entry.bitmap_glyph.bitmap();
            let bmp_width = bitmap.width() as f32;
            let bmp_height = bitmap.rows() as f32;
            let right = (pen_x + bmp_start_left + bmp_width).ceil();
            let top = (pen_y + bmp_start_top + bmp_height).ceil();

            width = if right > width {right} else {width};
            height = if top > height {top} else {height};

            previous_glyph_index = cache_entry.glyph_index;
            pen_x += (cache_entry.glyph.advance_x() as f32) / 65536.0;
            pen_y += (cache_entry.glyph.advance_y() as f32) / 65536.0;
        }

        self.texture = glium::texture::Texture2d::new_empty(self.renderer.display,
            UncompressedFloatFormat::U8U8U8U8, width as u32, height as u32);

        self.vertex_buffer = glium::VertexBuffer::new(self.renderer.display, vec![
            Vertex { position: [ 0.0,   0.0,    0.0], tex_coords: [0.0, 0.0] },
            Vertex { position: [ 0.0,   height, 0.0], tex_coords: [0.0, 1.0] },
            Vertex { position: [ width, 0.0,    0.0], tex_coords: [1.0, 0.0] },
            Vertex { position: [ width, height, 0.0], tex_coords: [1.0, 1.0] }
        ]);

        // second pass to render to texture
        pen_x = 0.0;
        pen_y = 0.0;
        previous_glyph_index = 0;
        first = true;
        let projection = Matrix4::ortho(0.0, width, height, 0.0);
        println!("projection:\n{:?}", projection);
        for ch in self.text.chars() {
            let key = CacheKey {
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = self.renderer.get_cache_entry(key);

            if first {
                first = false;
                let kerning = self.face.get_kerning(previous_glyph_index,
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
            let top = pen_y + bmp_start_top + bmp_height;


            let texture = &cache_entry.texture;
            let model = Matrix4::identity().translate(left, -top, 0.0);
            let mvp = projection.mult(&model);
            let uniforms = Uniforms {
                matrix: *mvp.as_array(),
                texture: texture,
                color: [1.0, 1.0, 1.0, 1.0],
            };
            let vertex_buffer = glium::VertexBuffer::new(self.renderer.display, vec![
                Vertex { position: [ 0.0,        0.0,           0.0], tex_coords: [0.0, 0.0] },
                Vertex { position: [ 0.0,        bmp_height,    0.0], tex_coords: [0.0, 1.0] },
                Vertex { position: [ bmp_width,  0.0,           0.0], tex_coords: [1.0, 0.0] },
                Vertex { position: [ bmp_width,  bmp_height,    0.0], tex_coords: [1.0, 1.0] }
            ]);

            self.texture.as_surface().draw(&vertex_buffer, &self.renderer.index_buffer,
                                          &self.renderer.program, uniforms,
                                          &Default::default()).ok().unwrap();

            previous_glyph_index = cache_entry.glyph_index;
            pen_x += (cache_entry.glyph.advance_x() as f32) / 65536.0;
            pen_y += (cache_entry.glyph.advance_y() as f32) / 65536.0;
        }
    }

    pub fn draw(&self, frame: &mut glium::Frame, matrix: &Matrix4) {
        let uniforms = Uniforms {
            matrix: *matrix.as_array(),
            texture: &self.texture,
            color: self.color,
        };
        frame.draw(&self.vertex_buffer, &self.renderer.index_buffer,
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

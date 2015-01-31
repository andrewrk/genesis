extern crate freetype;
extern crate image;

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
use glium::LinearBlendingFactor;

use self::freetype::face::Face;
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
    program_gray: glium::Program,
    program_color: glium::Program,
    draw_params: glium::DrawParameters,
}


impl<'a> TextRenderer<'a> {
    pub fn new(display: &'a glium::Display) -> TextRenderer<'a> {
        let index_buffer = glium::IndexBuffer::new(display, TriangleStrip(vec![0 as u16, 1, 2, 3]));

        let program_gray = glium::Program::from_source(display, r"
            #version 110
            uniform mat4 matrix;
            attribute vec3 position;
            attribute vec2 tex_coords;
            varying vec2 v_tex_coords;
            void main() {
                gl_Position = vec4(position, 1.0) * matrix;
                v_tex_coords = tex_coords;
            }
        ", r"
            #version 110
            uniform sampler2D texture;
            uniform vec4 color;
            varying vec2 v_tex_coords;
            void main() {
                gl_FragColor = vec4(color.rgb, color.a * texture2D(texture, v_tex_coords));
            }
        ", None).unwrap();
        let program_color = glium::Program::from_source(display, r"
            #version 110
            uniform mat4 matrix;
            attribute vec3 position;
            attribute vec2 tex_coords;
            varying vec2 v_tex_coords;
            void main() {
                gl_Position = vec4(position, 1.0) * matrix;
                v_tex_coords = tex_coords;
            }
        ", r"
            #version 110
            uniform sampler2D texture;
            uniform vec4 color;
            varying vec2 v_tex_coords;
            void main() {
                gl_FragColor = vec4(color.rgb, texture2D(texture, v_tex_coords).a * color.a);
            }
        ", None).unwrap();

        let library = freetype::Library::init().unwrap();
        TextRenderer {
            library: library,
            display: display,
            program_color: program_color,
            program_gray: program_gray,
            cache: HashMap::new(),
            index_buffer: index_buffer,
            draw_params: glium::DrawParameters {
                blending_function: Option::Some(glium::BlendingFunction::Addition{
                    source: LinearBlendingFactor::SourceAlpha,
                    destination: LinearBlendingFactor::OneMinusSourceAlpha,
                }),
                .. Default::default()
            },
        }
    }

    pub fn load_face(&self, path: &Path) -> Result<freetype::Face, freetype::error::Error> {
        self.library.new_face(path, 0)
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
    text: String,
    face: &'a freetype::Face,
    size: isize,
    texture: glium::texture::Texture2d,
    color: [f32; 4],
    vertex_buffer: glium::VertexBuffer<Vertex>,
}

impl<'a> Label<'a> {
    pub fn new(renderer: &TextRenderer, face: &'a Face) -> Label<'a> {
        let texture = glium::texture::Texture2d::new_empty(renderer.display,
            UncompressedFloatFormat::U8U8U8U8, 16, 16);
        let vertex_buffer = glium::VertexBuffer::new(renderer.display, vec![
            Vertex { position: [ 0.0,     0.0,     0.0], tex_coords: [0.0, 0.0] },
            Vertex { position: [ 0.0,     16.0,    0.0], tex_coords: [0.0, 1.0] },
            Vertex { position: [ 16.0,    0.0,     0.0], tex_coords: [1.0, 0.0] },
            Vertex { position: [ 16.0,    16.0,    0.0], tex_coords: [1.0, 1.0] }
        ]);
        Label {
            face: face,
            text: String::from_str("label"),
            size: 16,
            texture: texture,
            color: [1.0, 1.0, 1.0, 1.0],
            vertex_buffer: vertex_buffer,
        }
    }

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

    pub fn update(&mut self, renderer: &mut TextRenderer<'a>) {
        // one pass to determine width and height
        // pen_x and pen_y are on the baseline. the char can go lower than it
        let mut pen_x: f32 = 0.0;
        let mut pen_y: f32 = 0.0;
        let mut previous_glyph_index = 0;
        let mut first = true;
        let mut above_size: f32 = 0.0; // pixel count above the baseline
        let mut below_size: f32 = 0.0; // pixel count below the baseline
        let mut bounding_width = 0.0;
        for ch in self.text.chars() {
            let key = CacheKey {
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = renderer.get_cache_entry(key);

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
            let this_above_size = pen_y + bmp_start_top;
            let this_below_size = bmp_height - this_above_size;
            above_size = if this_above_size > above_size {this_above_size} else {above_size};
            below_size = if this_below_size > below_size {this_below_size} else {below_size};
            bounding_width = right;

            previous_glyph_index = cache_entry.glyph_index;
            pen_x += (cache_entry.glyph.advance_x() as f32) / 65536.0;
            pen_y += (cache_entry.glyph.advance_y() as f32) / 65536.0;
        }
        let bounding_height = (above_size + below_size).ceil();

        self.texture = glium::texture::Texture2d::new_empty(renderer.display,
            UncompressedFloatFormat::U8U8U8U8, bounding_width as u32, bounding_height as u32);
        self.texture.as_surface().clear_color(0.0, 0.0, 0.0, 0.0);

        self.vertex_buffer = glium::VertexBuffer::new(renderer.display, vec![
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
                face: self.face,
                size: self.size,
                ch: ch,
            };
            let cache_entry = renderer.get_cache_entry(key);

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
            let top = above_size - bmp_start_top;
            let model = Matrix4::identity().translate(left, top, 0.0);
            let mvp = projection.mult(&model);
            let texture = &cache_entry.texture;
            let uniforms = Uniforms {
                matrix: *mvp.as_array(),
                texture: texture,
                color: [0.0, 0.0, 0.0, 1.0],
            };
            let vertex_buffer = glium::VertexBuffer::new(renderer.display, vec![
                Vertex { position: [ 0.0,        0.0,           0.0], tex_coords: [0.0, 0.0] },
                Vertex { position: [ 0.0,        bmp_height,    0.0], tex_coords: [0.0, 1.0] },
                Vertex { position: [ bmp_width,  0.0,           0.0], tex_coords: [1.0, 0.0] },
                Vertex { position: [ bmp_width,  bmp_height,    0.0], tex_coords: [1.0, 1.0] }
            ]);

            self.texture.as_surface().draw(&vertex_buffer, &renderer.index_buffer,
                                           &renderer.program_gray, uniforms,
                                           &Default::default()).ok().unwrap();

            previous_glyph_index = cache_entry.glyph_index;
            pen_x += (cache_entry.glyph.advance_x() as f32) / 65536.0;
            pen_y += (cache_entry.glyph.advance_y() as f32) / 65536.0;
        }
    }

    pub fn draw(&self, renderer: &TextRenderer, frame: &mut glium::Frame, matrix: &Matrix4) {
        let uniforms = Uniforms {
            matrix: *matrix.as_array(),
            texture: &self.texture,
            color: self.color,
        };
        frame.draw(&self.vertex_buffer, &renderer.index_buffer,
                   &renderer.program_color, uniforms,
                   &renderer.draw_params).ok().unwrap();
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
        let flow = if signed_pitch >= 0 {Flow::Down} else {Flow::Up};
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

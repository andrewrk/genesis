#![allow(unstable)]
#![feature(plugin)]

extern crate gfx;

#[macro_use]
#[plugin]
extern crate gfx_macros;

extern crate glfw;
extern crate groove;

use gfx::{DeviceHelper, ToSlice};
use glfw::Context;
use std::vec::Vec;
use std::option::Option;
use std::result::Result;
use std::thread::Thread;
use std::sync::Arc;
use std::sync::RwLock;

#[vertex_format]
#[derive(Copy)]
struct Vertex {
    #[name = "a_Pos"]
    pos: [f32; 2],

    #[name = "a_Color"]
    color: [f32; 3],
}

static VERTEX_SRC: gfx::ShaderSource<'static> = shaders! {
glsl_120: b"
    #version 120
    attribute vec2 a_Pos;
    attribute vec3 a_Color;
    varying vec4 v_Color;
    void main() {
        v_Color = vec4(a_Color, 1.0);
        gl_Position = vec4(a_Pos, 0.0, 1.0);
    }
",
glsl_150: b"
    #version 150 core
    in vec2 a_Pos;
    in vec3 a_Color;
    out vec4 v_Color;
    void main() {
        v_Color = vec4(a_Color, 1.0);
        gl_Position = vec4(a_Pos, 0.0, 1.0);
    }
"
};

static FRAGMENT_SRC: gfx::ShaderSource<'static> = shaders! {
glsl_120: b"
    #version 120
    varying vec4 v_Color;
    void main() {
        gl_FragColor = v_Color;
    }
",
glsl_150: b"
    #version 150 core
    in vec4 v_Color;
    out vec4 o_Color;
    void main() {
        o_Color = v_Color;
    }
"
};

fn main() {
    let mut stderr = &mut std::io::stderr();
    let args = std::os::args_as_bytes();
    let exe = String::from_utf8_lossy(args[0].as_slice());

    if args.len() != 2 {
        print_usage(stderr, exe.as_slice());
        std::os::set_exit_status(1);
        return;
    }
    let input_path = Path::new(args[1].as_slice());

    let waveform = Waveform::new(input_path);

    let mut glfw = glfw::init(glfw::FAIL_ON_ERRORS).unwrap();
    glfw.window_hint(glfw::WindowHint::ContextVersion(3, 2));
    glfw.window_hint(glfw::WindowHint::OpenglForwardCompat(true));
    glfw.window_hint(glfw::WindowHint::OpenglProfile(glfw::OpenGlProfileHint::Core));

    let (mut window, events) = glfw
        .create_window(640, 480, "genesis", glfw::WindowMode::Windowed)
        .expect("Failed to create GLFW window.");

    window.make_current();

    glfw.set_error_callback(glfw::FAIL_ON_ERRORS);
    window.set_key_polling(true);

    let (w, h) = window.get_framebuffer_size();
    let frame = gfx::Frame::new(w as u16, h as u16);

    let mut device = gfx::GlDevice::new(|s| window.get_proc_address(s));

    let vertex_data = [
        Vertex { pos: [ -0.5, -0.5 ], color: [1.0, 0.0, 0.0] },
        Vertex { pos: [  0.5, -0.5 ], color: [0.0, 1.0, 0.0] },
        Vertex { pos: [  0.0,  0.5 ], color: [0.0, 0.0, 1.0] },
    ];

    let mesh = device.create_mesh(&vertex_data);
    let slice = mesh.to_slice(gfx::PrimitiveType::TriangleList);

    let program = device.link_program(VERTEX_SRC.clone(), FRAGMENT_SRC.clone()).unwrap();

    let mut graphics = gfx::Graphics::new(device);
    let batch: gfx::batch::RefBatch<(), ()> = graphics.make_batch(
        &program, &mesh, slice, &gfx::DrawState::new()).unwrap();

    let clear_data = gfx::ClearData {
        color: [0.3, 0.3, 0.3, 1.0],
        depth: 1.0,
        stencil: 0,
    };

    while !window.should_close() {
        glfw.poll_events();

        for (_, event) in glfw::flush_messages(&events) {
            match event {
                glfw::WindowEvent::Key(glfw::Key::Escape, _, glfw::Action::Press, _) =>
                    window.set_should_close(true),
                _ => {},
            }
        }

        graphics.clear(clear_data, gfx::COLOR, &frame);
        graphics.draw(&batch, &(), &frame);
        graphics.end_frame();

        waveform.read().unwrap().display();

        window.swap_buffers();
    }
}

fn print_usage(stderr: &mut std::io::LineBufferedWriter<std::io::stdio::StdWriter>, exe: &str) {
    let _ = write!(stderr, "Usage: {} <file>\n", exe);
}

enum WaveformLoadState {
    Error,
    Spawning,
    Opening,
    Reading,
    Complete,
}

struct Waveform {
    buffers: Vec<groove::DecodedBuffer>,
    load_state: WaveformLoadState,
}

impl Waveform {
    fn new(path: Path) -> Arc<RwLock<Self>> {
        let waveform_arc = Arc::new(RwLock::new(Waveform {
            load_state: WaveformLoadState::Spawning,
            buffers: Vec::new(),
        }));
        let waveform_rw = waveform_arc.clone();
        Thread::spawn(move || {
            let set_load_state = |&: state: WaveformLoadState| {
                let mut waveform = waveform_rw.write().unwrap();
                waveform.load_state = state;
            };
            set_load_state(WaveformLoadState::Opening);
            let file = match groove::File::open(&path) {
                Option::Some(f) => f,
                Option::None => {
                    set_load_state(WaveformLoadState::Error);
                    panic!("unable to open file");
                },
            };
            set_load_state(WaveformLoadState::Reading);

            let playlist = groove::Playlist::new();
            let sink = groove::Sink::new();
            sink.set_audio_format(groove::AudioFormat {
                sample_rate: 44100,
                channel_layout: groove::ChannelLayout::LayoutStereo,
                sample_fmt: groove::SampleFormat {
                    sample_type: groove::SampleType::Dbl,
                    planar: false,
                },
            });
            match sink.attach(&playlist) {
                Result::Ok(_) => {},
                Result::Err(_) => {
                    set_load_state(WaveformLoadState::Error);
                    panic!("error attaching sink");
                }
            }
            playlist.append(&file, 1.0, 1.0);

            loop {
                match sink.buffer_get_blocking() {
                    Option::Some(decoded_buffer) => {
                        let mut waveform = waveform_rw.write().unwrap();
                        waveform.buffers.push(decoded_buffer);
                    },
                    Option::None => break,
                }
            }
            set_load_state(WaveformLoadState::Complete);
        });
        waveform_arc
    }

    fn display(&self) {
        //println!("waveform display");
    }
}

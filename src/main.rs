#![allow(unstable)]
#![feature(plugin)]

#[plugin]
extern crate glium_macros;

extern crate glutin;

#[macro_use]
extern crate glium;

extern crate groove;

use glium::Surface;
use glium::DisplayBuild;
use std::vec::Vec;
use std::option::Option;
use std::result::Result;
use std::thread::Thread;
use std::sync::Arc;
use std::sync::RwLock;
use std::io::timer;
use std::time::Duration;

#[vertex_format]
#[derive(Copy)]
struct Vertex {
    position: [f32; 2],
    color: [f32; 3],
}

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


    // building the display, ie. the main object
    let display = glutin::WindowBuilder::new()
        .with_title(String::from_str("genesis"))
        .build_glium()
        .unwrap();

    // building the vertex buffer, which contains all the vertices that we will draw
    let vertex_buffer = glium::VertexBuffer::new(&display,
            vec![
                Vertex { position: [-0.5, -0.5], color: [0.0, 1.0, 0.0] },
                Vertex { position: [ 0.0,  0.5], color: [0.0, 0.0, 1.0] },
                Vertex { position: [ 0.5, -0.5], color: [1.0, 0.0, 0.0] },
            ]);

    // building the index buffer
    let index_buffer = glium::IndexBuffer::new(&display,
        glium::index_buffer::TrianglesList(vec![0u16, 1, 2]));

    // compiling shaders and linking them together
    let program = glium::Program::from_source(&display,
        // vertex shader
        "
            #version 110
            uniform mat4 matrix;
            attribute vec2 position;
            attribute vec3 color;
            varying vec3 vColor;
            void main() {
                gl_Position = vec4(position, 0.0, 1.0) * matrix;
                vColor = color;
            }
        ",

        // fragment shader
        "
            #version 110
            varying vec3 vColor;
            void main() {
                gl_FragColor = vec4(vColor, 1.0);
            }
        ",

        // geometry shader
        None)
        .unwrap();

    'main: loop {
        // building the uniforms
        let uniforms = uniform! {
            matrix: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0f32]
                    ]
        };

        // drawing a frame
        let mut target = display.draw();
        target.clear_color(0.0, 0.0, 0.0, 0.0);
        target.draw(&vertex_buffer, &index_buffer, &program, &uniforms,
                    &std::default::Default::default()).unwrap();
        target.finish();

        // sleeping for some time in order not to use up too much CPU
        timer::sleep(Duration::milliseconds(17));

        // polling and handling the events received by the window
        for event in display.poll_events() {
            match event {
                glutin::Event::Closed => break 'main,
                glutin::Event::KeyboardInput(_, _, Some(glutin::VirtualKeyCode::Escape))
                    => break 'main,
                _ => (),
            }
        }
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

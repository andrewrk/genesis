// dear rust. yes I would like to use you. thank you. -andy
#![feature(plugin)]
#![feature(hash)]
#![feature(std_misc)]
#![feature(core)]
#![feature(io)]
#![feature(os)]
#![feature(path)]
#![feature(collections)]

#[plugin]
extern crate glium_macros;

extern crate glutin;

#[macro_use]
extern crate glium;

extern crate groove;
extern crate math3d;

mod text;

use text::{Label, Gui};

use glium::{Surface, Display, DisplayBuild};

use glutin::Event;
use glutin::VirtualKeyCode;

use std::vec::Vec;
use std::option::Option;
use std::option::Option::{Some, None};
use std::result::Result;
use std::result::Result::{Err, Ok};
use std::thread::Thread;
use std::sync::Arc;
use std::sync::RwLock;

use math3d::{Matrix4};

use groove::SampleFormat;

fn main() {
    let mut stderr = &mut std::old_io::stderr();
    let args = std::os::args_as_bytes();
    let exe = String::from_utf8_lossy(args[0].as_slice());

    if args.len() != 2 {
        print_usage(stderr, exe.as_slice());
        std::os::set_exit_status(1);
        return;
    }
    let input_path = Path::new(args[1].as_slice());

    // building the display, ie. the main object
    let display = glutin::WindowBuilder::new()
        .with_title(String::from_str("genesis"))
        .build_glium()
        .unwrap();

    let gui = Gui::new(display.clone());
    let open_sans_regular = gui.load_face(&Path::new("./assets/OpenSans-Regular.ttf"))
        .ok().expect("failed to load font");

    let label = gui.create_label(open_sans_regular);
    label.item().set_text(String::from_str("abcdefghijklmnopqrstuvwxyz"));
    label.item().set_color(1.0, 1.0, 1.0, 1.0);
    label.item().update();

    let label2 = gui.create_label(open_sans_regular);
    label2.item().set_text(String::from_str("hurray, font rendering!"));
    label2.item().set_color(0.0, 0.0, 1.0, 1.0);
    label2.item().update();

    'main: loop {
        // polling and handling the events received by the window
        for event in display.poll_events() {
            match event {
                Event::Closed => break 'main,
                Event::KeyboardInput(_, _, Some(key_code)) => {
                    match key_code {
                        VirtualKeyCode::Escape => break 'main,
                        VirtualKeyCode::Left => {
                        },
                        VirtualKeyCode::Right => {
                        },
                        VirtualKeyCode::Up => {
                        },
                        VirtualKeyCode::Down => {
                        },
                        _ => (),
                    }
                },
                Event::Resized(_, _) => gui.resize(),
                _ => (),
            }
        }
        gui.draw_frame();
    }
}

fn print_usage(stderr: &mut std::old_io::LineBufferedWriter<std::old_io::stdio::StdWriter>, exe: &str) {
    let _ = write!(stderr, "Usage: {} <file>\n", exe);
}

/*
struct AudioChannel {
    samples: Vec<f64>,
    sample_rate: i32,
}

impl AudioChannel {
    fn new(sample_rate: i32) -> Self {
        AudioChannel {
            samples: Vec::new(),
            sample_rate: i32,
        }
    }
}

#[derive(Copy, Debug)]
enum AudioFileError {
    OpenFail,
    AttachSinkFail,
    UnsupportedAudioFormat,
}

struct AudioFile {
    channels: Vec<AudioChannel>,
}

impl AudioFile {
    fn from_path(path: &Path) -> Result<Self, AudioFileError> {
        let file = match groove::File::open(&path) {
            Some(f) => f,
            None => return Err(AudioFileError::OpenFail),
        };
        let audio_format = file.audio_format();
        let channel_count = audio_format.channel_layout.count();
        let channels = Vec::with_capacity(channel_count);
        for i in 0..channel_count {
            let audio_channel = AudioChannel::new(audio_format.sample_rate);
            channels.push(audio_channel);
        }
        let playlist = groove::Playlist::new();
        let sink = groove::Sink::new();
        sink.disable_resample(true);
        match sink.attach(&playlist) {
            Ok(_) => {},
            Err(_) => return Err(AudioFileError::AttachSinkFail),
        }
        playlist.append(&file, 1.0, 1.0);
        loop {
            match sink.buffer_get_blocking() {
                Some(decoded_buffer) => {
                    let sample_type = decoded_buffer.sample_format.sample_type;
                    let planar = decoded_buffer.sample_format.planar;
                    match (sample_type, planar) {
                        (SampleType::Dbl, false) => {
                            let samples = decoded_buffer.as_slice_f64();
                            for (index, sample) in samples.iter().enumerate() {
                                let channel_index = index % channel_count;
                                channels[channel_index].push(sample);
                            }
                        },
                        _ => return Err(AudioFileError::UnsupportedSampleFormat),
                    }
                },
                None => break,
            }
        }
        AudioFile {
            channels: chanels,
        }
    }
}

struct AudioFileWidget {
    audio_file_result: Result<AudioFile, AudioFileError>,
    label: Label,
}

impl AudioFileWidget {
    fn from_path(path: &Path) -> Self {
        AudioFileWidget {
            audio_file_result: AudioFile::from_path(path),
        }
    }
    fn draw(&self, frame: &mut glium::Frame, matrix: &Matrix4) {
        match self.audio_file_result {
            Ok(audio_file) => {},
            Err(err) => {},
        }
    }
}
*/

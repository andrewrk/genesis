extern crate sdl2;

use sdl2::video::{WindowPos, Window, OPENGL};
use sdl2::event::{Event, poll_event};

fn main() {
    sdl2::init(sdl2::INIT_VIDEO);

    let window = match Window::new("genesis",
                                   WindowPos::PosCentered, WindowPos::PosCentered,
                                   640, 480, OPENGL)
    {
        Ok(window) => window,
        Err(err) => panic!("failed to create window: {}", err),
    };

    loop {
        match poll_event() {
            Event::Quit(_) => break,
            Event::None => continue,
            _ => continue,
        }
    }

    sdl2::quit();
}

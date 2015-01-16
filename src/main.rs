extern crate sdl2;

use sdl2::video::{WindowPos, Window, OPENGL};
use sdl2::timer::delay;

fn main() {
    sdl2::init(sdl2::INIT_VIDEO);

    let window = match Window::new("genesis",
                                   WindowPos::PosCentered, WindowPos::PosCentered,
                                   640, 480, OPENGL)
    {
        Ok(window) => window,
        Err(err) => panic!("failed to create window: {}", err),
    };

    window.show();
    delay(3000);

    sdl2::quit();
}

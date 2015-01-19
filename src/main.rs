extern crate gfx;
extern crate glutin;

use gfx::{Device, DeviceHelper};

fn main() {
    let window = glutin::Window::new().unwrap();
    window.set_title("genesis");
    unsafe { window.make_current() }
    let (w, h) = window.get_inner_size().unwrap();

    let mut device = gfx::GlDevice::new(|s| window.get_proc_address(s));
    let mut renderer = device.create_renderer();

    renderer.clear(
        gfx::ClearData {
            color: [0.3, 0.3, 0.3, 1.0],
            depth: 1.0,
            stencil: 0,
        },
        gfx::COLOR,
        &gfx::Frame::new(w as u16, h as u16)
    );

    'main: loop {
        for event in window.poll_events() {
            match event {
                glutin::Event::KeyboardInput(_, _, Some(glutin::VirtualKeyCode::Escape))
                    => break 'main,
                glutin::Event::Closed => break 'main,
                _ => {},
            }
        }
        device.submit(renderer.as_buffer());
        window.swap_buffers();
    }
}

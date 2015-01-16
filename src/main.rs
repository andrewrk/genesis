extern crate gl;
extern crate glfw;

use glfw::{Context, OpenGlProfileHint, WindowHint, WindowMode};

fn main() {
    let glfw = glfw::init(glfw::FAIL_ON_ERRORS).unwrap();

    glfw.window_hint(WindowHint::ContextVersion(3, 2));
    glfw.window_hint(WindowHint::OpenglForwardCompat(true));
    glfw.window_hint(WindowHint::OpenglProfile(OpenGlProfileHint::Core));

    let (window, _) = glfw.create_window(800, 600, "genesis", WindowMode::Windowed)
        .expect("Failed to create GLFW window.");

    // must make context current before calling gl::load_with
    window.make_current();

    gl::load_with(|s| window.get_proc_address(s));

    while !window.should_close() {
        glfw.poll_events();

        clear_screen();

        window.swap_buffers();
    }
}

fn clear_screen() {
    unsafe {
        gl::ClearColor(0.3, 0.3, 0.3, 1.0);
        gl::Clear(gl::COLOR_BUFFER_BIT);
    }
}

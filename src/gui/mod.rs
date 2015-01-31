mod text;

pub use self::text::Label;

use glium::Display;

pub struct Gui<'a> {
    face: text::Face,
    text_renderer: text::TextRenderer<'a>,
}

impl<'a> Gui<'a> {
    pub fn new(display: &'a Display) -> Gui {
        let face;
        let text_renderer = text::TextRenderer::new(display);
        face = text_renderer.load_face(&Path::new("./assets/OpenSans-Regular.ttf"))
            .ok().expect("failed to load font");

        Gui {
            face: face,
            text_renderer: text_renderer,
        }
    }
    pub fn create_label(&'a mut self) -> Label {
        self.text_renderer.create_label(&self.face)
    }
}

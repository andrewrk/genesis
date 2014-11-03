extern crate rgtk;

use rgtk::gtk;
use rgtk::Connect;
use rgtk::GtkWindowTrait;
use rgtk::GtkContainerTrait;
use rgtk::GtkWidgetTrait;
use rgtk::gtk::signals::{DeleteEvent};

fn main() {
    gtk::init();

    let mut window = gtk::Window::new(gtk::window_type::TopLevel).unwrap();
    window.set_title("Genesis Sound Editor");
    window.set_border_width(10);
    window.set_window_position(gtk::window_position::Center);
    window.set_default_size(350, 70);

    Connect::connect(&window, DeleteEvent::new(|_| {
        gtk::main_quit();
        true
    }));

    let button = gtk::Button::new_with_label("Click me!").unwrap();
    window.add(&button);
    window.show_all();

    gtk::main();
}

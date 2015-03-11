#include "vertex_array.hpp"
#include "gui.hpp"
#include "gui_window.hpp"
#include "debug.hpp"

VertexArray::VertexArray(Gui *gui, void (*init_cb)(void *), void *userdata) :
    _gui(gui),
    _init_cb(init_cb),
    _userdata(userdata)
{
    GuiWindow *prev_window = _gui->get_bound_window();

    _vertex_array_objects.resize(_gui->_window_list.length());
    for (int i = 0; i < _vertex_array_objects.length(); i += 1) {
        GuiWindow *gui_window = _gui->_window_list.at(i);
        gui_window->bind();
        GLuint *id = &_vertex_array_objects.at(i);
        glGenVertexArrays(1, id);
        glBindVertexArray(*id);
        _init_cb(_userdata);
        assert_no_gl_error();
    }

    _gui_index = _gui->_vertex_array_list.length();
    _gui->_vertex_array_list.append(this);

    prev_window->bind();
}

VertexArray::~VertexArray() {
    GuiWindow *prev_window = _gui->get_bound_window();

    for (int i = 0; i < _gui->_window_list.length(); i += 1) {
        _gui->_window_list.at(i)->bind();
        glDeleteVertexArrays(1, &_vertex_array_objects.at(i));
    }

    _gui->_vertex_array_list.swap_remove(_gui_index);
    if (_gui_index < _gui->_vertex_array_list.length())
        _gui->_vertex_array_list.at(_gui_index)->_gui_index = _gui_index;

    prev_window->bind();
}

void VertexArray::bind(const GuiWindow *gui_window) {
    glBindVertexArray(_vertex_array_objects.at(gui_window->_gui_index));
}

void VertexArray::remove_index(int index) {
    _gui->_window_list.at(index)->bind();
    GLuint vertex_array_id = _vertex_array_objects.swap_remove(index);
    glDeleteVertexArrays(1, &vertex_array_id);
}

void VertexArray::append_context() {
    GLuint id;
    glGenVertexArrays(1, &id);
    _vertex_array_objects.append(id);
    glBindVertexArray(id);
    _init_cb(_userdata);
    assert_no_gl_error();
}

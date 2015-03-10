#include "vertex_array.hpp"
#include "gui.hpp"
#include "gui_window.hpp"
#include "debug.hpp"

VertexArray::VertexArray(Gui *gui, void (*init_cb)(void *), void *userdata) :
    _gui(gui),
    _init_cb(init_cb),
    _userdata(userdata)
{
    _vertex_array_objects.resize(_gui->_window_list.length());
    glGenVertexArrays(_vertex_array_objects.length(), _vertex_array_objects.raw());
    for (int i = 0; i < _vertex_array_objects.length(); i += 1) {
        glBindVertexArray(_vertex_array_objects.at(i));
        _init_cb(_userdata);
        assert_no_gl_error();
    }

    _gui_index = _gui->_vertex_array_list.length();
    _gui->_vertex_array_list.append(this);
}

VertexArray::~VertexArray() {
    glDeleteVertexArrays(_vertex_array_objects.length(), _vertex_array_objects.raw());

    _gui->_vertex_array_list.swap_remove(_gui_index);
    if (_gui_index < _gui->_vertex_array_list.length())
        _gui->_vertex_array_list.at(_gui_index)->_gui_index = _gui_index;
}

void VertexArray::bind(const GuiWindow *gui_window) {
    glBindVertexArray(_vertex_array_objects.at(gui_window->_gui_index));
}

void VertexArray::remove_index(int index) {
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

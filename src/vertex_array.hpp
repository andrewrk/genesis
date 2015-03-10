#ifndef VERTEX_ARRAY_HPP
#define VERTEX_ARRAY_HPP

#include "list.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class Gui;
class GuiWindow;
class VertexArray {
public:
    VertexArray(Gui *gui, void (*init_cb)(void *), void *userdata);
    ~VertexArray();

    void bind(const GuiWindow *window);

    void remove_index(int index);
    void append_context();
private:
    Gui *_gui;

    // one vertex array object per gl context
    // when a context is created this list must be updated
    List<GLuint> _vertex_array_objects;

    void (*_init_cb)(void *);
    void *_userdata;

    // index into Gui's list of vertex arrays
    int _gui_index;

    VertexArray(const VertexArray &copy) = delete;
    VertexArray &operator=(const VertexArray &copy) = delete;
};

#endif

#include "gui.hpp"
#include "debug.hpp"
#include "vertex_array.hpp"

uint32_t hash_int(const int &x) {
    return (uint32_t) x;
}

static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

static void panic_on_glfw_error(int error, const char* description) {
    panic("GLFW error: %s", description);
}

GlobalGlfwContext::GlobalGlfwContext() {
    glfwSetErrorCallback(panic_on_glfw_error);

    if (!glfwInit())
        panic("GLFW initialize");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 1);

}

GlobalGlfwContext::~GlobalGlfwContext() {
    glfwTerminate();
}

void Gui::init_primitive_vertex_array() {
    glBindBuffer(GL_ARRAY_BUFFER, _static_geometry._rect_2d_vertex_buffer);
    glEnableVertexAttribArray(_shader_program_manager._primitive_attrib_position);
    glVertexAttribPointer(_shader_program_manager._primitive_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}

Gui::Gui(GenesisContext *context, ResourceBundle *resource_bundle) :
    _running(true),
    _focus_window(nullptr),
    _utility_window(create_window(false)),
    _resource_bundle(resource_bundle),
    _spritesheet(this, "spritesheet"),
    _img_entry_dir(_spritesheet.get_image_info("img/entry-dir.png")),
    _img_entry_file(_spritesheet.get_image_info("img/entry-file.png")),
    _img_null(_spritesheet.get_image_info("img/null.png")),
    _primitive_vertex_array(this, init_primitive_vertex_array, this),
    _genesis_context(context)
{

    ft_ok(FT_Init_FreeType(&_ft_library));
    _resource_bundle->get_file_buffer("font.ttf", _default_font_buffer);
    ft_ok(FT_New_Memory_Face(_ft_library, (FT_Byte*)_default_font_buffer.raw(),
                _default_font_buffer.length(), 0, &_default_font_face));

    _cursor_default = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    _cursor_ibeam = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
}

Gui::~Gui() {
    glfwDestroyCursor(_cursor_default);
    glfwDestroyCursor(_cursor_ibeam);

    auto it = _font_size_cache.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;
        FontSize *font_size_object = entry->value;
        destroy(font_size_object, 1);
    }

    FT_Done_Face(_default_font_face);
    FT_Done_FreeType(_ft_library);
}

void Gui::exec() {
    while (_running) {
        glfwPollEvents();
        genesis_flush_events(_genesis_context);
        for (int i = 0; i < _window_list.length(); i += 1) {
            _window_list.at(i)->flush_events();
        }

        // draw the utility window first because it's the one with vsync on
        _utility_window->bind();
        _utility_window->draw();
        for (int i = 1; i < _window_list.length(); i += 1) {
            GuiWindow *_gui_window = _window_list.at(i);
            _gui_window->bind();
            _gui_window->draw();
        }
    }
}

FontSize *Gui::get_font_size(int font_size) {
    auto *entry = _font_size_cache.maybe_get(font_size);
    if (entry)
        return entry->value;
    FontSize *font_size_object = create<FontSize>(_default_font_face, font_size);
    _font_size_cache.put(font_size, font_size_object);
    return font_size_object;
}

void Gui::fill_rect(GuiWindow *window, const glm::vec4 &color, const glm::mat4 &mvp) {
    _shader_program_manager._primitive_shader_program.bind();

    _shader_program_manager._primitive_shader_program.set_uniform(
            _shader_program_manager._primitive_uniform_color, color);

    _shader_program_manager._primitive_shader_program.set_uniform(
            _shader_program_manager._primitive_uniform_mvp, mvp);

    _primitive_vertex_array.bind(window);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Gui::draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp) {
    _spritesheet.draw(window, img, mvp);
}

GuiWindow *Gui::create_window(bool with_borders) {
    GuiWindow *window = create<GuiWindow>(this, with_borders);
    window->_gui_index = _window_list.length();
    if (_window_list.append(window))
        panic("out of memory");
    for (int i = 0; i < _vertex_array_list.length(); i += 1) {
        VertexArray *vertex_array = _vertex_array_list.at(i);
        // the context is bound because it happens in create<GuiWindow>()
        vertex_array->append_context();
    }
    return window;
}

void Gui::destroy_window(GuiWindow *window) {
    if (window == _focus_window)
        _focus_window = nullptr;

    if (window->_gui_index < 0)
        panic("window did not have its gui index set");

    int index = window->_gui_index;
    for (int i = 0; i < _vertex_array_list.length(); i += 1) {
        VertexArray *vertex_array = _vertex_array_list.at(i);
        vertex_array->remove_index(index);
    }
    _window_list.swap_remove(index);
    if (index < _window_list.length())
        _window_list.at(index)->_gui_index = index;
    destroy(window, 1);

    if (_window_list.length() == 1)
        _running = false;
}

GuiWindow *Gui::get_bound_window() {
    GLFWwindow *window = glfwGetCurrentContext();
    return static_cast<GuiWindow*>(glfwGetWindowUserPointer(window));
}

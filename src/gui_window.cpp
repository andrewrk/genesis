#include "gui_window.hpp"
#include "gui.hpp"
#include "util.hpp"
#include "widget.hpp"
#include "debug_gl.hpp"
#include "menu_widget.hpp"
#include "os.hpp"

static void run(void *arg) {
    GuiWindow *gui_window = (GuiWindow *)arg;
    gui_window->setup_context();

    while (gui_window->running) {
        gui_window->draw();
    }

    gui_window->teardown_context();
}

static void handle_new_size(GuiWindow *gui_window, int width, int height) {
    gui_window->_width = width;
    gui_window->_height = height;
    gui_window->_projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
}

static void static_window_pos_callback(GLFWwindow* window, int left, int top) {
    return static_cast<GuiWindow*>(glfwGetWindowUserPointer(window))->window_pos_callback(left, top);
}

GuiWindow::GuiWindow(Gui *gui, bool is_normal_window, int left, int top, int width, int height) :
    _userdata(nullptr),
    _gui(gui),
    _mouse_over_widget(nullptr),
    _focus_widget(nullptr),
    menu_widget(nullptr),
    _is_iconified(false),
    is_visible(true),
    _last_click_time(os_get_time()),
    _last_click_button(MouseButtonLeft),
    _double_click_timeout(0.3),
    running(true),
    main_widget(nullptr),
    context_menu(nullptr),
    is_maximized(false)
{
    if (is_normal_window) {
        glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
        glfwWindowHint(GLFW_DECORATED, GL_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
        window = glfwCreateWindow(width, height, "genesis", NULL, _gui->_utility_window->window);
    } else {
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
        window = glfwCreateWindow(width, height, "genesis", NULL, NULL);
        is_visible = false;
    }
    if (!window)
        panic("unable to create window");
    glfwSetWindowUserPointer(window, this);

    glfwSetWindowPos(window, left, top);

    int window_size_width, window_size_height;
    glfwGetWindowSize(window, &window_size_width, &window_size_height);
    got_window_size(window_size_width, window_size_height);

    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    handle_new_size(this, framebuffer_width, framebuffer_height);

    int new_left, new_top;
    glfwGetWindowPos(window, &new_left, &new_top);
    got_window_pos(new_left, new_top);


    glfwSetWindowPosCallback(window, static_window_pos_callback);
    glfwSetWindowIconifyCallback(window, static_window_iconify_callback);
    glfwSetFramebufferSizeCallback(window, static_framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, static_window_size_callback);
    glfwSetKeyCallback(window, static_key_callback);
    glfwSetCharModsCallback(window, static_charmods_callback);
    glfwSetWindowCloseCallback(window, static_window_close_callback);
    glfwSetCursorPosCallback(window, static_cursor_pos_callback);
    glfwSetMouseButtonCallback(window, static_mouse_button_callback);
    glfwSetScrollCallback(window, static_scroll_callback);


    if (is_normal_window) {
        if (thread.start(run, this))
            panic("unable to start thread");
    } else {
        setup_context();
    }
}

GuiWindow::~GuiWindow() {
    destroy_context_menu();

    if (_gui->_utility_window == this) {
        teardown_context();
    } else {
        running = false;

        _gui->gui_mutex.unlock();
        thread.join();
        _gui->gui_mutex.lock();
    }

    if (main_widget)
        destroy(main_widget, 1);

    glfwDestroyWindow(window);
}

void GuiWindow::setup_context() {
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Vertex Array Objects can't be shared between contexts. This makes managing
    // these VAOs very difficult. So we simply create and bind exactly one vertex
    // array per context and use glVertexAttribPointer etc every frame.
    glGenVertexArrays(1, &vertex_array_object);
    glBindVertexArray(vertex_array_object);

    glClearColor(0.3, 0.3, 0.3, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    assert_no_gl_error();

    viewport_update_queued = true;
}

void GuiWindow::teardown_context() {
    glDeleteVertexArrays(1, &vertex_array_object);
    assert_no_gl_error();
}

void GuiWindow::window_iconify_callback(int iconified) {
    MutexLocker locker(&_gui->gui_mutex);
    _is_iconified = iconified;
}

void GuiWindow::draw() {
    if (viewport_update_queued.exchange(false))
        glViewport(0, 0, _width, _height);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    {
        MutexLocker locker(&_gui->gui_mutex);

        if (main_widget && main_widget->is_visible)
            main_widget->draw(_projection);
        if (context_menu && context_menu->is_visible)
            context_menu->draw(_projection);

    }
    glfwSwapBuffers(window);
}

void GuiWindow::layout_main_widget() {
    if (main_widget) {
        main_widget->left = 0;
        main_widget->top = 0;
        main_widget->width = _width;
        main_widget->height = _height;
        main_widget->on_resize();
    }
}

void GuiWindow::framebuffer_size_callback(int width, int height) {
    MutexLocker locker(&_gui->gui_mutex);

    handle_new_size(this, width, height);

    layout_main_widget();

    viewport_update_queued = true;
}

void GuiWindow::set_main_widget(Widget *widget) {
    main_widget = widget;
    widget->parent_widget = nullptr;
    layout_main_widget();
    set_focus_widget(main_widget);
}

void GuiWindow::window_size_callback(int width, int height) {
    MutexLocker locker(&_gui->gui_mutex);
    got_window_size(width, height);
    events.trigger(EventWindowSizeChange);
}

void GuiWindow::window_pos_callback(int left, int top) {
    MutexLocker locker(&_gui->gui_mutex);
    got_window_pos(left, top);
    events.trigger(EventWindowPosChange);
}

void GuiWindow::got_window_pos(int left, int top) {
    client_left = left;
    client_top = top;
}

void GuiWindow::got_window_size(int width, int height) {
    _client_width = width;
    _client_height = height;
}

void GuiWindow::window_close_callback() {
    MutexLocker locker(&_gui->gui_mutex);
    events.trigger(EventWindowClose);
}

void GuiWindow::key_callback(int key, int scancode, int action, int mods) {
    MutexLocker locker(&_gui->gui_mutex);

    KeyEvent key_event = {
        (action == GLFW_PRESS || action == GLFW_REPEAT) ? KeyActionDown : KeyActionUp,
        (VirtKey)key,
        mods,
    };

    Widget *event_target = _focus_widget;
    while (event_target) {
        if (event_target->on_key_event(&key_event))
            return;
        event_target = event_target->parent_widget;
    }

    if (menu_widget) {
        if (menu_widget->on_key_event(&key_event))
            return;
    }
}

void GuiWindow::charmods_callback(unsigned int codepoint, int mods) {
    MutexLocker locker(&_gui->gui_mutex);


    TextInputEvent text_event = {
        codepoint,
        mods,
    };

    if (_focus_widget)
        _focus_widget->on_text_input(&text_event);
}

int GuiWindow::get_modifiers() {
    int mod_shift  = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) ? GLFW_MOD_SHIFT : 0;
    int mod_alt    = (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) ? GLFW_MOD_ALT : 0;
    int mod_control= (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) ? GLFW_MOD_CONTROL : 0;
    int mod_super  = (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS) ? GLFW_MOD_SUPER : 0;
    return mod_shift|mod_control|mod_alt|mod_super;
}

void GuiWindow::cursor_pos_callback(double xpos, double ypos) {
    MutexLocker locker(&_gui->gui_mutex);

    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;

    bool left = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    bool middle = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    bool right = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    MouseEvent mouse_event = {
        x,
        y,
        MouseButtonNone,
        MouseActionMove,
        MouseButtons {left, middle, right},
        get_modifiers(),
    };

    on_mouse_move(&mouse_event);
}

void GuiWindow::mouse_button_callback(int button, int action, int mods) {
    MutexLocker locker(&_gui->gui_mutex);

    MouseButton btn;
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            btn = MouseButtonLeft;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            btn = MouseButtonMiddle;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            btn = MouseButtonRight;
            break;
        default:
            return;
    }
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;
    bool left = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    bool middle = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    bool right = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    MouseAction mouse_action = (action == GLFW_PRESS) ? MouseActionDown : MouseActionUp;
    bool is_double_click = false;
    if (mouse_action == MouseActionDown) {
        double this_click_time = os_get_time();
        if (_last_click_button == btn) {
            if (this_click_time - _last_click_time < _double_click_timeout)
                is_double_click = true;
        }
        _last_click_time = this_click_time;
    }
    MouseEvent mouse_event = {
        x,
        y,
        btn,
        mouse_action,
        MouseButtons {left, middle, right},
        mods,
        is_double_click,
    };
    on_mouse_move(&mouse_event);
}

void GuiWindow::scroll_callback(double xoffset, double yoffset) {
    MutexLocker locker(&_gui->gui_mutex);

    if (!_focus_widget)
        return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;
    MouseWheelEvent wheel_event = {
        x - _focus_widget->left,
        y - _focus_widget->top,
        (int)sign(xoffset),
        (int)sign(yoffset),
        get_modifiers(),
    };

    _focus_widget->on_mouse_wheel(&wheel_event);
}

void GuiWindow::on_mouse_move(const MouseEvent *event) {
    if (context_menu && _mouse_over_widget != context_menu) {
        ContextMenuWidget *target = context_menu;
        while (target) {
            if (try_mouse_move_event_on_widget(target, event))
                return;
            target = target->sub_menu;
        }
        if (event->action == MouseActionDown) {
            destroy_context_menu();
            return;
        }
    }

    // if we're pressing a mouse button, the mouse over widget gets the event
    bool pressing_any_btn = (event->buttons.left || event->buttons.middle || event->buttons.right);

    if (_mouse_over_widget) {
        int right = _mouse_over_widget->left + _mouse_over_widget->width;
        int bottom = _mouse_over_widget->top + _mouse_over_widget->height;
        bool in_bounds = (event->x >= _mouse_over_widget->left &&
                event->y >= _mouse_over_widget->top &&
                event->x < right &&
                event->y < bottom);

        MouseEvent mouse_event = *event;
        mouse_event.x -= _mouse_over_widget->left;
        mouse_event.y -= _mouse_over_widget->top;

        if (in_bounds || pressing_any_btn) {
            if (event->action == MouseActionDown)
                set_focus_widget(_mouse_over_widget);
            _mouse_over_widget->on_mouse_move(&mouse_event);
            return;
        } else {
            // not in bounds, not pressing any button
            if (event->action == MouseActionUp) {
                // give them the mouse up event
                _mouse_over_widget->on_mouse_move(&mouse_event);
            }
            Widget *old_mouse_over_widget = _mouse_over_widget;
            _mouse_over_widget = NULL;
            set_cursor_default();
            if (old_mouse_over_widget)
                old_mouse_over_widget->on_mouse_out(&mouse_event);
        }
    }

    assert(_mouse_over_widget == nullptr);

    if (main_widget)
        try_mouse_move_event_on_widget(main_widget, event);
}

void GuiWindow::remove_widget(Widget *widget) {
    if (widget == _mouse_over_widget)
        _mouse_over_widget = nullptr;
    if (widget == _focus_widget) {
        _focus_widget = nullptr;
        set_focus_widget(main_widget);
    }
}

bool GuiWindow::try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event) {
    bool pressing_any_btn = (event->buttons.left || event->buttons.middle || event->buttons.right);
    int right = widget->left + widget->width;
    int bottom = widget->top + widget->height;
    if (event->x >= widget->left && event->y >= widget->top &&
        event->x < right && event->y < bottom)
    {
        MouseEvent mouse_event = *event;
        mouse_event.x -= widget->left;
        mouse_event.y -= widget->top;

        bool first_over = _mouse_over_widget != widget;
        _mouse_over_widget = widget;

        if (pressing_any_btn && _mouse_over_widget != _focus_widget)
            set_focus_widget(_mouse_over_widget);

        if (first_over)
            widget->on_mouse_over(&mouse_event);
        widget->on_mouse_move(&mouse_event);
        return true;
    }
    return false;
}

bool GuiWindow::widget_is_menu(Widget *widget) {
    if (widget == menu_widget)
        return true;

    ContextMenuWidget *target = context_menu;
    while (target) {
        if (target == widget)
            return true;

        target = target->sub_menu;
    }
    return false;
}

void GuiWindow::set_focus_widget(Widget *widget) {
    if (_focus_widget == widget)
        return;
    if (_focus_widget) {
        Widget *old_focus_widget = _focus_widget;
        _focus_widget = NULL;
        old_focus_widget->on_lose_focus();
        if (widget_is_menu(old_focus_widget) && !widget_is_menu(widget))
            destroy_context_menu();
    }
    if (!widget)
        return;
    _focus_widget = widget;
    _focus_widget->on_gain_focus();
}

void GuiWindow::fill_rect(const glm::vec4 &color, const glm::mat4 &mvp) {
    _gui->_shader_program_manager._primitive_shader_program.bind();

    _gui->_shader_program_manager._primitive_shader_program.set_uniform(
            _gui->_shader_program_manager._primitive_uniform_color, color);

    _gui->_shader_program_manager._primitive_shader_program.set_uniform(
            _gui->_shader_program_manager._primitive_uniform_mvp, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, _gui->_static_geometry._rect_2d_vertex_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager._primitive_attrib_position);
    glVertexAttribPointer(_gui->_shader_program_manager._primitive_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void GuiWindow::fill_rect(const glm::vec4 &color, int x, int y, int w, int h) {
    glm::mat4 model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(x, y, 0.0f)),
                        glm::vec3(w, h, 0.0f));
    glm::mat4 mvp = _projection * model;
    fill_rect(color, mvp);
}

void GuiWindow::draw_image(const SpritesheetImage *img, int x, int y, int w, int h) {
    float scale_x = ((float)w) / ((float)img->width);
    float scale_y = ((float)h) / ((float)img->height);
    glm::mat4 model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(x, y, 0.0f)),
                        glm::vec3(scale_x, scale_y, 0.0f));
    glm::mat4 mvp = _projection * model;
    _gui->draw_image(this, img, mvp);
}

void GuiWindow::set_cursor_beam() {
    glfwSetCursor(window, _gui->cursor_ibeam);
}

void GuiWindow::set_cursor_default() {
    glfwSetCursor(window, _gui->cursor_default);
}

void GuiWindow::set_cursor_hresize() {
    glfwSetCursor(window, _gui->cursor_hresize);
}

void GuiWindow::set_cursor_vresize() {
    glfwSetCursor(window, _gui->cursor_vresize);
}

void GuiWindow::set_clipboard_string(const String &str) {
    glfwSetClipboardString(window, str.encode().raw());
}

String GuiWindow::get_clipboard_string() const {
    const char* clip_text = glfwGetClipboardString(window);
    if (!clip_text)
        return String();

    bool ok;
    String str = String::decode(clip_text, &ok);
    if (!ok)
        fprintf(stderr, "Reading invalid UTF-8 from the clipboard\n");
    return str;
}

bool GuiWindow::clipboard_has_string() const {
    const char* clip_text = glfwGetClipboardString(window);
    return (clip_text != nullptr);
}

ContextMenuWidget * GuiWindow::pop_context_menu(MenuWidgetItem *menu_widget_item,
        int left, int top, int width, int height)
{
    destroy_context_menu();
    context_menu = create<ContextMenuWidget>(menu_widget_item);
    context_menu->left = left;
    context_menu->top = top + height;

    if (context_menu->left + context_menu->width > _width)
        context_menu->left = left - context_menu->width;
    if (context_menu->top + context_menu->height > _height)
        context_menu->top = top - context_menu->height;

    context_menu->on_resize();

    context_menu->width = context_menu->min_width();
    context_menu->height = context_menu->min_height();

    context_menu->on_resize();

    set_focus_widget(context_menu);

    return context_menu;
}

void GuiWindow::refresh_context_menu() {
    if (context_menu)
        context_menu->on_resize();
}

void GuiWindow::destroy_context_menu() {
    if (context_menu) {
        destroy(context_menu, 1);
        context_menu = nullptr;
    }
}

void GuiWindow::maximize() {
    // waiting for GLFW to support this feature
    // https://github.com/andrewrk/genesis/issues/18
}

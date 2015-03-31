#include "gui_window.hpp"
#include "gui.hpp"
#include "util.hpp"
#include "widget.hpp"
#include "text_widget.hpp"
#include "find_file_widget.hpp"
#include "audio_edit_widget.hpp"
#include "select_widget.hpp"

static bool default_on_key_event(GuiWindow *, const KeyEvent *event) {
    return false;
}

static bool default_on_text_event(GuiWindow *, const TextInputEvent *event) {
    return false;
}

static void default_on_close_event(GuiWindow *) {
    fprintf(stderr, "no window close handler attached\n");
}

GuiWindow::GuiWindow(Gui *gui, bool is_normal_window) :
    _userdata(nullptr),
    _gui(gui),
    _mouse_over_widget(nullptr),
    _focus_widget(nullptr),
    _on_key_event(default_on_key_event),
    _on_text_event(default_on_text_event),
    _on_close_event(default_on_close_event),
    _is_iconified(false),
    _is_visible(true),
    _last_click_time(glfwGetTime()),
    _last_click_button(MouseButtonLeft),
    _double_click_timeout(0.3)
{
    if (is_normal_window) {
        glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
        glfwWindowHint(GLFW_DECORATED, GL_TRUE);
        glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
        _window = glfwCreateWindow(1366, 768, "genesis", NULL, _gui->_utility_window->_window);
    } else {
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
        _window = glfwCreateWindow(100, 100, "genesis", NULL, NULL);
        _is_visible = false;
    }
    if (!_window)
        panic("unable to create window");
    glfwSetWindowUserPointer(_window, this);

    bind();
    // utility window is the only one that has vsync on, and we draw it last
    glfwSwapInterval(is_normal_window ? 0 : 1);

    glClearColor(0.3, 0.3, 0.3, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glfwSetWindowIconifyCallback(_window, static_window_iconify_callback);
    glfwSetFramebufferSizeCallback(_window, static_framebuffer_size_callback);
    glfwSetWindowSizeCallback(_window, static_window_size_callback);
    glfwSetKeyCallback(_window, static_key_callback);
    glfwSetCharModsCallback(_window, static_charmods_callback);
    glfwSetWindowCloseCallback(_window, static_window_close_callback);
    glfwSetCursorPosCallback(_window, static_cursor_pos_callback);
    glfwSetMouseButtonCallback(_window, static_mouse_button_callback);
    glfwSetScrollCallback(_window, static_scroll_callback);

    // we want to know the framebuffer size right now
    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(_window, &framebuffer_width, &framebuffer_height);
    framebuffer_size_callback(framebuffer_width, framebuffer_height);

    // we want to know the window size right now
    int window_size_width, window_size_height;
    glfwGetWindowSize(_window, &window_size_width, &window_size_height);
    window_size_callback(window_size_width, window_size_height);

}

GuiWindow::~GuiWindow() {
    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        destroy_widget(widget);
    }

    glfwDestroyWindow(_window);
}

void GuiWindow::window_iconify_callback(int iconified) {
    _is_iconified = iconified;
}

void GuiWindow::bind() {
    glfwMakeContextCurrent(_window);
}

void GuiWindow::flush_events() {
    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        widget->flush_events();
    }
}

void GuiWindow::draw() {
    if (_gui->_utility_window != this) {
        if (_is_iconified)
            return;
        if (!_is_visible)
            return;
    }

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        if (widget->_is_visible)
            widget->draw(this, _projection);
    }
    glfwSwapBuffers(_window);
}

void GuiWindow::framebuffer_size_callback(int width, int height) {
    _width = width;
    _height = height;
    glViewport(0, 0, _width, _height);
    _projection = glm::ortho(0.0f, (float)_width, (float)_height, 0.0f);
}

void GuiWindow::window_size_callback(int width, int height) {
    _client_width = width;
    _client_height = height;
}

void GuiWindow::window_close_callback() {
    _on_close_event(this);
}

void GuiWindow::key_callback(int key, int scancode, int action, int mods) {
    KeyEvent key_event = {
        (action == GLFW_PRESS || action == GLFW_REPEAT) ? KeyActionDown : KeyActionUp,
        (VirtKey)key,
        mods,
    };

    if (_on_key_event(this, &key_event))
        return;

    if (!_focus_widget)
        return;

    _focus_widget->on_key_event(&key_event);
}

void GuiWindow::charmods_callback(unsigned int codepoint, int mods) {
    TextInputEvent text_event = {
        codepoint,
        mods,
    };

    if (_on_text_event(this, &text_event))
        return;

    if (!_focus_widget)
        return;

    _focus_widget->on_text_input(&text_event);
}

int GuiWindow::get_modifiers() {
    int mod_shift  = (glfwGetKey(_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) ? GLFW_MOD_SHIFT : 0;
    int mod_alt    = (glfwGetKey(_window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                      glfwGetKey(_window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) ? GLFW_MOD_ALT : 0;
    int mod_control= (glfwGetKey(_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                      glfwGetKey(_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) ? GLFW_MOD_CONTROL : 0;
    int mod_super  = (glfwGetKey(_window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
                      glfwGetKey(_window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS) ? GLFW_MOD_SUPER : 0;
    return mod_shift|mod_control|mod_alt|mod_super;
}

void GuiWindow::cursor_pos_callback(double xpos, double ypos) {
    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;

    bool left = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    bool middle = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    bool right = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
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
    glfwGetCursorPos(_window, &xpos, &ypos);
    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;
    bool left = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    bool middle = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    bool right = (glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    MouseAction mouse_action = (action == GLFW_PRESS) ? MouseActionDown : MouseActionUp;
    bool is_double_click = false;
    if (mouse_action == MouseActionDown) {
        double this_click_time = glfwGetTime();
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
    if (!_focus_widget)
        return;

    double xpos, ypos;
    glfwGetCursorPos(_window, &xpos, &ypos);
    int x = (xpos / (double)_client_width) * _width;
    int y = (ypos / (double)_client_height) * _height;
    MouseWheelEvent wheel_event = {
        x - _focus_widget->left(),
        y - _focus_widget->top(),
        (int)sign(xoffset),
        (int)sign(yoffset),
        get_modifiers(),
    };

    _focus_widget->on_mouse_wheel(&wheel_event);
}

void GuiWindow::on_mouse_move(const MouseEvent *event) {
    // if we're pressing a mouse button, the mouse over widget gets the event
    bool pressing_any_btn = (event->buttons.left || event->buttons.middle || event->buttons.right);
    if (_mouse_over_widget) {
        int right = _mouse_over_widget->left() + _mouse_over_widget->width();
        int bottom = _mouse_over_widget->top() + _mouse_over_widget->height();
        bool in_bounds = (event->x >= _mouse_over_widget->left() &&
                event->y >= _mouse_over_widget->top() &&
                event->x < right &&
                event->y < bottom);

        MouseEvent mouse_event = *event;
        mouse_event.x -= _mouse_over_widget->left();
        mouse_event.y -= _mouse_over_widget->top();

        if (in_bounds || pressing_any_btn) {
            if (pressing_any_btn && _mouse_over_widget != _focus_widget)
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
            old_mouse_over_widget->on_mouse_out(&mouse_event);
        }
    }

    if (_mouse_over_widget != NULL)
        panic("expected _mouse_over_widget NULL");

    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        if (try_mouse_move_event_on_widget(widget, event))
            return;
    }
}

void GuiWindow::init_widget(Widget *widget) {
    widget->_is_visible = true;
    widget->_gui_index = _widget_list.length();
    if (_widget_list.append(widget))
        panic("out of memory");
}

SelectWidget * GuiWindow::create_select_widget() {
    SelectWidget *select_widget = create<SelectWidget>(this, _gui);
    init_widget(select_widget);
    return select_widget;
}

TextWidget * GuiWindow::create_text_widget() {
    TextWidget *text_widget = create<TextWidget>(this, _gui);
    init_widget(text_widget);
    return text_widget;
}

FindFileWidget * GuiWindow::create_find_file_widget() {
    FindFileWidget *find_file_widget = create<FindFileWidget>(this, _gui);
    init_widget(find_file_widget);
    return find_file_widget;
}

AudioEditWidget * GuiWindow::create_audio_edit_widget() {
    AudioEditWidget *audio_edit_widget = create<AudioEditWidget>(this, _gui, _gui->_genesis_context);
    init_widget(audio_edit_widget);
    return audio_edit_widget;
}

void GuiWindow::destroy_widget(Widget *widget) {
    if (widget == _mouse_over_widget)
        _mouse_over_widget = NULL;
    if (widget == _focus_widget)
        _focus_widget = NULL;

    if (widget->_gui_index >= 0) {
        int index = widget->_gui_index;
        _widget_list.swap_remove(index);
        if (index < _widget_list.length())
            _widget_list.at(index)->_gui_index = index;
    }
    destroy(widget, 1);
}

bool GuiWindow::try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event) {
    bool pressing_any_btn = (event->buttons.left || event->buttons.middle || event->buttons.right);
    int right = widget->left() + widget->width();
    int bottom = widget->top() + widget->height();
    if (event->x >= widget->left() && event->y >= widget->top() &&
        event->x < right && event->y < bottom)
    {
        MouseEvent mouse_event = *event;
        mouse_event.x -= widget->left();
        mouse_event.y -= widget->top();

        _mouse_over_widget = widget;

        if (pressing_any_btn && _mouse_over_widget != _focus_widget)
            set_focus_widget(_mouse_over_widget);

        widget->on_mouse_over(&mouse_event);
        widget->on_mouse_move(&mouse_event);
        return true;
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
    }
    if (!widget)
        return;
    _focus_widget = widget;
    _focus_widget->on_gain_focus();
}

void GuiWindow::fill_rect(const glm::vec4 &color, int x, int y, int w, int h) {
    glm::mat4 model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(x, y, 0.0f)),
                        glm::vec3(w, h, 0.0f));
    glm::mat4 mvp = _projection * model;
    _gui->fill_rect(this, color, mvp);
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
    glfwSetCursor(_window, _gui->_cursor_ibeam);
}

void GuiWindow::set_cursor_default() {
    glfwSetCursor(_window, _gui->_cursor_default);
}

void GuiWindow::set_clipboard_string(const String &str) {
    glfwSetClipboardString(_window, str.encode().raw());
}

String GuiWindow::get_clipboard_string() const {
    const char* clip_text = glfwGetClipboardString(_window);
    if (!clip_text)
        return String();

    bool ok;
    String str = String::decode(clip_text, &ok);
    if (!ok)
        fprintf(stderr, "Reading invalid UTF-8 from the clipboard\n");
    return str;
}

bool GuiWindow::clipboard_has_string() const {
    const char* clip_text = glfwGetClipboardString(_window);
    return (clip_text != nullptr);
}

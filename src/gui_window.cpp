#include "gui_window.hpp"
#include "gui.hpp"
#include "util.hpp"
#include "widget.hpp"
#include "text_widget.hpp"
#include "find_file_widget.hpp"
#include "audio_edit_widget.hpp"

static bool default_on_key_event(GuiWindow *, const KeyEvent *event) {
    return false;
}

static void default_on_close_event(GuiWindow *) {
    fprintf(stderr, "no window close handler attached\n");
}

GuiWindow::GuiWindow(Gui *gui, bool with_borders) :
    _userdata(nullptr),
    _gui(gui),
    _last_mouse_x(0),
    _last_mouse_y(0),
    _mouse_over_widget(nullptr),
    _focus_widget(nullptr),
    _on_key_event(default_on_key_event),
    _on_close_event(default_on_close_event)
{
    if (with_borders) {
        _window = SDL_CreateWindow("genesis",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1366, 768, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    } else {
        _window = SDL_CreateWindow("genesis",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            100, 100, SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|SDL_WINDOW_BORDERLESS);
    }
    if (!_window)
        panic("unable to create SDL window");

    _context = SDL_GL_CreateContext(_window);

    if (!_context)
        panic("unable to create gl context: %s", SDL_GetError());

    SDL_GL_MakeCurrent(_window, _context);

    glClearColor(0.3, 0.3, 0.3, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    resize();
}

GuiWindow::~GuiWindow() {
    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        destroy_widget(widget);
    }

    SDL_GL_DeleteContext(_context);
    SDL_DestroyWindow(_window);
}

void GuiWindow::draw() {
    SDL_GL_MakeCurrent(_window, _context);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        if (widget->_is_visible)
            widget->draw(this, _projection);
    }

    SDL_GL_SwapWindow(_window);
}

void GuiWindow::resize() {
    SDL_GL_GetDrawableSize(_window, &_width, &_height);
    glViewport(0, 0, _width, _height);
    _projection = glm::ortho(0.0f, (float)_width, (float)_height, 0.0f);
}

void GuiWindow::on_close() {
    _on_close_event(this);
}

void GuiWindow::on_mouse_move(const MouseEvent *event) {
    _last_mouse_x = event->x;
    _last_mouse_y = event->y;

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

void GuiWindow::on_text_input(const TextInputEvent *event) {
    if (!_focus_widget)
        return;

    _focus_widget->on_text_input(event);
}

void GuiWindow::on_key_event(const KeyEvent *event) {
    if (_on_key_event(this, event))
        return;

    if (!_focus_widget)
        return;

    _focus_widget->on_key_event(event);
}

void GuiWindow::on_mouse_wheel(const MouseWheelEvent *event) {
    if (!_focus_widget)
        return;

    MouseWheelEvent modified_event = *event;
    modified_event.x -= _focus_widget->left();
    modified_event.y -= _focus_widget->top();
    _focus_widget->on_mouse_wheel(&modified_event);
}

void GuiWindow::init_widget(Widget *widget) {
    widget->_is_visible = true;
    widget->_gui_index = _widget_list.length();
    _widget_list.append(widget);
}

TextWidget * GuiWindow::create_text_widget() {
    TextWidget *text_widget = create<TextWidget>(_gui);
    init_widget(text_widget);
    return text_widget;
}

FindFileWidget * GuiWindow::create_find_file_widget() {
    FindFileWidget *find_file_widget = create<FindFileWidget>(this, _gui);
    init_widget(find_file_widget);
    return find_file_widget;
}

AudioEditWidget * GuiWindow::create_audio_edit_widget() {
    AudioEditWidget *audio_edit_widget = create<AudioEditWidget>(this, _gui, &_gui->_audio_hardware);
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


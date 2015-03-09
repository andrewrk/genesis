#include "gui.hpp"
#include "debug.hpp"
#include "text_widget.hpp"
#include "find_file_widget.hpp"
#include "audio_edit_widget.hpp"
#include "audio_hardware.hpp"

uint32_t hash_int(const int &x) {
    return (uint32_t) x;
}

static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

static bool default_on_key_event(Gui *, const KeyEvent *event) {
    return false;
}

Gui::Gui(SDL_Window *window, ResourceBundle *resource_bundle,
        ShaderProgramManager *shader_program_manager, AudioHardware *audio_hardware) :
    _shader_program_manager(shader_program_manager),
    _window(window),
    _mouse_over_widget(NULL),
    _focus_widget(NULL),
    _resource_bundle(resource_bundle),
    _spritesheet(this, "spritesheet"),
    _img_entry_dir((Image*)_spritesheet.get_image_info("img/entry-dir.png")),
    _img_entry_file((Image*)_spritesheet.get_image_info("img/entry-file.png")),
    _img_null((Image*)_spritesheet.get_image_info("img/null.png")),
    _userdata(NULL),
    _on_key_event(default_on_key_event),
    _audio_hardware(audio_hardware)
{
    glGenVertexArrays(1, &_primitive_vertex_array);
    glBindVertexArray(_primitive_vertex_array);

    glGenBuffers(1, &_primitive_vertex_buffer);

    GLfloat vertexes[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
    };
    glBindBuffer(GL_ARRAY_BUFFER, _primitive_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), vertexes, GL_STATIC_DRAW);


    glEnableVertexAttribArray(_shader_program_manager->_primitive_attrib_position);
    glVertexAttribPointer(_shader_program_manager->_primitive_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    assert_no_gl_error();


    ft_ok(FT_Init_FreeType(&_ft_library));
    _resource_bundle->get_file_buffer("font.ttf", _default_font_buffer);
    ft_ok(FT_New_Memory_Face(_ft_library, (FT_Byte*)_default_font_buffer.raw(),
                _default_font_buffer.length(), 0, &_default_font_face));

    _cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    _cursor_ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    glClearColor(0.3, 0.3, 0.3, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    resize();
}

Gui::~Gui() {
    for (long i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        destroy_widget(widget);
    }

    SDL_FreeCursor(_cursor_default);
    SDL_FreeCursor(_cursor_ibeam);

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

    glDeleteBuffers(1, &_primitive_vertex_buffer);
    glDeleteVertexArrays(1, &_primitive_vertex_array);
}

KeyModifiers Gui::get_key_modifiers() {
    const uint8_t *key_state = SDL_GetKeyboardState(NULL);
    return {
        (bool)key_state[SDL_SCANCODE_LSHIFT],
        (bool)key_state[SDL_SCANCODE_RSHIFT],
        (bool)key_state[SDL_SCANCODE_LCTRL],
        (bool)key_state[SDL_SCANCODE_RCTRL],
        (bool)key_state[SDL_SCANCODE_LALT],
        (bool)key_state[SDL_SCANCODE_RALT],
        (bool)key_state[SDL_SCANCODE_LGUI],
        (bool)key_state[SDL_SCANCODE_RGUI],
        (bool)key_state[SDL_SCANCODE_NUMLOCKCLEAR],
        (bool)key_state[SDL_SCANCODE_CAPSLOCK],
        (bool)key_state[SDL_SCANCODE_MODE],
    };
}

void Gui::exec() {
    bool running = true;
    while (running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                {
                    KeyEvent key_event = {
                        (event.key.state == SDL_PRESSED) ? KeyActionDown : KeyActionUp,
                        (VirtKey)event.key.keysym.sym,
                        {
                            (bool)(event.key.keysym.mod & KMOD_LSHIFT),
                            (bool)(event.key.keysym.mod & KMOD_RSHIFT),
                            (bool)(event.key.keysym.mod & KMOD_LCTRL),
                            (bool)(event.key.keysym.mod & KMOD_RCTRL),
                            (bool)(event.key.keysym.mod & KMOD_LALT),
                            (bool)(event.key.keysym.mod & KMOD_RALT),
                            (bool)(event.key.keysym.mod & KMOD_LGUI),
                            (bool)(event.key.keysym.mod & KMOD_RGUI),
                            (bool)(event.key.keysym.mod & KMOD_NUM),
                            (bool)(event.key.keysym.mod & KMOD_CAPS),
                            (bool)(event.key.keysym.mod & KMOD_MODE),
                        },
                    };
                    on_key_event(&key_event);
                }
                break;
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        resize();
                        break;
                }
                break;
            case SDL_MOUSEMOTION:
                {
                    bool left = event.motion.state & SDL_BUTTON_LMASK;
                    bool middle = event.motion.state & SDL_BUTTON_MMASK;
                    bool right = event.motion.state & SDL_BUTTON_RMASK;
                    MouseEvent mouse_event = {
                        event.motion.x,
                        event.motion.y,
                        MouseButtonNone,
                        MouseActionMove,
                        MouseButtons {left, middle, right},
                        get_key_modifiers(),
                    };
                    on_mouse_move(&mouse_event);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                {
                    MouseButton btn;
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        btn = MouseButtonLeft;
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                        btn = MouseButtonMiddle;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        btn = MouseButtonRight;
                    } else {
                        break;
                    }
                    bool left = event.button.state & SDL_BUTTON_LMASK;
                    bool middle = event.button.state & SDL_BUTTON_MMASK;
                    bool right = event.button.state & SDL_BUTTON_RMASK;
                    MouseAction action;
                    if (event.button.type == SDL_MOUSEBUTTONDOWN) {
                        if (event.button.clicks == 1) {
                            action = MouseActionDown;
                        } else {
                            action = MouseActionDbl;
                        }
                    } else {
                        action = MouseActionUp;
                    }
                    MouseEvent mouse_event = {
                        event.button.x,
                        event.button.y,
                        btn,
                        action,
                        MouseButtons {left, middle, right},
                        get_key_modifiers(),
                    };
                    on_mouse_move(&mouse_event);
                }
                break;
            case SDL_TEXTEDITING:
                {
                    TextInputEvent text_event = {
                        TextInputActionCandidate,
                        String(event.edit.text),
                    };
                    on_text_input(&text_event);
                    break;
                }
            case SDL_TEXTINPUT:
                {
                    TextInputEvent text_event = {
                        TextInputActionCommit,
                        String(event.text.text),
                    };
                    on_text_input(&text_event);
                    break;
                }
            case SDL_MOUSEWHEEL:
                {
                    MouseWheelEvent wheel_event = {
                        event.wheel.x,
                        event.wheel.y,
                        get_key_modifiers(),
                    };
                    on_mouse_wheel(&wheel_event);
                    break;
                }
            }
        }
        _audio_hardware->flush_events();

        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

        for (long i = 0; i < _widget_list.length(); i += 1) {
            Widget *widget = _widget_list.at(i);
            if (widget->_is_visible) {
                widget->draw(_projection);
            }
        }

        SDL_GL_SwapWindow(_window);
    }
}

void Gui::resize() {
    SDL_GL_GetDrawableSize(_window, &_width, &_height);
    glViewport(0, 0, _width, _height);
    _projection = glm::ortho(0.0f, (float)_width, (float)_height, 0.0f);
}

void Gui::init_widget(Widget *widget) {
    widget->_is_visible = true;
    widget->_gui_index = _widget_list.length();
    _widget_list.append(widget);
}

TextWidget * Gui::create_text_widget() {
    TextWidget *text_widget = create<TextWidget>(this);
    init_widget(text_widget);
    return text_widget;
}

FindFileWidget * Gui::create_find_file_widget() {
    FindFileWidget *find_file_widget = create<FindFileWidget>(this);
    init_widget(find_file_widget);
    return find_file_widget;
}

AudioEditWidget * Gui::create_audio_edit_widget() {
    AudioEditWidget *audio_edit_widget = create<AudioEditWidget>(this, _audio_hardware);
    init_widget(audio_edit_widget);
    return audio_edit_widget;
}

FontSize *Gui::get_font_size(int font_size) {
    auto *entry = _font_size_cache.maybe_get(font_size);
    if (entry)
        return entry->value;
    FontSize *font_size_object = create<FontSize>(_default_font_face, font_size);
    _font_size_cache.put(font_size, font_size_object);
    return font_size_object;
}

void Gui::fill_rect(const glm::vec4 &color, int x, int y, int w, int h) {
    glm::mat4 model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(x, y, 0.0f)),
                        glm::vec3(w, h, 0.0f));
    glm::mat4 mvp = _projection * model;
    fill_rect(color, mvp);
}

void Gui::fill_rect(const glm::vec4 &color, const glm::mat4 &mvp) {
    _shader_program_manager->_primitive_shader_program.bind();

    _shader_program_manager->_primitive_shader_program.set_uniform(
            _shader_program_manager->_primitive_uniform_color, color);

    _shader_program_manager->_primitive_shader_program.set_uniform(
            _shader_program_manager->_primitive_uniform_mvp, mvp);

    glBindVertexArray(_primitive_vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Gui::draw_image(const Image *img, int x, int y, int w, int h) {
    float scale_x = ((float)w) / ((float)img->width);
    float scale_y = ((float)h) / ((float)img->height);
    glm::mat4 model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(x, y, 0.0f)),
                        glm::vec3(scale_x, scale_y, 0.0f));
    glm::mat4 mvp = _projection * model;
    draw_image(img, mvp);
}

void Gui::draw_image(const Image *img, const glm::mat4 &mvp) {
    _spritesheet.draw(img, mvp);
}

void Gui::destroy_widget(Widget *widget) {
    if (widget == _mouse_over_widget)
        _mouse_over_widget = NULL;
    if (widget == _focus_widget)
        _focus_widget = NULL;

    if (widget->_gui_index >= 0)
        _widget_list.swap_remove(widget->_gui_index);
    destroy(widget, 1);
}

bool Gui::try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event) {
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

void Gui::on_mouse_move(const MouseEvent *event) {
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

void Gui::set_focus_widget(Widget *widget) {
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

void Gui::start_text_editing(int x, int y, int w, int h) {
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_SetTextInputRect(&rect);
    SDL_StartTextInput();
}

void Gui::stop_text_editing() {
    SDL_StopTextInput();
}

void Gui::on_text_input(const TextInputEvent *event) {
    if (!_focus_widget)
        return;

    _focus_widget->on_text_input(event);
}

void Gui::on_key_event(const KeyEvent *event) {
    if (_on_key_event(this, event))
        return;

    if (!_focus_widget)
        return;

    _focus_widget->on_key_event(event);
}

void Gui::on_mouse_wheel(const MouseWheelEvent *event) {
    if (!_focus_widget)
        return;

    _focus_widget->on_mouse_wheel(event);
}

void Gui::set_clipboard_string(const String &str) {
    int err = SDL_SetClipboardText(str.encode().raw());
    if (err)
        fprintf(stderr, "Error setting clipboard text: %s\n", SDL_GetError());
}

String Gui::get_clipboard_string() const {
    char* clip_text = SDL_GetClipboardText();
    bool ok;
    String str = String::decode(clip_text, &ok);
    SDL_free(clip_text);
    if (!ok)
        fprintf(stderr, "Reading invalid UTF-8 from the clipboard\n");
    return str;
}

bool Gui::clipboard_has_string() const {
    return SDL_HasClipboardText();
}


#include "gui.hpp"
#include "debug.hpp"
#include "audio_hardware.hpp"
#include "vertex_array.hpp"

uint32_t hash_int(const int &x) {
    return (uint32_t) x;
}

static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

GlobalSdlContext::GlobalSdlContext() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("SDL initialize");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
}

GlobalSdlContext::~GlobalSdlContext() {
    SDL_Quit();
}

void Gui::init_primitive_vertex_array() {
    glBindBuffer(GL_ARRAY_BUFFER, _static_geometry._rect_2d_vertex_buffer);
    glEnableVertexAttribArray(_shader_program_manager._primitive_attrib_position);
    glVertexAttribPointer(_shader_program_manager._primitive_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);
}

Gui::Gui(ResourceBundle *resource_bundle) :
    _running(true),
    _focus_window(nullptr),
    _utility_window(create_window(false)),
    _resource_bundle(resource_bundle),
    _spritesheet(this, "spritesheet"),
    _img_entry_dir(_spritesheet.get_image_info("img/entry-dir.png")),
    _img_entry_file(_spritesheet.get_image_info("img/entry-file.png")),
    _img_null(_spritesheet.get_image_info("img/null.png")),
    _primitive_vertex_array(this, init_primitive_vertex_array, this)
{

    ft_ok(FT_Init_FreeType(&_ft_library));
    _resource_bundle->get_file_buffer("font.ttf", _default_font_buffer);
    ft_ok(FT_New_Memory_Face(_ft_library, (FT_Byte*)_default_font_buffer.raw(),
                _default_font_buffer.length(), 0, &_default_font_face));

    _cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    _cursor_ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
}

Gui::~Gui() {
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

GuiWindow * Gui::get_window_from_sdl_id(Uint32 id) {
    SDL_Window *window = SDL_GetWindowFromID(id);
    if (!window)
        return nullptr;
    for (int i = 0; i < _window_list.length(); i += 1) {
        GuiWindow *gui_window = _window_list.at(i);
        if (gui_window->_window == window)
            return gui_window;
    }
    return nullptr;
}

void Gui::exec() {
    while (_running) {
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
                    GuiWindow *gui_window = get_window_from_sdl_id(event.key.windowID);
                    if (gui_window)
                        gui_window->on_key_event(&key_event);
                }
                break;
            case SDL_QUIT:
                _running = false;
                break;
            case SDL_WINDOWEVENT:
                {
                    GuiWindow *gui_window = get_window_from_sdl_id(event.window.windowID);
                    if (!gui_window)
                        break;
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        gui_window->resize();
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        gui_window->on_close();
                        break;
                    }
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
                    GuiWindow *gui_window = get_window_from_sdl_id(event.motion.windowID);
                    if (gui_window)
                        gui_window->on_mouse_move(&mouse_event);
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
                    GuiWindow *gui_window = get_window_from_sdl_id(event.button.windowID);
                    if (gui_window)
                        gui_window->on_mouse_move(&mouse_event);
                }
                break;
            case SDL_TEXTEDITING:
                {
                    TextInputEvent text_event = {
                        TextInputActionCandidate,
                        String(event.edit.text),
                    };
                    GuiWindow *gui_window = get_window_from_sdl_id(event.edit.windowID);
                    if (gui_window)
                        gui_window->on_text_input(&text_event);
                    break;
                }
            case SDL_TEXTINPUT:
                {
                    TextInputEvent text_event = {
                        TextInputActionCommit,
                        String(event.text.text),
                    };
                    GuiWindow *gui_window = get_window_from_sdl_id(event.text.windowID);
                    if (gui_window)
                        gui_window->on_text_input(&text_event);
                    break;
                }
            case SDL_MOUSEWHEEL:
                {
                    GuiWindow *gui_window = get_window_from_sdl_id(event.wheel.windowID);
                    if (gui_window) {
                        MouseWheelEvent wheel_event = {
                            gui_window->_last_mouse_x,
                            gui_window->_last_mouse_y,
                            event.wheel.x,
                            event.wheel.y,
                            get_key_modifiers(),
                        };

                        gui_window->on_mouse_wheel(&wheel_event);
                    }
                    break;
                }
            }
        }
        _audio_hardware.flush_events();

        for (long i = 0; i < _window_list.length(); i += 1) {
            GuiWindow *_gui_window = _window_list.at(i);
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
    _window_list.append(window);
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
    _window_list.swap_remove(index);
    if (index < _window_list.length())
        _window_list.at(index)->_gui_index = index;
    for (int i = 0; i < _vertex_array_list.length(); i += 1) {
        VertexArray *vertex_array = _vertex_array_list.at(i);
        vertex_array->remove_index(index);
    }
    destroy(window, 1);

    if (_window_list.length() == 1)
        _running = false;
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


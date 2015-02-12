#include "gui.hpp"
#include "debug.hpp"
#include "text_widget.hpp"

uint32_t hash_int(const int &x) {
    return (uint32_t) x;
}

static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

Gui::Gui(SDL_Window *window) :
    _text_shader_program(R"VERTEX(

#version 150 core

in vec3 VertexPosition;
in vec2 TexCoord;

out vec2 FragTexCoord;

uniform mat4 MVP;

void main(void)
{
    FragTexCoord = TexCoord;
    gl_Position = MVP * vec4(VertexPosition, 1.0);
}

)VERTEX", R"FRAGMENT(

#version 150 core

in vec2 FragTexCoord;
out vec4 FragColor;

uniform sampler2D Tex;
uniform vec4 Color;

void main(void)
{
    FragColor = vec4(1, 1, 1, texture(Tex, FragTexCoord).a) * Color;
}

)FRAGMENT", NULL),
    _primitive_shader_program(R"VERTEX(

#version 150 core

in vec3 VertexPosition;

uniform mat4 MVP;

void main(void) {
    gl_Position = MVP * vec4(VertexPosition, 1.0);
}

)VERTEX", R"FRAGMENT(

#version 150 core

out vec4 FragColor;

uniform vec4 Color;

void main(void) {
    FragColor = Color;
}

)FRAGMENT", NULL),
    _window(window),
    _mouse_over_widget(NULL),
    _focus_widget(NULL)
{
    _text_attrib_tex_coord = _text_shader_program.attrib_location("TexCoord");
    _text_attrib_position = _text_shader_program.attrib_location("VertexPosition");
    _text_uniform_mvp = _text_shader_program.uniform_location("MVP");
    _text_uniform_tex = _text_shader_program.uniform_location("Tex");
    _text_uniform_color = _text_shader_program.uniform_location("Color");

    _primitive_attrib_position = _primitive_shader_program.attrib_location("VertexPosition");
    _primitive_uniform_mvp = _primitive_shader_program.uniform_location("MVP");
    _primitive_uniform_color = _primitive_shader_program.uniform_location("Color");


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


    glEnableVertexAttribArray(_primitive_attrib_position);
    glVertexAttribPointer(_primitive_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    assert_no_gl_error();


    ft_ok(FT_Init_FreeType(&_ft_library));
    ft_ok(FT_New_Face(_ft_library, "assets/OpenSans-Regular.ttf", 0, &_default_font_face));

    _cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    _cursor_ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

    // disable vsync for now because of https://bugs.launchpad.net/unity/+bug/1415195
    SDL_GL_SetSwapInterval(0);

    glClearColor(0.3, 0.3, 0.3, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    resize();
}

Gui::~Gui() {
    SDL_FreeCursor(_cursor_default);
    SDL_FreeCursor(_cursor_ibeam);

    HashMap<int, FontSize *, hash_int>::Iterator it = _font_size_cache.value_iterator();
    while (it.has_next()) {
        FontSize *font_size_object = it.next();
        destroy(font_size_object);
    }

    FT_Done_Face(_default_font_face);
    FT_Done_FreeType(_ft_library);

    glDeleteBuffers(1, &_primitive_vertex_buffer);
    glDeleteVertexArrays(1, &_primitive_vertex_array);
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
                        event.key.keysym.mod,
                    };
                    on_key_event(key_event);
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
                    };
                    on_mouse_move(mouse_event);
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
                    };
                    on_mouse_move(mouse_event);
                }
                break;
            case SDL_TEXTEDITING:
                {
                    TextInputEvent text_event = {
                        TextInputActionCandidate,
                        String(event.edit.text),
                    };
                    on_text_input(text_event);
                    break;
                }
            case SDL_TEXTINPUT:
                {
                    TextInputEvent text_event = {
                        TextInputActionCommit,
                        String(event.text.text),
                    };
                    on_text_input(text_event);
                    break;
                }
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        for (int i = 0; i < _widget_list.length(); i += 1) {
            TextWidget *text_widget = reinterpret_cast<TextWidget*>(_widget_list.at(i));
            if (text_widget->is_visible())
                text_widget->draw(_projection);
        }

        SDL_GL_SwapWindow(_window);
        SDL_Delay(17);
    }
}

void Gui::resize() {
    SDL_GL_GetDrawableSize(_window, &_width, &_height);
    glViewport(0, 0, _width, _height);
    _projection = glm::ortho(0.0f, (float)_width, (float)_height, 0.0f);
}

TextWidget * Gui::create_text_widget() {
    TextWidget *text_widget = create<TextWidget>(this, _widget_list.length());
    _widget_list.append(text_widget);
    return text_widget;
}

FontSize *Gui::get_font_size(int font_size) {
    FontSize *font_size_object;
    if (_font_size_cache.get(font_size, &font_size_object))
        return font_size_object;
    font_size_object = create<FontSize>(this, _default_font_face, font_size);
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
    _primitive_shader_program.bind();

    _primitive_shader_program.set_uniform(_primitive_uniform_color, color);
    _primitive_shader_program.set_uniform(_primitive_uniform_mvp, mvp);


    glBindVertexArray(_primitive_vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Gui::remove_widget(Widget *widget) {
    TextWidget *text_widget = reinterpret_cast<TextWidget*>(widget);
    _widget_list.swap_remove(text_widget->_gui_index);
}

void Gui::on_mouse_move(const MouseEvent &event) {
    // if we're pressing a mouse button, the mouse over widget gets the event
    bool pressing_any_btn = (event.buttons.left || event.buttons.middle || event.buttons.right);
    if (_mouse_over_widget) {
        TextWidget *mouse_over_label = reinterpret_cast<TextWidget*>(_mouse_over_widget);
        int right = mouse_over_label->left() + mouse_over_label->width();
        int bottom = mouse_over_label->top() + mouse_over_label->height();
        bool in_bounds = (event.x >= mouse_over_label->left() &&
                event.y >= mouse_over_label->top() &&
                event.x < right &&
                event.y < bottom);

        MouseEvent mouse_event = event;
        mouse_event.x -= mouse_over_label->left();
        mouse_event.y -= mouse_over_label->top();

        if (in_bounds || pressing_any_btn) {
            if (pressing_any_btn && _mouse_over_widget != _focus_widget)
                set_focus_widget(_mouse_over_widget);
            mouse_over_label->on_mouse_move(mouse_event);
            return;
        } else {
            // not in bounds, not pressing any button
            if (event.action == MouseActionUp) {
                // give them the mouse up event
                mouse_over_label->on_mouse_move(mouse_event);
            }
            _mouse_over_widget = NULL;
            mouse_over_label->on_mouse_out(mouse_event);
        }
    }

    if (_mouse_over_widget != NULL)
        panic("expected _mouse_over_widget NULL");

    for (int i = 0; i < _widget_list.length(); i += 1) {
        Widget *widget = _widget_list.at(i);
        TextWidget *text_widget = reinterpret_cast<TextWidget*>(widget);

        int right = text_widget->left() + text_widget->width();
        int bottom = text_widget->top() + text_widget->height();
        if (event.x >= text_widget->left() && event.y >= text_widget->top() &&
            event.x < right && event.y < bottom)
        {
            MouseEvent mouse_event = event;
            mouse_event.x -= text_widget->left();
            mouse_event.y -= text_widget->top();

            _mouse_over_widget = text_widget;

            if (pressing_any_btn && _mouse_over_widget != _focus_widget)
                set_focus_widget(_mouse_over_widget);

            text_widget->on_mouse_over(mouse_event);
            text_widget->on_mouse_move(mouse_event);
            return;
        }
    }
}

void Gui::set_focus_widget(Widget *widget) {
    if (_focus_widget == widget)
        return;
    if (_focus_widget) {
        TextWidget *text_widget = reinterpret_cast<TextWidget*>(_focus_widget);
        _focus_widget = NULL;
        text_widget->on_lose_focus();
    }
    if (!widget)
        return;
    _focus_widget = widget;
    TextWidget *text_widget = reinterpret_cast<TextWidget*>(widget);
    text_widget->on_gain_focus();
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

void Gui::on_text_input(const TextInputEvent &event) {
    if (!_focus_widget)
        panic("focus widget non NULL and text input on");

    TextWidget *text_widget = reinterpret_cast<TextWidget*>(_focus_widget);
    text_widget->on_text_input(event);
}

void Gui::on_key_event(const KeyEvent &event) {
    if (!_focus_widget)
        return;

    TextWidget *text_widget = reinterpret_cast<TextWidget*>(_focus_widget);
    text_widget->on_key_event(event);
}

void Gui::set_clipboard_string(const String &str) {
    int err = SDL_SetClipboardText(str.encode().raw());
    if (err)
        fprintf(stderr, "Error setting clipboard text: %s\n", SDL_GetError());
}

String Gui::get_clipboard_string() const {
    char* clip_text = SDL_GetClipboardText();
    bool ok;
    String str = String::decode(clip_text, ok);
    SDL_free(clip_text);
    if (!ok)
        fprintf(stderr, "Reading invalid UTF-8 from the clipboard\n");
    return str;
}

bool Gui::clipboard_has_string() const {
    return SDL_HasClipboardText();
}

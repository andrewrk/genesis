#include "gui.hpp"
#include "debug.hpp"
#include "os.hpp"
#include "audio_graph.hpp"

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
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GENESIS_DEBUG_MODE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

}

GlobalGlfwContext::~GlobalGlfwContext() {
    glfwTerminate();
}

static void audio_device_callback(void *userdata) {
    Gui *gui = (Gui *)userdata;
    gui->events.trigger(EventAudioDeviceChange);
}

static void midi_device_callback(void *userdata) {
    Gui *gui = (Gui *)userdata;
    gui->events.trigger(EventMidiDeviceChange);
}

Gui::Gui(GenesisContext *context, ResourceBundle *resource_bundle) :
    _running(true),
    _focus_window(nullptr),
    _utility_window(create_utility_window()),
    _resource_bundle(resource_bundle),
    _spritesheet(this, "spritesheet"),
    img_entry_dir(_spritesheet.get_image_info("font-awesome/folder.png")),
    img_entry_dir_open(_spritesheet.get_image_info("font-awesome/folder-open.png")),
    img_entry_file(_spritesheet.get_image_info("font-awesome/file.png")),
    img_plus(_spritesheet.get_image_info("font-awesome/plus-square.png")),
    img_minus(_spritesheet.get_image_info("font-awesome/minus-square.png")),
    img_microphone(_spritesheet.get_image_info("font-awesome/microphone.png")),
    img_volume_up(_spritesheet.get_image_info("font-awesome/volume-up.png")),
    img_check(_spritesheet.get_image_info("font-awesome/check.png")),
    img_caret_right(_spritesheet.get_image_info("font-awesome/caret-right.png")),
    img_arrow_up(_spritesheet.get_image_info("font-awesome/arrow-up.png")),
    img_arrow_down(_spritesheet.get_image_info("font-awesome/arrow-down.png")),
    img_arrow_left(_spritesheet.get_image_info("font-awesome/arrow-left.png")),
    img_arrow_right(_spritesheet.get_image_info("font-awesome/arrow-right.png")),
    img_music(_spritesheet.get_image_info("font-awesome/music.png")),
    img_plug(_spritesheet.get_image_info("font-awesome/plug.png")),
    img_null(_spritesheet.get_image_info("img/null.png")),
    img_play_head(_spritesheet.get_image_info("img/play_head.png")),
    _genesis_context(context),
    dragging(false),
    drag_data(nullptr),
    drag_window(nullptr)
{

    ft_ok(FT_Init_FreeType(&_ft_library));
    _resource_bundle->get_file_buffer("font.ttf", _default_font_buffer);
    ft_ok(FT_New_Memory_Face(_ft_library, (FT_Byte*)_default_font_buffer.raw(),
                _default_font_buffer.length(), 0, &_default_font_face));

    cursor_default = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    cursor_ibeam = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    cursor_hresize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    cursor_vresize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

    genesis_set_audio_device_callback(_genesis_context, audio_device_callback, this);
    genesis_set_midi_device_callback(_genesis_context, midi_device_callback, this);

    genesis_refresh_audio_devices(_genesis_context);
    genesis_refresh_midi_devices(_genesis_context);

    gui_mutex.lock();
}

Gui::~Gui() {
    gui_mutex.unlock();

    glfwDestroyCursor(cursor_default);
    glfwDestroyCursor(cursor_ibeam);

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
    gui_mutex.unlock();
    fps = 60.0;
    double last_time = os_get_time();
    while (_running) {
        gui_mutex.lock();
        genesis_flush_events(_genesis_context);
        if (genesis_underrun_occurred(_genesis_context))
            fprintf(stderr, "buffer underrun\n");
        glfwPollEvents();
        events.trigger(EventFlushEvents);
        gui_mutex.unlock();

        _utility_window->draw();

        double this_time = os_get_time();
        double delta = this_time - last_time;
        last_time = this_time;
        double this_fps = 1.0 / delta;
        fps = fps * 0.90 + this_fps * 0.10;
    }
    gui_mutex.lock();
}

FontSize *Gui::get_font_size(int font_size) {
    auto *entry = _font_size_cache.maybe_get(font_size);
    if (entry)
        return entry->value;
    FontSize *font_size_object = create<FontSize>(_default_font_face, font_size);
    _font_size_cache.put(font_size, font_size_object);
    return font_size_object;
}

void Gui::draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp) {
    _spritesheet.draw(window, img, mvp);
}

void Gui::draw_image_color(GuiWindow *window, const SpritesheetImage *img,
        const glm::mat4 &mvp, const glm::vec4 &color)
{
    _spritesheet.draw_color(window, img, mvp, color);
}

GuiWindow *Gui::create_window(int left, int top, int width, int height) {
    return create_generic_window(true, left, top, width, height);
}

GuiWindow *Gui::create_utility_window() {
    return create_generic_window(false, 0, 0, 100, 100);
}

GuiWindow *Gui::create_generic_window(bool with_borders, int left, int top, int width, int height) {
    GuiWindow *window = create<GuiWindow>(this, with_borders, left, top, width, height);
    window->_gui_index = _window_list.length();
    ok_or_panic(_window_list.append(window));
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
    destroy(window, 1);

    if (_window_list.length() == 1)
        _running = false;
}

void Gui::start_drag(GuiWindow *gui_window, const MouseEvent *event, DragData *new_drag_data) {
    if (drag_data)
        end_drag();
    drag_window = gui_window;
    drag_orig_event = *event;
    drag_data = new_drag_data;
    dragging = false;
}

void Gui::end_drag() {
    if (drag_data->destruct)
        drag_data->destruct(drag_data);
    destroy(drag_data, 1);
    drag_data = nullptr;
    dragging = false;
    drag_window = nullptr;
}

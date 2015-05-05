#include "gui.hpp"
#include "debug.hpp"
#include "os.hpp"

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
    glfwWindowHint(GLFW_STENCIL_BITS, 1);

}

GlobalGlfwContext::~GlobalGlfwContext() {
    glfwTerminate();
}

static void audio_device_callback(void *userdata) {
    Gui *gui = (Gui *)userdata;
    gui->dispatch_handlers(gui->audio_device_handlers);
}

static void midi_device_callback(void *userdata) {
    Gui *gui = (Gui *)userdata;
    gui->dispatch_handlers(gui->midi_device_handlers);
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
    img_null(_spritesheet.get_image_info("img/null.png")),
    _genesis_context(context),
    fps_callback(nullptr),
    fps_callback_userdata(nullptr)
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
        genesis_flush_events(_genesis_context);
        glfwPollEvents();
        _utility_window->draw();

        double this_time = os_get_time();
        double delta = this_time - last_time;
        last_time = this_time;
        double this_fps = 1.0 / delta;
        fps = fps * 0.90 + this_fps * 0.10;
        if (fps_callback)
            fps_callback(fps_callback_userdata);
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
    if (_window_list.append(window))
        panic("out of memory");
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

static void add_handler(List<Gui::Handler> &list, void (*fn)(void *), void *userdata) {
    ok_or_panic(list.resize(list.length() + 1));
    Gui::Handler *handler = &list.at(list.length() - 1);
    handler->fn = fn;
    handler->userdata = userdata;
}

static int get_handler_index(List<Gui::Handler> &list, void (*fn)(void *)) {
    for (int i = 0; i < list.length(); i += 1) {
        if (list.at(i).fn == fn)
            return i;
    }
    panic("handler not found");
}

static void remove_handler(List<Gui::Handler> &list, void (*fn)(void *)) {
    int index = get_handler_index(list, fn);
    list.swap_remove(index);
}

void Gui::attach_audio_device_callback(void (*fn)(void *), void *userdata) {
    add_handler(audio_device_handlers, fn, userdata);
}

void Gui::detach_audio_device_callback(void (*fn)(void *)) {
    remove_handler(audio_device_handlers, fn);
}

void Gui::attach_midi_device_callback(void (*fn)(void *), void *userdata) {
    add_handler(midi_device_handlers, fn, userdata);
}

void Gui::detach_midi_device_callback(void (*fn)(void *)) {
    remove_handler(midi_device_handlers, fn);
}

void Gui::dispatch_handlers(const List<Handler> &list) {
    MutexLocker locker(&gui_mutex);
    for (int i = 0; i < list.length(); i += 1) {
        const Handler *handler = &list.at(i);
        handler->fn(handler->userdata);
    }
}

void Gui::set_fps_callback(void (*fn)(void *), void *userdata) {
    fps_callback_userdata = userdata;
    fps_callback = fn;
}

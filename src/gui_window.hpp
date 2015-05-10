#ifndef GUI_WINDOW_HPP
#define GUI_WINDOW_HPP

#include "list.hpp"
#include "glm.hpp"
#include "key_event.hpp"
#include "mouse_event.hpp"
#include "drag_event.hpp"
#include "string.hpp"
#include "glfw.hpp"
#include "threads.hpp"
#include "event_dispatcher.hpp"

#include <atomic>
using std::atomic_bool;

class Gui;
class Widget;
struct SpritesheetImage;
class MenuWidgetItem;
class MenuWidget;
class ContextMenuWidget;

class GuiWindow {
public:
    GuiWindow(Gui *gui, bool is_normal_window, int left, int top, int width, int height);
    ~GuiWindow();

    void draw();

    void remove_widget(Widget *widget);
    void set_focus_widget(Widget *widget);

    void maximize();

    void set_cursor_beam();
    void set_cursor_default();
    void set_cursor_hresize();
    void set_cursor_vresize();

    bool try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event);

    void fill_rect(const glm::vec4 &color, const glm::mat4 &mvp);
    void fill_rect(const glm::vec4 &color, int x, int y, int w, int h);
    void draw_image(const SpritesheetImage *img, int x, int y, int w, int h);
    void fill_rect_gradient(const glm::vec4 &top_color, const glm::vec4 &bottom_color, const glm::mat4 &mvp);

    void set_clipboard_string(const String &str);
    String get_clipboard_string() const;
    bool clipboard_has_string() const;

    void set_main_widget(Widget *widget);
    // coords are the rectangle that originated the menu. you might only need left and top.
    ContextMenuWidget * pop_context_menu(MenuWidgetItem *menu_widget_item, int left, int top, int width, int height);
    void refresh_context_menu();

    void start_drag(const MouseEvent *event, DragData *drag_data);

    void *_userdata;
    // index into Gui's list of windows
    int _gui_index;

    Gui *gui;
    GLFWwindow *window;
    GLuint vertex_array_object;

    // pixels
    int _width;
    int _height;

    // screen coordinates
    int _client_width;
    int _client_height;
    int client_left;
    int client_top;

    glm::mat4 _projection;
    Widget *_mouse_over_widget;
    Widget *_focus_widget;
    MenuWidget *menu_widget;

    EventDispatcher events;

    bool _is_iconified;
    bool is_visible;

    double _last_click_time;
    MouseButton _last_click_button;
    double _double_click_timeout;

    Thread thread;
    atomic_bool running;
    atomic_bool viewport_update_queued;

    Widget *main_widget;
    ContextMenuWidget *context_menu;

    void layout_main_widget();
    int get_modifiers();
    void on_mouse_move(const MouseEvent *event);

    bool is_maximized;
    Widget *drag_widget;

    void window_iconify_callback(int iconified);
    void framebuffer_size_callback(int width, int height);
    void window_size_callback(int width, int height);
    void key_callback(int key, int scancode, int action, int mods);
    void charmods_callback(unsigned int codepoint, int mods);
    void window_close_callback();
    void cursor_pos_callback(double xpos, double ypos);
    void mouse_button_callback(int button, int action, int mods);
    void scroll_callback(double xoffset, double yoffset);
    void window_pos_callback(int left, int top);

    void setup_context();
    void teardown_context();
    void destroy_context_menu();
    void got_window_size(int width, int height);
    void got_window_pos(int left, int top);
    bool widget_is_menu(Widget *widget);

    bool forward_drag_event(Widget *widget, const DragEvent *event);

};

#endif

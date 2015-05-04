#ifndef MENU_WIDGET_HPP
#define MENU_WIDGET_HPP

#include "widget.hpp"
#include "key_event.hpp"
#include "string.hpp"
#include "label.hpp"

struct SpritesheetImage;

class GuiWindow;
class MenuWidgetItem {
public:
    MenuWidgetItem(GuiWindow *gui_window);
    MenuWidgetItem(GuiWindow *gui_window, String name, int mnemonic_index, KeySequence shortcut);
    ~MenuWidgetItem();

    MenuWidgetItem *add_menu(const String &name, const KeySequence &shortcut);
    MenuWidgetItem *add_menu(const String &name, int mnemonic_index, const KeySequence &shortcut);

    void set_activate_handler(void (*fn)(void *), void *userdata) {
        this->activate_handler = fn;
        this->userdata = userdata;
    }

    void set_enabled(bool enabled) {
        this->enabled = enabled;
    }

    void set_caption(const String &caption);
    void set_caption(const String &caption, int mnemonic_index);

    void set_icon(const SpritesheetImage *icon) {
        this->icon = icon;
    }

    GuiWindow *gui_window;

    Label label;
    glm::mat4 label_model;
    glm::mat4 mnemonic_model;
    Label shortcut_label;
    glm::mat4 shortcut_label_model;

    int top;
    int bottom;

    int mnemonic_index;
    KeySequence shortcut;
    List<MenuWidgetItem *> children;
    void (*activate_handler)(void *);
    void *userdata;
    bool enabled;
    const SpritesheetImage *icon;
    glm::mat4 icon_model;

    void activate();
    VirtKey get_mnemonic_key();

};

class ContextMenuWidget : public Widget {
public:
    ContextMenuWidget(MenuWidgetItem *menu_widget_item);
    ~ContextMenuWidget() override;

    void draw(const glm::mat4 &projection) override;

    int min_width() const override { return calculated_width; }
    int max_width() const override { return calculated_width; }
    int min_height() const override { return calculated_height; }
    int max_height() const override { return calculated_height; }

    void on_mouse_move(const MouseEvent *) override;
    void on_mouse_out(const MouseEvent *) override;
    bool on_key_event(const KeyEvent *) override;

    void on_resize() override { update_model(); }

    MenuWidgetItem *menu_widget_item;
    int calculated_width;
    int calculated_height;
    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int spacing;
    int item_padding_top;
    int item_padding_bottom;
    int icon_width;
    int icon_height;
    int icon_spacing;
    glm::vec4 bg_color;
    glm::vec4 activated_color;
    glm::vec4 text_color;
    glm::vec4 text_disabled_color;
    glm::vec4 activated_text_color;
    glm::mat4 bg_model;
    void *userdata;
    void (*on_destroy)(ContextMenuWidget *);
    MenuWidgetItem *activated_item;

    void update_model();
    MenuWidgetItem *get_item_at(int y);
    int get_menu_widget_index(MenuWidgetItem *item);
};

class MenuWidget : public Widget {
public:
    MenuWidget(GuiWindow *gui_window);
    ~MenuWidget() override;

    MenuWidgetItem *add_menu(const String &name);
    MenuWidgetItem *add_menu(const String &name, int mnemonic_index);

    void draw(const glm::mat4 &projection) override;

    int min_width() const override {
        return calculated_width;
    }

    int max_width() const override {
        return -1;
    }

    int min_height() const override {
        return calculated_height;
    }

    int max_height() const override {
        return calculated_height;
    }

    void on_resize() override {
        update_model();
    }

    void on_mouse_move(const MouseEvent *event) override;
    bool on_key_event(const KeyEvent *) override;

    struct TopLevelMenu {
        MenuWidgetItem *item;
        int left;
        int right;
    };
    List<TopLevelMenu> children;
    glm::vec4 bg_color;
    glm::vec4 text_color;
    glm::vec4 activated_color;
    glm::vec4 activated_text_color;
    glm::mat4 bg_model;
    int calculated_width;
    int calculated_height;
    int spacing_left;
    int spacing_right;
    int spacing_top;
    int spacing_bottom;
    TopLevelMenu *activated_item;
    bool down_unclick;

    void update_model();
    void pop_top_level(TopLevelMenu *child, bool select_first_item);
    TopLevelMenu *get_child_at(int x, int y);
    int get_item_index(TopLevelMenu *item);
    bool dispatch_shortcut(MenuWidgetItem *parent, const KeyEvent *event);
};

#endif

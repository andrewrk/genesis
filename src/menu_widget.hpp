#ifndef MENU_WIDGET_HPP
#define MENU_WIDGET_HPP

#include "widget.hpp"
#include "key_event.hpp"
#include "string.hpp"
#include "label.hpp"

struct KeySequence {
    int modifiers;
    VirtKey key;
};

bool null_key_sequence(const KeySequence &seq);
String key_sequence_to_string(const KeySequence &seq);

class GuiWindow;
class MenuWidgetItem {
public:
    MenuWidgetItem(GuiWindow *gui_window, String name, int mnemonic_index, KeySequence shortcut);
    ~MenuWidgetItem();

    MenuWidgetItem *add_menu(String name, int mnemonic_index, KeySequence shortcut) {
        MenuWidgetItem *new_item = create<MenuWidgetItem>(gui_window, name, mnemonic_index, shortcut);
        if (children.append(new_item))
            panic("out of memory");
        return new_item;
    }

    void set_activate_handler(void (*fn)(void *), void *userdata) {
        this->activate_handler = fn;
        this->userdata = userdata;
    }

    GuiWindow *gui_window;

    Label label;
    glm::mat4 label_model;
    Label shortcut_label;
    glm::mat4 shortcut_label_model;

    int top;
    int bottom;

    int mnemonic_index;
    KeySequence shortcut;
    List<MenuWidgetItem *> children;
    void (*activate_handler)(void *);
    void *userdata;
};

class ContextMenuWidget : public Widget {
public:
    ContextMenuWidget(MenuWidgetItem *menu_widget_item);
    ~ContextMenuWidget();

    void draw(const glm::mat4 &projection) override;

    int min_width() const override { return calculated_width; }
    int max_width() const override { return calculated_width; }
    int min_height() const override { return calculated_height; }
    int max_height() const override { return calculated_height; }

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
    glm::vec4 bg_color;
    glm::vec4 text_color;
    glm::mat4 bg_model;
    void *userdata;
    void (*on_destroy)(ContextMenuWidget *);

    void update_model();
};

class MenuWidget : public Widget {
public:
    MenuWidget(GuiWindow *gui_window);
    ~MenuWidget() override {
        for (int i = 0; i < children.length(); i += 1) {
            TopLevelMenu *child = &children.at(i);
            destroy(child->item, 1);
        }
    }

    MenuWidgetItem *add_menu(String name, int mnemonic_index) {
        if (children.resize(children.length() + 1))
            panic("out of memory");

        TopLevelMenu *child = &children.at(children.length() - 1);
        child->item = create<MenuWidgetItem>(gui_window, name, mnemonic_index, no_shortcut());
        update_model();
        return child->item;
    }

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

    static KeySequence no_shortcut() {
        return {
            .modifiers = -1,
            .key = VirtKeyUnknown,
        };
    }

    static KeySequence alt_shortcut(VirtKey key) {
        return {
            .modifiers = KeyModAlt,
            .key = key,
        };
    }

    static KeySequence ctrl_shortcut(VirtKey key) {
        return {
            .modifiers = KeyModControl,
            .key = key,
        };
    }

    static KeySequence shortcut(VirtKey key) {
        return {
            .modifiers = 0,
            .key = key,
        };
    }

    struct TopLevelMenu {
        MenuWidgetItem *item;
        int left;
        int right;
    };
    List<TopLevelMenu> children;
    glm::vec4 bg_color;
    glm::vec4 text_color;
    glm::mat4 bg_model;
    int calculated_width;
    int calculated_height;
    int spacing_left;
    int spacing_right;
    int spacing_top;
    int spacing_bottom;
    bool activated;

    void update_model();
    void pop_top_level(TopLevelMenu *child);
    TopLevelMenu *get_child_at(int x, int y);
};

#endif

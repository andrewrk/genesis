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

class MenuWidgetItem {
public:
    MenuWidgetItem(String name, int mnemonic_index, KeySequence shortcut) :
        name(name),
        mnemonic_index(mnemonic_index),
        shortcut(shortcut)
    {
    }

    ~MenuWidgetItem() {
        panic("TODO");
    }

    MenuWidgetItem *add_menu(String name, int mnemonic_index, KeySequence shortcut) {
        panic("TODO");
    }

    void set_activate_handler(void (*fn)(MenuWidgetItem *)) {
        activate_handler = fn;
    }

private:
    String name;
    int mnemonic_index;
    KeySequence shortcut;
    List<MenuWidgetItem *> children;
    GuiWindow *window;
    void (*activate_handler)(MenuWidgetItem *item);
};

class GuiWindow;
class MenuWidget : public Widget {
public:
    MenuWidget(GuiWindow *gui_window);
    ~MenuWidget() override { }

    MenuWidgetItem *add_menu(String name, int mnemonic_index) {
        if (children.resize(children.length() + 1))
            panic("out of memory");

        TopLevelMenu *child = &children.at(children.length() - 1);
        child->item = create<MenuWidgetItem>(name, mnemonic_index, no_shortcut());
        child->label = create<Label>(gui);
        update_model();
        return child->item;
    }

    void draw(const glm::mat4 &projection);

    int min_width() const override {
        return calculated_width;
    }

    int max_width() const override {
        return calculated_width;
    }

    int min_height() const override {
        return calculated_height;
    }

    int max_height() const override {
        return calculated_height;
    }


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
        Label *label;
        MenuWidgetItem *item;
        glm::mat4 label_mvp;
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

    void update_model() {
        int next_left = 0;
        int max_label_height = 0;
        for (int i = 0; i < children.length(); i += 1) {
            TopLevelMenu *child = &children.at(i);
            child->label->update();
            max_label_height = max(max_label_height, child->label->height());

            child->left = next_left;
            next_left += spacing_left + child->label->width() + spacing_right;
            child->right = next_left;

            int label_left = left + child->left + spacing_left;
            int label_top = top + spacing_top;
            child->label_mvp = glm::translate(
                                    glm::mat4(1.0f),
                                    glm::vec3(label_left, label_top, 0.0f));
        }
        calculated_width = next_left;
        calculated_height = spacing_top + max_label_height + spacing_bottom;
        bg_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(left, top, 0.0f)),
                        glm::vec3(calculated_width, calculated_height, 1.0f));
    }
};

#endif

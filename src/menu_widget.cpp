#include "menu_widget.hpp"
#include "color.hpp"
#include "gui_window.hpp"

bool null_key_sequence(const KeySequence &seq) {
    return seq.modifiers == -1;
}

String key_sequence_to_string(const KeySequence &seq) {
    String result;
    if (key_mod_ctrl(seq.modifiers))
        result.append("Ctrl+");
    if (key_mod_shift(seq.modifiers))
        result.append("Shift+");
    if (key_mod_alt(seq.modifiers))
        result.append("Alt+");
    if (key_mod_super(seq.modifiers))
        result.append("Super+");
    result.append(virt_key_to_string(seq.key));
    return result;
}

MenuWidgetItem::MenuWidgetItem(GuiWindow *gui_window, String name, int mnemonic_index, KeySequence shortcut) :
    gui_window(gui_window),
    label(gui_window->_gui),
    shortcut_label(gui_window->_gui),
    mnemonic_index(mnemonic_index),
    shortcut(shortcut)
{
    label.set_text(name);
    if (!null_key_sequence(shortcut))
        shortcut_label.set_text(key_sequence_to_string(shortcut));
}

MenuWidgetItem::~MenuWidgetItem() {
    for (int i = 0; i < children.length(); i += 1) {
        MenuWidgetItem *child = children.at(i);
        destroy(child, 1);
    }
}

ContextMenuWidget::ContextMenuWidget(MenuWidgetItem *menu_widget_item) :
    Widget(menu_widget_item->gui_window),
    menu_widget_item(menu_widget_item),
    padding_top(2),
    padding_bottom(2),
    padding_left(12),
    padding_right(2),
    spacing(12),
    item_padding_top(4),
    item_padding_bottom(4),
    bg_color(parse_color("#FCFCFC")),
    activated_color(parse_color("#2B71BC")),
    text_color(parse_color("#353535")),
    activated_text_color(parse_color("#f0f0f0")),
    userdata(nullptr),
    on_destroy(nullptr),
    activated_item(nullptr)
{
    update_model();
}

ContextMenuWidget::~ContextMenuWidget() {
    if (on_destroy)
        on_destroy(this);
}

void ContextMenuWidget::update_model() {
    calculated_width = 0;
    int next_top = padding_top;
    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        child->label.update();
        child->shortcut_label.update();
        child->top = next_top;
        next_top += item_padding_top +
            max(child->label.height(), child->shortcut_label.height()) +
            item_padding_bottom;
        child->bottom = next_top;

        int this_width = padding_left + child->label.width() +
            spacing + child->shortcut_label.width() + padding_right;
        calculated_width = max(calculated_width, this_width);
    }
    calculated_height = next_top + padding_bottom;

    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        int label_left = left + padding_left;
        int label_top = top + child->top + item_padding_top;
        child->label_model = glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(label_left, label_top, 0.0f));
        if (!null_key_sequence(child->shortcut)) {
            int shortcut_label_left = calculated_width - padding_right - child->shortcut_label.width();
            int shortcut_label_top = label_top;
            child->shortcut_label_model = glm::translate(
                                    glm::mat4(1.0f),
                                    glm::vec3(shortcut_label_left, shortcut_label_top, 0.0f));
        }
    }

    bg_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(left, top, 0.0f)),
                    glm::vec3(calculated_width, calculated_height, 1.0f));
}

void ContextMenuWidget::draw(const glm::mat4 &projection) {
    // background
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        glm::vec4 this_text_color = (child == activated_item) ? activated_text_color : text_color;
        if (child == activated_item) {
            gui_window->fill_rect(activated_color,
                left, top + child->top,
                calculated_width, child->bottom - child->top);
        }

        glm::mat4 label_mvp = projection * child->label_model;
        child->label.draw(gui_window, label_mvp, this_text_color);
        if (!null_key_sequence(child->shortcut)) {
            glm::mat4 shortcut_label_mvp = projection * child->shortcut_label_model;
            child->shortcut_label.draw(gui_window, shortcut_label_mvp, this_text_color);
        }
    }
}

MenuWidgetItem *ContextMenuWidget::get_item_at(int y) {
    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        if (y >= child->top && y < child->bottom)
            return child;
    }
    return nullptr;
}

void ContextMenuWidget::on_mouse_move(const MouseEvent *event) {
    MenuWidgetItem *hover_item = get_item_at(event->y);
    switch (event->action) {
        case MouseActionMove:
            activated_item = hover_item;
            break;
        case MouseActionUp:
            gui_window->destroy_context_menu();
            if (hover_item->activate_handler) {
                hover_item->activate_handler(hover_item->userdata);
            } else {
                fprintf(stderr, "No handler attached: %s\n", hover_item->label.text().encode().raw());
            }
            break;
        default:
            return;
    }
}

void ContextMenuWidget::on_mouse_out(const MouseEvent *event) {
    activated_item = nullptr;
}

MenuWidget::MenuWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    bg_color(parse_color("#CECECE")),
    text_color(parse_color("#353535")),
    activated_color(parse_color("#2E5783")),
    activated_text_color(parse_color("#f0f0f0")),
    spacing_left(12),
    spacing_right(12),
    spacing_top(4),
    spacing_bottom(4),
    activated_item(nullptr)
{
}

void MenuWidget::draw(const glm::mat4 &projection) {
    // background
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        glm::mat4 label_mvp = projection * child->item->label_model;
        if (activated_item == child) {
            gui_window->fill_rect(activated_color,
                    left + child->left, top,
                    child->right - child->left, calculated_height);
            child->item->label.draw(gui_window, label_mvp, activated_text_color);
        } else {
            child->item->label.draw(gui_window, label_mvp, text_color);
        }
    }
}

void MenuWidget::update_model() {
    int next_left = 0;
    int max_label_height = 0;
    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        child->item->label.update();
        max_label_height = max(max_label_height, child->item->label.height());

        child->left = next_left;
        next_left += spacing_left + child->item->label.width() + spacing_right;
        child->right = next_left;

        int label_left = left + child->left + spacing_left;
        int label_top = top + spacing_top;
        child->item->label_model = glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(label_left, label_top, 0.0f));
    }
    calculated_width = next_left;
    calculated_height = spacing_top + max_label_height + spacing_bottom;
    bg_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(left, top, 0.0f)),
                    glm::vec3(width, calculated_height, 1.0f));
}

static void on_context_menu_destroy(ContextMenuWidget *context_menu) {
    MenuWidget *menu_widget = (MenuWidget*)context_menu->userdata;
    menu_widget->activated_item = nullptr;
}

void MenuWidget::pop_top_level(TopLevelMenu *child) {
    ContextMenuWidget *context_menu = gui_window->pop_context_menu(child->item,
            left + child->left, top,
            child->right - child->left, calculated_height);
    context_menu->userdata = this;
    context_menu->on_destroy = on_context_menu_destroy;
    activated_item = child;
}

MenuWidget::TopLevelMenu *MenuWidget::get_child_at(int x, int y) {
    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        if (x >= child->left && x < child->right &&
            y >= 0 && y < calculated_height)
        {
            return child;
        }
    }
    return nullptr;
}

void MenuWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionDown:
            {
                TopLevelMenu *child = get_child_at(event->x, event->y);
                if (!child)
                    return;
                pop_top_level(child);
                break;
            }
        case MouseActionMove:
            {
                if (!activated_item)
                    return;
                TopLevelMenu *child = get_child_at(event->x, event->y);
                if (child) {
                    pop_top_level(child);
                } else if (gui_window->context_menu) {
                    MouseEvent event_for_child = *event;
                    event_for_child.x -= left;
                    event_for_child.y -= top;
                    gui_window->try_mouse_move_event_on_widget(gui_window->context_menu, &event_for_child);
                }
                break;
            }
        default:
            return;
    }
}

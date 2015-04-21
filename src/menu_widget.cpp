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
    padding_top(0),
    padding_bottom(0),
    padding_left(12),
    padding_right(2),
    spacing(12),
    item_padding_top(4),
    item_padding_bottom(4),
    bg_color(parse_color("#FCFCFC")),
    text_color(parse_color("#353535"))
{
    update_model();
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
        glm::mat4 label_mvp = projection * child->label_model;
        child->label.draw(gui_window, label_mvp, text_color);
        if (!null_key_sequence(child->shortcut)) {
            glm::mat4 shortcut_label_mvp = projection * child->shortcut_label_model;
            child->shortcut_label.draw(gui_window, shortcut_label_mvp, text_color);
        }
    }
}


MenuWidget::MenuWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    bg_color(parse_color("#CECECE")),
    text_color(parse_color("#353535")),
    spacing_left(12),
    spacing_right(12),
    spacing_top(4),
    spacing_bottom(4)
{
}

void MenuWidget::draw(const glm::mat4 &projection) {
    // background
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        glm::mat4 label_mvp = projection * child->item->label_model;
        child->item->label.draw(gui_window, label_mvp, text_color);
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

void MenuWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionDown:
            {
                TopLevelMenu *child = get_child_at(event->x, event->y);
                if (!child)
                    return;
                gui_window->pop_context_menu(child->item,
                        left + child->left, top,
                        child->right - child->left, calculated_height);
                break;
            }
        default:
            return;
    }
}

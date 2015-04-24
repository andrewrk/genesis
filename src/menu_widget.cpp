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

bool key_sequence_match(const KeySequence &seq, const KeyEvent *event) {
    return (seq.modifiers == event->modifiers) && (seq.key == event->virt_key);
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

MenuWidgetItem::MenuWidgetItem(GuiWindow *gui_window) :
    MenuWidgetItem(gui_window, "", -1, no_shortcut())
{
}

MenuWidgetItem::~MenuWidgetItem() {
    for (int i = 0; i < children.length(); i += 1) {
        MenuWidgetItem *child = children.at(i);
        destroy(child, 1);
    }
}

MenuWidgetItem *MenuWidgetItem::add_menu(const String &name, int mnemonic_index, const KeySequence &shortcut) {
    MenuWidgetItem *new_item = create<MenuWidgetItem>(gui_window, name, mnemonic_index, shortcut);
    if (children.append(new_item))
        panic("out of memory");
    return new_item;
}

MenuWidgetItem *MenuWidgetItem::add_menu(const String &name_orig, const KeySequence &shortcut) {
    // parse for mnemonic_index
    int mnemonic_index = -1;
    String name = name_orig;
    for (int i = 0; i < name.length() - 1; i += 1) {
        uint32_t codepoint = name.at(i);
        uint32_t next_codepoint = name.at(i + 1);
        if (codepoint == (uint32_t)'&' && next_codepoint != (uint32_t)'&') {
            mnemonic_index = i;
            name.remove_range(i, i + 1);
            break;
        }
    }
    return add_menu(name, mnemonic_index, shortcut);
}

VirtKey MenuWidgetItem::get_mnemonic_key() {
    if (mnemonic_index == -1)
        return VirtKeyUnknown;

    uint32_t orig_codepoint = label.text().at(mnemonic_index);
    uint32_t lower_codepoint = String::char_to_lower(orig_codepoint);
    switch (lower_codepoint) {
        default: return VirtKeyUnknown;
        case (uint32_t)'0': return VirtKey0;
        case (uint32_t)'1': return VirtKey1;
        case (uint32_t)'2': return VirtKey2;
        case (uint32_t)'3': return VirtKey3;
        case (uint32_t)'4': return VirtKey4;
        case (uint32_t)'5': return VirtKey5;
        case (uint32_t)'6': return VirtKey6;
        case (uint32_t)'7': return VirtKey7;
        case (uint32_t)'8': return VirtKey8;
        case (uint32_t)'9': return VirtKey9;
        case (uint32_t)'a': return VirtKeyA;
        case (uint32_t)'b': return VirtKeyB;
        case (uint32_t)'c': return VirtKeyC;
        case (uint32_t)'d': return VirtKeyD;
        case (uint32_t)'e': return VirtKeyE;
        case (uint32_t)'f': return VirtKeyF;
        case (uint32_t)'g': return VirtKeyG;
        case (uint32_t)'h': return VirtKeyH;
        case (uint32_t)'i': return VirtKeyI;
        case (uint32_t)'j': return VirtKeyJ;
        case (uint32_t)'k': return VirtKeyK;
        case (uint32_t)'l': return VirtKeyL;
        case (uint32_t)'m': return VirtKeyM;
        case (uint32_t)'n': return VirtKeyN;
        case (uint32_t)'o': return VirtKeyO;
        case (uint32_t)'p': return VirtKeyP;
        case (uint32_t)'q': return VirtKeyQ;
        case (uint32_t)'r': return VirtKeyR;
        case (uint32_t)'s': return VirtKeyS;
        case (uint32_t)'t': return VirtKeyT;
        case (uint32_t)'u': return VirtKeyU;
        case (uint32_t)'v': return VirtKeyV;
        case (uint32_t)'w': return VirtKeyW;
        case (uint32_t)'x': return VirtKeyX;
        case (uint32_t)'y': return VirtKeyY;
        case (uint32_t)'z': return VirtKeyZ;
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

        if (child->mnemonic_index >= 0) {
            int start_x, end_x;
            int start_y, end_y;
            child->label.pos_at_cursor(child->mnemonic_index, start_x, start_y);
            child->label.pos_at_cursor(child->mnemonic_index + 1, end_x, end_y);
            child->mnemonic_model = glm::scale(
                            glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(label_left + start_x, label_top + start_y + 1, 0.0f)),
                            glm::vec3(end_x - start_x, 1.0f, 1.0f));
        }

        if (!null_key_sequence(child->shortcut)) {
            int shortcut_label_left = left + calculated_width - padding_right - child->shortcut_label.width();
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

        if (child->mnemonic_index >= 0) {
            glm::mat4 mnemonic_mvp = projection * child->mnemonic_model;
            gui_window->fill_rect(this_text_color, mnemonic_mvp);
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

void MenuWidgetItem::activate() {
    if (!activate_handler) {
        fprintf(stderr, "No handler attached: %s\n", label.text().encode().raw());
        return;
    }
    GuiWindow *gui_window_ref = gui_window;
    activate_handler(userdata);
    gui_window_ref->destroy_context_menu();
}

void ContextMenuWidget::on_mouse_move(const MouseEvent *event) {
    MenuWidgetItem *hover_item = get_item_at(event->y);
    switch (event->action) {
        case MouseActionMove:
            activated_item = hover_item;
            break;
        case MouseActionUp:
            if (hover_item)
                hover_item->activate();
            else
                gui_window->destroy_context_menu();
            break;
        default:
            return;
    }
}

int ContextMenuWidget::get_menu_widget_index(MenuWidgetItem *item) {
    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        if (child == item)
            return i;
    }
    panic("item not found");
}

bool ContextMenuWidget::on_key_event(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    if (event->modifiers != 0)
        return false;

    if (event->virt_key == VirtKeyUp || event->virt_key == VirtKeyDown) {
        int dir = (event->virt_key == VirtKeyUp) ? -1 : 1;
        int new_index;
        if (activated_item) {
            int activated_index = get_menu_widget_index(activated_item);
            new_index = (activated_index + dir) % menu_widget_item->children.length();
        } else {
            new_index = (dir == 1) ? 0 : (menu_widget_item->children.length() - 1);
        }
        activated_item = menu_widget_item->children.at(new_index);
        return true;
    }

    if (event->virt_key == VirtKeyLeft || event->virt_key == VirtKeyRight) {
        int dir = (event->virt_key == VirtKeyLeft) ? -1 : 1;
        MenuWidget *menu_widget = gui_window->menu_widget;
        int current_index = menu_widget->get_item_index(menu_widget->activated_item);
        int new_index = (current_index + dir) % menu_widget->children.length();
        menu_widget->pop_top_level(&menu_widget->children.at(new_index), true);
        return true;
    }

    if (activated_item && (event->virt_key == VirtKeyEnter || event->virt_key == VirtKeySpace)) {
        activated_item->activate();
        return true;
    }

    if (event->virt_key == VirtKeyEscape) {
        gui_window->destroy_context_menu();
        return true;
    }

    for (int i = 0; i < menu_widget_item->children.length(); i += 1) {
        MenuWidgetItem *child = menu_widget_item->children.at(i);
        VirtKey target_key = child->get_mnemonic_key();
        if (target_key == event->virt_key) {
            child->activate();
            return true;
        }
    }

    return false;
}

void ContextMenuWidget::on_mouse_out(const MouseEvent *event) {
    activated_item = nullptr;
}

MenuWidget::MenuWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    bg_color(parse_color("#CECECE")),
    text_color(parse_color("#353535")),
    activated_color(parse_color("#2B71BC")),
    activated_text_color(parse_color("#f0f0f0")),
    spacing_left(12),
    spacing_right(12),
    spacing_top(4),
    spacing_bottom(4),
    activated_item(nullptr),
    down_unclick(false)
{
    gui_window->menu_widget = this;
}

MenuWidget::~MenuWidget() {
    gui_window->menu_widget = nullptr;

    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        destroy(child->item, 1);
    }
}

MenuWidgetItem *MenuWidget::add_menu(String name, int mnemonic_index) {
    if (children.resize(children.length() + 1))
        panic("out of memory");

    TopLevelMenu *child = &children.at(children.length() - 1);
    child->item = create<MenuWidgetItem>(gui_window, name, mnemonic_index, no_shortcut());
    update_model();
    on_size_hints_changed();
    return child->item;
}

void MenuWidget::draw(const glm::mat4 &projection) {
    // background
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        glm::mat4 label_mvp = projection * child->item->label_model;
        glm::vec4 this_text_color = (activated_item == child) ? activated_text_color : text_color;
        if (activated_item == child) {
            gui_window->fill_rect(activated_color,
                    left + child->left, top,
                    child->right - child->left, calculated_height);
        }
        child->item->label.draw(gui_window, label_mvp, this_text_color);
        if (child->item->mnemonic_index >= 0) {
            glm::mat4 mnemonic_mvp = projection * child->item->mnemonic_model;
            gui_window->fill_rect(this_text_color, mnemonic_mvp);
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

        if (child->item->mnemonic_index >= 0) {
            int start_x, end_x;
            int start_y, end_y;
            child->item->label.pos_at_cursor(child->item->mnemonic_index, start_x, start_y);
            child->item->label.pos_at_cursor(child->item->mnemonic_index + 1, end_x, end_y);
            child->item->mnemonic_model = glm::scale(
                            glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(label_left + start_x, label_top + start_y + 1, 0.0f)),
                            glm::vec3(end_x - start_x, 1.0f, 1.0f));
        }
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

void MenuWidget::pop_top_level(TopLevelMenu *child, bool select_first_item) {
    ContextMenuWidget *context_menu = gui_window->pop_context_menu(child->item,
            left + child->left, top,
            child->right - child->left, calculated_height);
    context_menu->userdata = this;
    context_menu->on_destroy = on_context_menu_destroy;
    activated_item = child;

    if (select_first_item)
        context_menu->activated_item = context_menu->menu_widget_item->children.at(0);
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
                down_unclick = (activated_item && activated_item == child);
                if (!child)
                    return;
                pop_top_level(child, false);
                break;
            }
        case MouseActionMove:
            {
                if (!activated_item)
                    return;
                TopLevelMenu *child = get_child_at(event->x, event->y);
                if (child) {
                    if (child != activated_item)
                        pop_top_level(child, false);
                } else if (gui_window->context_menu) {
                    MouseEvent event_for_child = *event;
                    event_for_child.x += left - gui_window->context_menu->left;
                    event_for_child.y += top - gui_window->context_menu->top;
                    gui_window->context_menu->on_mouse_move(&event_for_child);
                }
                break;
            }
        case MouseActionUp:
            {
                if (!activated_item)
                    return;
                TopLevelMenu *child = get_child_at(event->x, event->y);
                if (child) {
                    if (down_unclick && child == activated_item)
                        gui_window->destroy_context_menu();
                    return;
                }
                if (gui_window->context_menu) {
                    MouseEvent event_for_child = *event;
                    event_for_child.x += left - gui_window->context_menu->left;
                    event_for_child.y += top - gui_window->context_menu->top;
                    gui_window->context_menu->on_mouse_move(&event_for_child);
                }
                break;
            }
        default:
            return;
    }
}

bool MenuWidget::dispatch_shortcut(MenuWidgetItem *parent, const KeyEvent *event) {
    for (int i = 0; i < parent->children.length(); i += 1) {
        MenuWidgetItem *child = parent->children.at(i);
        if (child->children.length() > 0) {
            if (dispatch_shortcut(child, event))
                return true;
        } else if (key_sequence_match(child->shortcut, event)) {
            child->activate();
            return true;
        }
    }
    return false;
}

bool MenuWidget::on_key_event(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    for (int i = 0; i < children.length(); i += 1) {
        MenuWidgetItem *top_level_child = children.at(i).item;
        if (dispatch_shortcut(top_level_child, event))
            return true;
    }

    if (key_mod_only_alt(event->modifiers)) {
        for (int i = 0; i < children.length(); i += 1) {
            TopLevelMenu *child = &children.at(i);
            VirtKey target_key = child->item->get_mnemonic_key();
            if (target_key == event->virt_key) {
                pop_top_level(child, true);
                return true;
            }
        }
    }

    return false;
}

int MenuWidget::get_item_index(TopLevelMenu *item) {
    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        if (child == item)
            return i;
    }
    panic("item not found");
}


#ifndef GENESIS_SELECT_WIDGET_HPP
#define GENESIS_SELECT_WIDGET_HPP

#include "string.hpp"
#include "label.hpp"
#include "widget.hpp"
#include "sunken_box.hpp"

class SpritesheetImage;
class MenuWidgetItem;
class SelectWidget;

struct SelectWidgetItem {
    SelectWidget *parent;
    MenuWidgetItem *menu_item;
    String name;
    int index;
};

class SelectWidget : public Widget {
public:
    SelectWidget(GuiWindow *gui_window);
    ~SelectWidget() override;

    void draw(const glm::mat4 &projection) override;

    void on_mouse_move(const MouseEvent *event) override;
    void on_mouse_out(const MouseEvent *event) override;
    void on_mouse_over(const MouseEvent *event) override;
    void on_resize() override { update_model(); }

    int min_width() const override;
    int min_height() const override;
    int max_height() const override;

    void clear();
    void append_choice(const String &choice);
    void select_index(int index);

    int selected_index;

    void (*on_selected_index_change)(SelectWidget *);

    Label label;
    glm::mat4 label_model;
    SunkenBox bg;
    MenuWidgetItem *context_menu;
    void *userdata;

    int padding_left;
    int padding_right;
    int padding_top;
    int padding_bottom;
    glm::vec4 text_color;

    const SpritesheetImage *arrow_icon_img;
    glm::mat4 arrow_icon_model;

    List<SelectWidgetItem> items;

    bool hovering;
    bool context_menu_open;

    void update_model();
    void set_activate_handlers();
    void clear_context_menu();
};

#endif

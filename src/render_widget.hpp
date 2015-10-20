#ifndef GENESIS_RENDER_WIDGET_HPP
#define GENESIS_RENDER_WIDGET_HPP

#include "widget.hpp"
#include "grid_layout_widget.hpp"

struct Project;
class TextWidget;
class SelectWidget;
class ButtonWidget;
class SettingsFile;

class RenderWidget : public Widget {
public:
    RenderWidget(GuiWindow *gui_window, Project *project, SettingsFile *settings_file);
    ~RenderWidget() override;
    void draw(const glm::mat4 &projection) override;
    void on_resize() override;

    void on_mouse_move(const MouseEvent *) override;


    Project *project;
    SettingsFile *settings_file;
    GridLayoutWidget layout;

    TextWidget *create_form_label(const char *text);

    SelectWidget *output_format_select;
    SelectWidget *sample_format_select;
    SelectWidget *bit_rate_select;
    ButtonWidget *render_button;
    TextWidget *bit_rate_label;
    TextWidget *output_file_text;
    void select_settings_output_format();
    void select_settings_sample_format();
    void select_settings_bit_rate();

    void my_on_selected_output_format_change();
    void my_on_selected_sample_format_change();
    void my_on_selected_bit_rate_change();
    void my_on_render_activate();
};

#endif

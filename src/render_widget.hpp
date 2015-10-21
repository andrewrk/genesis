#ifndef GENESIS_RENDER_WIDGET_HPP
#define GENESIS_RENDER_WIDGET_HPP

#include "widget.hpp"
#include "grid_layout_widget.hpp"
#include "spacer_widget.hpp"

struct Project;
class TextWidget;
class SelectWidget;
class ButtonWidget;
struct SettingsFile;
struct RenderJob;
class RenderWidget;

struct RenderWidgetJob {
    RenderWidget *parent;
    TextWidget *done_text;
    ButtonWidget *stop_btn;
    RenderJob *render_job;
};

class RenderWidget : public Widget {
public:
    RenderWidget(GuiWindow *gui_window, Project *project, SettingsFile *settings_file);
    ~RenderWidget() override;
    void draw(const glm::mat4 &projection) override;
    void on_resize() override;

    void on_mouse_move(const MouseEvent *) override;


    Project *project;
    SettingsFile *settings_file;
    GridLayoutWidget main_layout;
    GridLayoutWidget props_layout;
    GridLayoutWidget jobs_layout;
    SpacerWidget spacer_widget;

    TextWidget *create_form_label(const char *text);

    List<RenderWidgetJob> job_list;

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
    void refresh_render_jobs();

    void init_render_widget_job(RenderWidgetJob *rwj);
    void deinit_render_widget_job(RenderWidgetJob *rwj);
};

#endif

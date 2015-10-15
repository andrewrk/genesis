#include "render_widget.hpp"
#include "project.hpp"
#include "text_widget.hpp"
#include "select_widget.hpp"
#include "spacer_widget.hpp"
#include "button_widget.hpp"
#include "settings_file.hpp"

static void on_selected_output_format_change(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->my_on_selected_output_format_change();
}

static void on_selected_sample_format_change(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->my_on_selected_sample_format_change();
}

static void on_selected_bit_rate_change(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->my_on_selected_bit_rate_change();
}

static void on_render_activate(Event, void *userdata) {
    panic("TODO");
}

static void on_settings_output_format_changed(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->select_settings_output_format();
}

static void on_settings_sample_format_changed(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->select_settings_sample_format();
}

static void on_settings_bit_rate_changed(Event, void *userdata) {
    RenderWidget *render_widget = (RenderWidget*)userdata;
    render_widget->select_settings_bit_rate();
}

RenderWidget::~RenderWidget() {
    output_format_select->events.detach_handler(EventSelectedIndexChanged, on_selected_output_format_change);
    sample_format_select->events.detach_handler(EventSelectedIndexChanged, on_selected_sample_format_change);
    bit_rate_select->events.detach_handler(EventSelectedIndexChanged, on_selected_bit_rate_change);
    render_button->events.detach_handler(EventActivate, on_render_activate);
    settings_file->events.detach_handler(EventSettingsDefaultRenderFormatChanged,
            on_settings_output_format_changed);
    settings_file->events.detach_handler(EventSettingsDefaultRenderSampleFormatChanged,
            on_settings_sample_format_changed);
    settings_file->events.detach_handler(EventSettingsDefaultRenderBitRateChanged,
            on_settings_bit_rate_changed);
}

RenderWidget::RenderWidget(GuiWindow *gui_window, Project *project, SettingsFile *settings_file) :
    Widget(gui_window),
    project(project),
    settings_file(settings_file),
    layout(gui_window)
{
    output_format_select = create<SelectWidget>(gui_window);
    int format_count = genesis_out_format_count(project->genesis_context);
    for (int i = 0; i < format_count; i += 1) {
        GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context, i);
        const char *desc = genesis_render_format_description(render_format);
        output_format_select->append_choice(desc);
    }
    output_format_select->events.attach_handler(EventSelectedIndexChanged, on_selected_output_format_change, this);
    layout.add_widget(create_form_label("Output format:"), 0, 0, HAlignRight, VAlignCenter);
    layout.add_widget(output_format_select, 0, 1, HAlignLeft, VAlignCenter);

    TextWidget *output_file_text = create<TextWidget>(gui_window);
    output_file_text->set_text("out.flac");
    layout.add_widget(create_form_label("Output file:"), 1, 0, HAlignRight, VAlignCenter);
    layout.add_widget(output_file_text, 1, 1, HAlignLeft, VAlignCenter);

    sample_format_select = create<SelectWidget>(gui_window);
    sample_format_select->events.attach_handler(EventSelectedIndexChanged,
            on_selected_sample_format_change, this);
    layout.add_widget(create_form_label("Sample format:"), 2, 0, HAlignRight, VAlignCenter);
    layout.add_widget(sample_format_select, 2, 1, HAlignLeft, VAlignCenter);

    bit_rate_select = create<SelectWidget>(gui_window);
    bit_rate_select->events.attach_handler(EventSelectedIndexChanged, on_selected_bit_rate_change, this);
    bit_rate_label = create_form_label("Bit rate:");
    layout.add_widget(bit_rate_label, 3, 0, HAlignRight, VAlignCenter);
    layout.add_widget(bit_rate_select, 3, 1, HAlignLeft, VAlignCenter);

    render_button = create<ButtonWidget>(gui_window);
    render_button->set_text("Render");
    render_button->events.attach_handler(EventActivate, on_render_activate, this);
    layout.add_widget(render_button, 4, 1, HAlignLeft, VAlignTop);

    SpacerWidget *spacer_widget = create<SpacerWidget>(gui_window, false, true);
    layout.add_widget(spacer_widget, 5, 0, HAlignCenter, VAlignCenter);

    settings_file->events.attach_handler(EventSettingsDefaultRenderFormatChanged,
            on_settings_output_format_changed, this);
    settings_file->events.attach_handler(EventSettingsDefaultRenderSampleFormatChanged,
            on_settings_sample_format_changed, this);
    settings_file->events.attach_handler(EventSettingsDefaultRenderBitRateChanged,
            on_settings_bit_rate_changed, this);
    select_settings_output_format();
}

TextWidget *RenderWidget::create_form_label(const char *text) {
    TextWidget *text_widget = create<TextWidget>(gui_window);

    text_widget->set_background_on(false);
    text_widget->set_text(text);
    text_widget->set_auto_size(true);
    return text_widget;
}


void RenderWidget::draw(const glm::mat4 &projection) {
    layout.draw(projection);
}

void RenderWidget::on_resize() {
    layout.left = left;
    layout.top = top;
    layout.width = width;
    layout.height = height;
    layout.on_resize();
}

void RenderWidget::on_mouse_move(const MouseEvent *ev) {
    forward_mouse_event(&layout, ev);
}

void RenderWidget::select_settings_output_format() {
    output_format_select->select_index(0);
    int out_format_count = genesis_out_format_count(project->genesis_context);
    for (int i = 0; i < out_format_count; i += 1) {
        GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context, i);
        if (render_format->render_format_type == settings_file->default_render_format) {
            output_format_select->select_index(i);
        }
    }

    select_settings_sample_format();
    select_settings_bit_rate();
}

void RenderWidget::select_settings_bit_rate() {
    GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context,
            output_format_select->selected_index);
    GenesisAudioFileCodec *codec = genesis_render_format_codec(render_format);

    int bit_rate_count = genesis_audio_file_codec_bit_rate_count(codec);
    if (bit_rate_count) {
        int sf_bit_rate = settings_file->default_render_bit_rates[render_format->render_format_type];

        int best_index = genesis_audio_file_codec_best_bit_rate(codec);
        bit_rate_select->clear();
        for (int i = 0; i < bit_rate_count; i += 1) {
            int bit_rate = genesis_audio_file_codec_bit_rate_index(codec, i);
            bit_rate_select->append_choice(ByteBuffer::format("%d kbps", bit_rate));
            if (bit_rate == sf_bit_rate)
                best_index = i;
        }
        bit_rate_select->select_index(best_index);
        bit_rate_select->is_visible = true;
        bit_rate_label->is_visible = true;
    } else {
        bit_rate_select->is_visible = false;
        bit_rate_label->is_visible = false;
    }
}

void RenderWidget::select_settings_sample_format() {
    GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context,
            output_format_select->selected_index);
    GenesisAudioFileCodec *codec = genesis_render_format_codec(render_format);
    SoundIoFormat sf_format = settings_file->default_render_sample_formats[render_format->render_format_type];

    int best_index = genesis_audio_file_codec_best_sample_format(codec);
    sample_format_select->clear();
    int sample_format_count = genesis_audio_file_codec_sample_format_count(codec);
    for (int i = 0; i < sample_format_count; i += 1) {
        SoundIoFormat format = genesis_audio_file_codec_sample_format_index(codec, i);
        sample_format_select->append_choice(soundio_format_string(format));
        if (format == sf_format)
            best_index = i;
    }

    sample_format_select->select_index(best_index);
}

void RenderWidget::my_on_selected_output_format_change() {
    GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context,
            output_format_select->selected_index);
    settings_file_set_default_render_format(settings_file, render_format->render_format_type);
    settings_file_commit(settings_file);
}

void RenderWidget::my_on_selected_sample_format_change() {
    GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context,
            output_format_select->selected_index);
    GenesisAudioFileCodec *codec = genesis_render_format_codec(render_format);

    SoundIoFormat format = genesis_audio_file_codec_sample_format_index(codec,
            sample_format_select->selected_index);

    settings_file_set_default_render_sample_format(settings_file, render_format->render_format_type, format);
    settings_file_commit(settings_file);
}

void RenderWidget::my_on_selected_bit_rate_change() {
    GenesisRenderFormat *render_format = genesis_out_format_index(project->genesis_context,
            output_format_select->selected_index);
    GenesisAudioFileCodec *codec = genesis_render_format_codec(render_format);

    int bit_rate = genesis_audio_file_codec_bit_rate_index(codec, bit_rate_select->selected_index);

    settings_file_set_default_render_bit_rate(settings_file, render_format->render_format_type, bit_rate);
    settings_file_commit(settings_file);
}

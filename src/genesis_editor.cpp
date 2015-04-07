#include "genesis_editor.hpp"
#include "list.hpp"
#include "gui_window.hpp"
#include "genesis.h"

GenesisEditor::GenesisEditor() :
    _resource_bundle("resources.bundle"),
    _find_file_widget(NULL)
{
    int err = genesis_create_context(&_genesis_context);
    if (err)
        panic("unable to create genesis context: %s", genesis_error_string(err));

    _gui = create<Gui>(_genesis_context, &_resource_bundle);

    _gui_window = _gui->create_window(true);
    _gui_window->_userdata = this;
    _gui_window->set_on_key_event(static_on_key_event);
    _gui_window->set_on_text_event(static_on_text_event);
    _gui_window->set_on_close_event(static_on_close_event);
}

GenesisEditor::~GenesisEditor() {}

void GenesisEditor::on_close_event(GuiWindow *window) {
    _gui->destroy_window(_gui_window);

    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::exec() {
    _gui->exec();
}

void GenesisEditor::destroy_find_file_widget() {
    if (!_find_file_widget)
        return;
    _gui_window->destroy_widget(_find_file_widget);
    _find_file_widget = NULL;
}

bool GenesisEditor::on_text_event(GuiWindow *window, const TextInputEvent *event) {
    return false;
}

bool GenesisEditor::on_key_event(GuiWindow *window, const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    switch (event->virt_key) {
        default:
            return false;
        case VirtKeyO:
            if (key_mod_only_ctrl(event->modifiers)) {
                show_open_file();
                return true;
            }
            return false;
        case VirtKeyS:
            if (key_mod_only_ctrl(event->modifiers)) {
                show_save_file();
                return true;
            }
            return false;
    }
    return false;
}

void GenesisEditor::ensure_find_file_widget() {
    if (_find_file_widget)
        return;

    _find_file_widget = _gui_window->create_find_file_widget();
    _find_file_widget->_userdata = this;
    _find_file_widget->set_pos(100, 100);
}

void GenesisEditor::show_save_file() {
    ensure_find_file_widget();
    _find_file_widget->set_mode(FindFileWidget::ModeSave);
    _gui_window->set_focus_widget(_find_file_widget);
    _find_file_widget->set_on_choose_file(on_choose_save_file);
}

void GenesisEditor::show_open_file() {
    ensure_find_file_widget();
    _find_file_widget->set_mode(FindFileWidget::ModeOpen);
    _find_file_widget->set_on_choose_file(static_on_choose_file);
    _gui_window->set_focus_widget(_find_file_widget);
}

void GenesisEditor::on_choose_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();
}

void GenesisEditor::on_choose_save_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();

    struct GenesisExportFormat export_format;
    export_format.bit_rate = 320 * 1000;
    export_format.codec = genesis_guess_audio_file_codec(_genesis_context, file_path.raw(), nullptr, nullptr);
    if (!export_format.codec)
        panic("can't find suitable format to save");
    int sample_format_count = genesis_audio_file_codec_sample_format_count(export_format.codec);
    if (sample_format_count <= 0)
        panic("unsupported sample format");
    export_format.sample_format = genesis_audio_file_codec_sample_format_index(export_format.codec, 0);
}

void GenesisEditor::edit_file(const char *filename) {
    on_choose_file(filename);
}

void GenesisEditor::create_new_file() {
}

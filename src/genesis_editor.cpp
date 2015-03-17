#include "genesis_editor.hpp"
#include "list.hpp"
#include "audio_edit_widget.hpp"
#include "gui_window.hpp"
#include "genesis.h"

GenesisEditor::GenesisEditor() :
    _resource_bundle("build/resources.bundle"),
    _gui(&_resource_bundle),
    _find_file_widget(NULL),
    _audio_edit_widget(NULL)
{
    _genesis_context = genesis_create_context();
    if (!_genesis_context)
        panic("unable to create genesis context");

    _gui_window = _gui.create_window(true);
    _gui_window->_userdata = this;
    _gui_window->set_on_key_event(static_on_key_event);
    _gui_window->set_on_text_event(static_on_text_event);
    _gui_window->set_on_close_event(static_on_close_event);
}

GenesisEditor::~GenesisEditor() {}

void GenesisEditor::on_close_event(GuiWindow *window) {
    _gui.destroy_window(_gui_window);

    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::exec() {
    _gui.exec();
}

void GenesisEditor::destroy_find_file_widget() {
    if (!_find_file_widget)
        return;
    _gui_window->destroy_widget(_find_file_widget);
    _find_file_widget = NULL;
}

void GenesisEditor::destroy_audio_edit_widget() {
    if (!_audio_edit_widget)
        return;
    _gui_window->destroy_widget(_audio_edit_widget);
    _audio_edit_widget = NULL;
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
    destroy_audio_edit_widget();
    ensure_find_file_widget();
    _find_file_widget->set_mode(FindFileWidget::ModeOpen);
    _find_file_widget->set_on_choose_file(static_on_choose_file);
    _gui_window->set_focus_widget(_find_file_widget);
}

void GenesisEditor::on_choose_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();

    destroy_audio_edit_widget();
    _audio_edit_widget = _gui_window->create_audio_edit_widget();
    _audio_edit_widget->set_pos(10, 10);
    _audio_edit_widget->set_size(_gui_window->_width - 20, _gui_window->_height - 20);
    _audio_edit_widget->load_file(file_path);
    _gui_window->set_focus_widget(_audio_edit_widget);
}

void GenesisEditor::on_choose_save_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();

    List<ExportSampleFormat> sample_formats;
    audio_file_get_supported_sample_formats(NULL, NULL, file_path.raw(), sample_formats);

    if (sample_formats.length() == 0)
        panic("can't find suitable format to save");

    _audio_edit_widget->save_as(file_path, sample_formats.at(0));

}

void GenesisEditor::edit_file(const char *filename) {
    on_choose_file(filename);
}

void GenesisEditor::create_new_file() {
    destroy_audio_edit_widget();

    _audio_edit_widget = _gui_window->create_audio_edit_widget();
    _audio_edit_widget->set_pos(10, 10);
    _audio_edit_widget->set_size(_gui_window->_width - 20, _gui_window->_height - 20);
    _gui_window->set_focus_widget(_audio_edit_widget);
}

#include "genesis_editor.hpp"
#include "list.hpp"
#include "audio_edit_widget.hpp"

GenesisEditor::GenesisEditor() :
    _resource_bundle("build/resources.bundle"),
    _find_file_widget(NULL),
    _audio_edit_widget(NULL)
{
    _window = SDL_CreateWindow("genesis",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1366, 768, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!_window)
        panic("unable to create SDL window");

    _context = SDL_GL_CreateContext(_window);

    if (!_context)
        panic("unable to create gl context: %s", SDL_GetError());

    _shader_program_manager = create<ShaderProgramManager>();

    _gui = create<Gui>(_window, &_resource_bundle, _shader_program_manager, &_audio_hardware);
    _gui->_userdata = this;
    _gui->set_on_key_event(static_on_gui_key);

}

GenesisEditor::~GenesisEditor() {
    destroy_find_file_widget();
    destroy_audio_edit_widget();
    destroy(_gui, 1);
    destroy(_shader_program_manager, 1);
    SDL_GL_DeleteContext(_context);
    SDL_DestroyWindow(_window);
}

void GenesisEditor::exec() {
    _gui->exec();
}

void GenesisEditor::destroy_find_file_widget() {
    if (!_find_file_widget)
        return;
    _gui->destroy_widget(&_find_file_widget->_widget);
    _find_file_widget = NULL;
}

void GenesisEditor::destroy_audio_edit_widget() {
    if (!_audio_edit_widget)
        return;
    _gui->destroy_widget(&_audio_edit_widget->_widget);
    _audio_edit_widget = NULL;
}

bool GenesisEditor::on_gui_key(Gui *gui, const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    switch (event->virt_key) {
        default:
            return false;
        case VirtKeyO:
            if (!event->ctrl())
                return false;
            show_open_file();
            return true;
        case VirtKeyS:
            if (!event->ctrl())
                return false;
            show_save_file();
            return true;
    }
    return false;
}

void GenesisEditor::ensure_find_file_widget() {
    if (_find_file_widget)
        return;

    _find_file_widget = _gui->create_find_file_widget();
    _find_file_widget->_userdata = this;
    _find_file_widget->set_pos(100, 100);
}

void GenesisEditor::show_save_file() {
    ensure_find_file_widget();
    _find_file_widget->set_mode(FindFileWidget::ModeSave);
    _gui->set_focus_widget(&_find_file_widget->_widget);
    _find_file_widget->set_on_choose_file(on_choose_save_file);
}

void GenesisEditor::show_open_file() {
    destroy_audio_edit_widget();
    ensure_find_file_widget();
    _find_file_widget->set_mode(FindFileWidget::ModeOpen);
    _find_file_widget->set_on_choose_file(on_choose_file);
    _gui->set_focus_widget(&_find_file_widget->_widget);
}

void GenesisEditor::on_choose_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();

    destroy_audio_edit_widget();
    _audio_edit_widget = _gui->create_audio_edit_widget();
    _audio_edit_widget->set_pos(10, 10);
    _audio_edit_widget->set_size(_gui->_width - 20, _gui->_height - 20);
    _audio_edit_widget->load_file(file_path);
    _gui->set_focus_widget(&_audio_edit_widget->_widget);
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

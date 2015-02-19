#include "genesis_editor.hpp"
#include "list.hpp"

#include <groove/groove.h>

struct Channel {
    int sample_rate;
    List<double> samples;
};

struct AudioFile {
    List<Channel> channels;
};

GenesisEditor::GenesisEditor() :
    _resource_bundle("build/resources.bundle"),
    _find_file_widget(NULL)
{
    _window = SDL_CreateWindow("genesis",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1366, 768, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!_window)
        panic("unable to create SDL window");

    _context = SDL_GL_CreateContext(_window);

    _gui = create<Gui>(_window, &_resource_bundle);
    _gui->_userdata = this;
    _gui->set_on_key_event(static_on_gui_key);

}

GenesisEditor::~GenesisEditor() {
    destroy_find_file_widget();
    destroy(_gui, 1);
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

bool GenesisEditor::on_gui_key(Gui *gui, const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    if (event->virt_key == VirtKeyO && event->ctrl()) {
        if (!_find_file_widget) {
            _find_file_widget = _gui->create_find_file_widget();
            _find_file_widget->set_mode(FindFileWidget::ModeOpen);
            _find_file_widget->set_pos(100, 100);
            _find_file_widget->_userdata = this;
            _find_file_widget->set_on_choose_file(on_choose_file);
        }
        _gui->set_focus_widget(&_find_file_widget->_widget);
        return true;
    }
    return false;
}

void GenesisEditor::on_choose_file(const ByteBuffer &file_path) {
    destroy_find_file_widget();

    GrooveFile *file = groove_file_open(const_cast<char *>(file_path.raw()));
    if (!file)
        panic("groove_file_open error");

    GrooveAudioFormat audio_format;
    groove_file_audio_format(file, &audio_format);

    int channel_count = groove_channel_layout_count(audio_format.channel_layout);

    AudioFile audio_file;
    audio_file.channels.resize(channel_count);
    for (int i = 0; i < audio_file.channels.length(); i += 1) {
        audio_file.channels.at(i).sample_rate = audio_format.sample_rate;
    }

    GroovePlaylist *playlist = groove_playlist_create();
    GrooveSink *sink = groove_sink_create();
    if (!playlist || !sink)
        panic("out of memory");

    sink->audio_format = audio_format;
    sink->disable_resample = 1;

    int err = groove_sink_attach(sink, playlist);
    if (err)
        panic("error attaching sink");

    GroovePlaylistItem *item = groove_playlist_insert(
            playlist, file, 1.0, 1.0, NULL);
    if (!item)
        panic("out of memory");

    fprintf(stderr, "waiting on buffer\n");
    GrooveBuffer *buffer;
    while (groove_sink_buffer_get(sink, &buffer, 1)) {
        fprintf(stderr, "got buffer\n");
        if (buffer->format.sample_rate != audio_format.sample_rate) {
            panic("non-consistent sample rate: %d -> %d",
                    audio_format.sample_rate, buffer->format.sample_rate );
        }
        switch (buffer->format.sample_fmt) {
            default:
                panic("unrecognized sample format");
                break;
            case GROOVE_SAMPLE_FMT_U8:          /* unsigned 8 bits */
                panic("unsupported sample format: u8");
                break;
            case GROOVE_SAMPLE_FMT_S16:         /* signed 16 bits */
                panic("unsupported sample format: s16");
                break;
            case GROOVE_SAMPLE_FMT_S32:         /* signed 32 bits */
                panic("unsupported sample format: s32");
                break;
            case GROOVE_SAMPLE_FMT_FLT:         /* float (32 bits) */
                panic("unsupported sample format: float");
                break;
            case GROOVE_SAMPLE_FMT_DBL:         /* double (64 bits) */
                panic("unsupported sample format: double");
                break;

            case GROOVE_SAMPLE_FMT_U8P:         /* unsigned 8 bits, planar */
                panic("unsupported sample format: u8 planar");
                break;
            case GROOVE_SAMPLE_FMT_S16P:        /* signed 16 bits, planar */
                panic("unsupported sample format: s16 planar");
                break;
            case GROOVE_SAMPLE_FMT_S32P:        /* signed 32 bits, planar */
                panic("unsupported sample format: s32 planar");
                break;
            case GROOVE_SAMPLE_FMT_FLTP:        /* float (32 bits), planar */
                panic("unsupported sample format: float planar");
                break;
            case GROOVE_SAMPLE_FMT_DBLP:         /* double (64 bits), planar */
                panic("unsupported sample format: double planar");
                break;
        }
    }

    groove_sink_detach(sink);
    groove_playlist_clear(playlist);
    groove_playlist_destroy(playlist);
    groove_file_close(file);
}

#ifndef AUDIO_EDIT_WIDGET
#define AUDIO_EDIT_WIDGET

#include "widget.hpp"
#include "byte_buffer.hpp"
#include "channel_layouts.hpp"

struct Channel {
    int sample_rate;
    List<double> samples;
};

struct AudioFile {
    List<Channel> channels;
    const ChannelLayout *channel_layout;
};

class Gui;
class AudioEditWidget {
public:
    Widget _widget;

    AudioEditWidget(Gui *gui);
    AudioEditWidget(const AudioEditWidget &copy) = delete;
    AudioEditWidget &operator=(const AudioEditWidget &copy) = delete;

    ~AudioEditWidget();
    void draw(const glm::mat4 &projection);
    int left() const { return _left; }
    int top() const { return _top; }
    int width() const { return _width; }
    int height() const { return _height; }
    void on_mouse_move(const MouseEvent *event);
    void on_mouse_out(const MouseEvent *event);
    void on_mouse_over(const MouseEvent *event);
    void on_gain_focus();
    void on_lose_focus();
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);


    void load_file(const ByteBuffer &file_path);
    void edit_audio_file(AudioFile *audio_file);
    void set_pos(int left, int top);
    void set_size(int width, int height);

private:

    int _left;
    int _top;
    int _width;
    int _height;

    AudioFile *_audio_file;

    void update_model();

    void destroy_audio_file();
    void destroy_all_ui();


    // widget methods
    static void destructor(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->~AudioEditWidget();
    }
    static void draw(Widget *widget, const glm::mat4 &projection) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->draw(projection);
    }
    static int left(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->left();
    }
    static int top(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->top();
    }
    static int width(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->width();
    }
    static int height(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->height();
    }
    static void on_mouse_move(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_move(event);
    }
    static void on_mouse_out(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_out(event);
    }
    static void on_mouse_over(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_over(event);
    }
    static void on_gain_focus(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_gain_focus();
    }
    static void on_lose_focus(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_lose_focus();
    }
    static void on_text_input(Widget *widget, const TextInputEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_text_input(event);
    }
    static void on_key_event(Widget *widget, const KeyEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_key_event(event);
    }
};

#endif

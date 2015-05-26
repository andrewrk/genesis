#include "audio_graph.hpp"
#include "mixer_node.hpp"

static const int AUDIO_CLIP_POLYPHONY = 32;

static_assert(sizeof(long) == 8, "require long to be 8 bytes");

struct AudioClipNodeChannel {
    struct GenesisAudioFileIterator iter;
    long offset;
};

struct AudioClipVoice {
    bool active;
    AudioClipNodeChannel channels[GENESIS_MAX_CHANNELS];
    int frames_until_start;
    long frame_index;
    long frame_end;
};

struct AudioClipNodeContext {
    AudioClip *audio_clip;
    GenesisAudioFile *audio_file;
    double pos;

    AudioClipVoice voices[AUDIO_CLIP_POLYPHONY];
    int next_note_index;
};

static long whole_notes_as_long(double whole_notes) {
    return (whole_notes * 4294967296.0);
}

static double whole_notes_as_double(long whole_notes) {
    return whole_notes / 4294967296.0;
}

static AudioClipVoice *find_next_voice(AudioClipNodeContext *context) {
    for (int i = 0;; i += 1) {
        AudioClipVoice *voice = &context->voices[context->next_note_index];
        context->next_note_index = (context->next_note_index + 1) % AUDIO_CLIP_POLYPHONY;
        if (!voice->active || i == AUDIO_CLIP_POLYPHONY)
            return voice;
    }
}

static void audio_clip_node_destroy(struct GenesisNode *node) {
    AudioClipNodeContext *audio_clip_context = (AudioClipNodeContext*)node->userdata;
    destroy(audio_clip_context, 1);
}

static int audio_clip_node_create(struct GenesisNode *node) {
    const GenesisNodeDescriptor *node_descr = genesis_node_descriptor(node);
    AudioClipNodeContext *audio_clip_context = create_zero<AudioClipNodeContext>();
    audio_clip_context->audio_clip = (AudioClip*)genesis_node_descriptor_userdata(node_descr);
    audio_clip_context->audio_file = audio_clip_context->audio_clip->audio_asset->audio_file;
    node->userdata = audio_clip_context;
    if (!node->userdata) {
        audio_clip_node_destroy(node);
        return GenesisErrorNoMem;
    }
    return 0;
}

static void audio_clip_node_seek(struct GenesisNode *node) {
    struct AudioClipNodeContext *context = (struct AudioClipNodeContext*)node->userdata;
    context->pos = node->timestamp;
    // TODO reset some state
}

static void audio_clip_node_run(struct GenesisNode *node) {
    struct AudioClipNodeContext *context = (struct AudioClipNodeContext*)node->userdata;
    struct GenesisContext *g_context = genesis_node_context(node);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);
    struct GenesisPort *events_in_port = genesis_node_port(node, 1);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct GenesisChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    int frame_rate = genesis_audio_port_sample_rate(audio_out_port);
    int channel_count = channel_layout->channel_count;
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    int event_count = genesis_events_in_port_fill_count(events_in_port);
    GenesisMidiEvent *event = genesis_events_in_port_read_ptr(events_in_port);
    int event_index;
    for (event_index = 0; event_index < event_count; event_index += 1, event += 1) {
        double whole_notes_until_start = max(0.0, event->start - context->pos);
        int frames_until_start = genesis_whole_notes_to_frames(g_context, whole_notes_until_start, frame_rate);
        if (frames_until_start >= output_frame_count)
            break;
        if (event->event_type == GenesisMidiEventTypeSegment) {
            AudioClipVoice *voice = find_next_voice(context);

            voice->active = true;
            voice->frames_until_start = frames_until_start;
            voice->frame_index = event->data.segment_data.start;
            voice->frame_end = event->data.segment_data.end;
            for (int ch = 0; ch < channel_count; ch += 1) {
                struct AudioClipNodeChannel *channel = &voice->channels[ch];
                channel->iter = genesis_audio_file_iterator(context->audio_file, ch, 0);
                channel->offset = 0;
            }
        }
    }
    genesis_events_in_port_advance_read_ptr(events_in_port, event_index);

    // set everything to silence and then we'll add samples in
    memset(out_buf, 0, output_frame_count * bytes_per_frame);

    for (int voice_i = 0; voice_i < GENESIS_NOTES_COUNT; voice_i += 1) {
        AudioClipVoice *voice = &context->voices[voice_i];
        if (!voice->active)
            continue;

        int out_frame_count = min(output_frame_count, output_frame_count - voice->frames_until_start);
        int audio_file_frames_left = voice->frame_end - voice->frame_index;
        int frames_to_advance = min(out_frame_count, audio_file_frames_left);
        for (int ch = 0; ch < channel_count; ch += 1) {
            struct AudioClipNodeChannel *channel = &voice->channels[ch];
            for (int frame_offset = 0; frame_offset < frames_to_advance; frame_offset += 1) {
                if (channel->offset >= channel->iter.end) {
                    genesis_audio_file_iterator_next(&channel->iter);
                    channel->offset = 0;
                }

                int out_frame_index = voice->frames_until_start + frame_offset;
                int out_sample_index = out_frame_index * channel_count + ch;
                out_buf[out_sample_index] += channel->iter.ptr[channel->offset];
                channel->offset += 1;
            }
        }
        voice->frame_index += frames_to_advance;
        voice->frames_until_start = 0;
        if (frames_to_advance == audio_file_frames_left)
            voice->active = false;
    }

    context->pos += genesis_frames_to_whole_notes(g_context, output_frame_count, frame_rate);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_frame_count);
}

static void spy_node_run(struct GenesisNode *node) {
    const struct GenesisNodeDescriptor *node_descriptor = genesis_node_descriptor(node);
    struct Project *project = (struct Project *)genesis_node_descriptor_userdata(node_descriptor);
    struct GenesisContext *context = project->genesis_context;
    struct GenesisPort *audio_in_port = genesis_node_port(node, 0);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 1);

    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_in_port);

    int frame_count = min(input_frame_count, output_frame_count);

    memcpy(genesis_audio_out_port_write_ptr(audio_out_port),
           genesis_audio_in_port_read_ptr(audio_in_port),
           frame_count * bytes_per_frame);

    genesis_audio_out_port_advance_write_ptr(audio_out_port, frame_count);
    genesis_audio_in_port_advance_read_ptr(audio_in_port, frame_count);

    if (project->is_playing) {
        int frame_rate = genesis_audio_port_sample_rate(audio_out_port);
        double whole_notes = genesis_frames_to_whole_notes(context, frame_count, frame_rate);
        project->play_head_pos += whole_notes_as_long(whole_notes);
        project->play_head_changed_flag.clear();
    }
}

static void audio_file_node_run(struct GenesisNode *node) {
    const struct GenesisNodeDescriptor *node_descriptor = genesis_node_descriptor(node);
    struct Project *project = (struct Project *)genesis_node_descriptor_userdata(node_descriptor);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct GenesisChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    int channel_count = channel_layout->channel_count;
    float *out_samples = genesis_audio_out_port_write_ptr(audio_out_port);

    int audio_file_frames_left = project->audio_file_frame_count - project->audio_file_frame_index;
    int frames_to_advance = min(output_frame_count, audio_file_frames_left);

    for (int ch = 0; ch < channel_count; ch += 1) {
        struct PlayChannelContext *channel_context = &project->audio_file_channel_context[ch];
        for (int frame_offset = 0; frame_offset < frames_to_advance; frame_offset += 1) {
            if (channel_context->offset >= channel_context->iter.end) {
                genesis_audio_file_iterator_next(&channel_context->iter);
                channel_context->offset = 0;
            }

            out_samples[channel_count * frame_offset + ch] = channel_context->iter.ptr[channel_context->offset];

            channel_context->offset += 1;
        }
    }
    int silent_frames = output_frame_count - frames_to_advance;
    memset(&out_samples[channel_count * frames_to_advance], 0, silent_frames * 4 * channel_count);

    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_frame_count);

    project->audio_file_frame_index += frames_to_advance;
}

static void stop_pipeline(Project *project) {
    genesis_stop_pipeline(project->genesis_context);
    genesis_node_disconnect_all_ports(project->playback_node);

    genesis_node_destroy(project->resample_node);
    project->resample_node = nullptr;

    genesis_node_destroy(project->mixer_node);
    project->mixer_node = nullptr;

    genesis_node_descriptor_destroy(project->mixer_descr);
    project->mixer_descr = nullptr;

    for (int i = 0; i < project->audio_clip_list.length(); i += 1) {
        AudioClip *audio_clip = project->audio_clip_list.at(i);
        genesis_node_disconnect_all_ports(audio_clip->node);
    }
}

static void rebuild_and_start_pipeline(Project *project) {
    int err;

    // one for each of the audio clips and one for the sample file preview node
    int mix_port_count = 1 + project->audio_clip_list.length();

    ok_or_panic(create_mixer_descriptor(project->genesis_context, mix_port_count, &project->mixer_descr));
    project->mixer_node = ok_mem(genesis_node_descriptor_create_node(project->mixer_descr));

    ok_or_panic(genesis_connect_audio_nodes(project->spy_node, project->playback_node));
    ok_or_panic(genesis_connect_audio_nodes(project->mixer_node, project->spy_node));

    int next_mixer_port = 1;
    for (int i = 0; i < project->audio_clip_list.length(); i += 1) {
        AudioClip *audio_clip = project->audio_clip_list.at(i);

        int audio_out_port_index = genesis_node_descriptor_find_port_index(audio_clip->node_descr, "audio_out");
        if (audio_out_port_index < 0)
            panic("port not found");

        GenesisPort *audio_out_port = genesis_node_port(audio_clip->node, audio_out_port_index);
        GenesisPort *audio_in_port = genesis_node_port(project->mixer_node, next_mixer_port++);

        ok_or_panic(genesis_connect_ports(audio_out_port, audio_in_port));
    }

    {
        int audio_out_port_index = genesis_node_descriptor_find_port_index(project->audio_file_descr, "audio_out");
        if (audio_out_port_index < 0)
            panic("port not found");

        GenesisPort *audio_out_port = genesis_node_port(project->audio_file_node, audio_out_port_index);
        GenesisPort *audio_in_port = genesis_node_port(project->mixer_node, next_mixer_port++);

        if ((err = genesis_connect_ports(audio_out_port, audio_in_port))) {
            if (err == GenesisErrorIncompatibleChannelLayouts || err == GenesisErrorIncompatibleSampleRates) {
                project->resample_node = ok_mem(genesis_node_descriptor_create_node(project->resample_descr));
                ok_or_panic(genesis_connect_audio_nodes(project->audio_file_node, project->resample_node));

                int audio_out_port_index = genesis_node_descriptor_find_port_index(
                        project->resample_descr, "audio_out");
                if (audio_out_port_index < 0)
                    panic("port not found");
                GenesisPort *audio_out_port = genesis_node_port(project->resample_node, audio_out_port_index);
                ok_or_panic(genesis_connect_ports(audio_out_port, audio_in_port));
            } else {
                ok_or_panic(err);
            }
        }
    }

    assert(next_mixer_port == mix_port_count + 1);
    if ((err = genesis_start_pipeline(project->genesis_context)))
        panic("unable to start pipeline: %s", genesis_error_string(err));
}

static void play_audio_file(Project *project, GenesisAudioFile *audio_file, bool is_asset) {
    const struct GenesisChannelLayout *channel_layout;
    int sample_rate;
    long audio_file_frame_count;
    if (audio_file) {
        channel_layout = genesis_audio_file_channel_layout(audio_file);
        sample_rate = genesis_audio_file_sample_rate(audio_file);
        audio_file_frame_count = genesis_audio_file_frame_count(audio_file);
    } else {
        channel_layout = genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
        sample_rate = 48000;
        audio_file_frame_count = 0;
    }

    stop_pipeline(project);

    if (project->audio_file && !project->preview_audio_file_is_asset) {
        genesis_audio_file_destroy(project->audio_file);
    }
    project->audio_file = audio_file;
    project->preview_audio_file_is_asset = is_asset;

    genesis_audio_port_descriptor_set_channel_layout(project->audio_file_port_descr, channel_layout, true, -1);
    genesis_audio_port_descriptor_set_sample_rate(project->audio_file_port_descr, sample_rate, true, -1);

    if (project->audio_file_node)
        genesis_node_destroy(project->audio_file_node);
    project->audio_file_node = genesis_node_descriptor_create_node(project->audio_file_descr);

    project->audio_file_frame_count = audio_file_frame_count;
    project->audio_file_frame_index = 0;
    if (project->audio_file) {
        for (int ch = 0; ch < channel_layout->channel_count; ch += 1) {
            struct PlayChannelContext *channel_context = &project->audio_file_channel_context[ch];
            channel_context->offset = 0;
            channel_context->iter = genesis_audio_file_iterator(project->audio_file, ch, 0);
        }
    }
    rebuild_and_start_pipeline(project);
}

int project_set_up_audio_graph(Project *project) {
    int err;

    project->play_head_pos = whole_notes_as_long(0.0);
    project->is_playing = false;

    project->resample_descr = genesis_node_descriptor_find(project->genesis_context, "resample");
    if (!project->resample_descr)
        panic("unable to find resampler");
    project->resample_node = nullptr;

    project->audio_file_descr = genesis_create_node_descriptor(project->genesis_context,
            1, "audio_file", "Audio file playback.");
    if (!project->audio_file_descr)
        panic("unable to create audio file node descriptor");
    genesis_node_descriptor_set_userdata(project->audio_file_descr, project);
    genesis_node_descriptor_set_run_callback(project->audio_file_descr, audio_file_node_run);
    project->audio_file_port_descr = genesis_node_descriptor_create_port(
            project->audio_file_descr, 0, GenesisPortTypeAudioOut, "audio_out");
    if (!project->audio_file_port_descr)
        panic("unable to create audio out port descriptor");
    project->audio_file_node = nullptr;


    project->spy_descr = genesis_create_node_descriptor(project->genesis_context,
            2, "spy", "Spy on the master channel.");
    if (!project->spy_descr)
        panic("unable to create spy node descriptor");
    genesis_node_descriptor_set_userdata(project->spy_descr, project);
    genesis_node_descriptor_set_run_callback(project->spy_descr, spy_node_run);
    GenesisPortDescriptor *spy_in_port = genesis_node_descriptor_create_port(
            project->spy_descr, 0, GenesisPortTypeAudioIn, "audio_in");
    GenesisPortDescriptor *spy_out_port = genesis_node_descriptor_create_port(
            project->spy_descr, 1, GenesisPortTypeAudioOut, "audio_out");

    genesis_audio_port_descriptor_set_channel_layout(spy_in_port,
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono), true, 1);
    genesis_audio_port_descriptor_set_sample_rate(spy_in_port, 48000, true, 1);

    genesis_audio_port_descriptor_set_channel_layout(spy_out_port,
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono), false, -1);
    genesis_audio_port_descriptor_set_sample_rate(spy_out_port, 48000, false, -1);

    project->spy_node = genesis_node_descriptor_create_node(project->spy_descr);
    if (!project->spy_node)
        panic("unable to create spy node");


    // block until we have audio devices list
    genesis_refresh_audio_devices(project->genesis_context);

    // create hardware playback node
    int playback_device_index = genesis_get_default_playback_device_index(project->genesis_context);
    if (playback_device_index < 0)
        panic("error getting audio device list");

    GenesisAudioDevice *audio_device = genesis_get_audio_device(project->genesis_context, playback_device_index);
    if (!audio_device)
        panic("error getting playback device");

    GenesisNodeDescriptor *playback_node_descr;
    if ((err = genesis_audio_device_create_node_descriptor(audio_device, &playback_node_descr)))
        return err;

    project->playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!project->playback_node)
        panic("unable to create playback node");

    genesis_audio_device_unref(audio_device);

    play_audio_file(project, nullptr, true);

    return 0;
}

void project_tear_down_audio_graph(Project *project) {
    if (project->genesis_context) {
        genesis_stop_pipeline(project->genesis_context);
        genesis_node_destroy(project->playback_node);
        project->playback_node = nullptr;
    }
}

void project_play_sample_file(Project *project, const ByteBuffer &path) {
    GenesisAudioFile *audio_file;
    int err;
    if ((err = genesis_audio_file_load(project->genesis_context, path.raw(), &audio_file))) {
        fprintf(stderr, "unable to load audio file: %s\n", genesis_error_string(err));
        return;
    }

    play_audio_file(project, audio_file, false);
}

void project_play_audio_asset(Project *project, AudioAsset *audio_asset) {
    int err;
    if ((err = project_ensure_audio_asset_loaded(project, audio_asset))) {
        if (err == GenesisErrorDecodingAudio) {
            fprintf(stderr, "Error decoding audio\n");
            return;
        } else {
            ok_or_panic(err);
        }
    }

    play_audio_file(project, audio_asset->audio_file, true);
}

bool project_is_playing(Project *project) {
    return project->is_playing;
}

void project_set_play_head(Project *project, double pos) {
    project->play_head_pos = whole_notes_as_long(max(0.0, pos));
    project->events.trigger(EventProjectPlayHeadChanged);
}

void project_pause(Project *project) {
    if (!project->is_playing)
        return;
    project->is_playing = false;
    project->events.trigger(EventProjectPlayingChanged);
}

void project_play(Project *project) {
    if (project->is_playing)
        return;
    project->start_play_head_pos = whole_notes_as_double(project->play_head_pos);
    project->is_playing = true;
    project->events.trigger(EventProjectPlayingChanged);
}

void project_restart_playback(Project *project) {
    project->play_head_pos = whole_notes_as_long(project->start_play_head_pos);
    project->is_playing = true;
    project->events.trigger(EventProjectPlayHeadChanged);
    project->events.trigger(EventProjectPlayingChanged);
}

void project_stop_playback(Project *project) {
    project->is_playing = false;
    project->play_head_pos = whole_notes_as_long(0.0);
    project->start_play_head_pos = 0.0;
    project->events.trigger(EventProjectPlayHeadChanged);
    project->events.trigger(EventProjectPlayingChanged);
}

void project_flush_events(Project *project) {
    if (!project->play_head_changed_flag.test_and_set()) {
        project->events.trigger(EventProjectPlayHeadChanged);
    }
}

double project_play_head_pos(Project *project) {
    return whole_notes_as_double(project->play_head_pos);
}

void project_add_node_to_audio_clip(Project *project, AudioClip *audio_clip) {
    assert(!audio_clip->node_descr);
    assert(!audio_clip->node);

    ok_or_panic(project_ensure_audio_asset_loaded(project, audio_clip->audio_asset));
    GenesisAudioFile *audio_file = audio_clip->audio_asset->audio_file;

    const struct GenesisChannelLayout *channel_layout = genesis_audio_file_channel_layout(audio_file);
    int sample_rate = genesis_audio_file_sample_rate(audio_file);

    GenesisContext *context = project->genesis_context;
    const char *name = "audio_clip";
    char *description = create_formatted_str("Audio Clip: %s", audio_clip->name.encode().raw());
    GenesisNodeDescriptor *node_descr = ok_mem(genesis_create_node_descriptor(context, 2, name, description));
    free(description); description = nullptr;

    genesis_node_descriptor_set_userdata(node_descr, audio_clip);

    struct GenesisPortDescriptor *audio_out_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeAudioOut, "audio_out");

    struct GenesisPortDescriptor *events_in_port = genesis_node_descriptor_create_port(
            node_descr, 1, GenesisPortTypeEventsIn, "events_in");

    if (!audio_out_port || !events_in_port)
        panic("unable to create ports");

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port, channel_layout, true, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, sample_rate, true, -1);

    genesis_node_descriptor_set_run_callback(node_descr, audio_clip_node_run);
    genesis_node_descriptor_set_seek_callback(node_descr, audio_clip_node_seek);
    genesis_node_descriptor_set_create_callback(node_descr, audio_clip_node_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, audio_clip_node_destroy);

    audio_clip->node_descr = node_descr;
    audio_clip->node = genesis_node_descriptor_create_node(node_descr);
}

void project_remove_node_from_audio_clip(Project *project, AudioClip *audio_clip) {
    genesis_node_destroy(audio_clip->node);
    audio_clip->node = nullptr;

    genesis_node_descriptor_destroy(audio_clip->node_descr);
    audio_clip->node_descr = nullptr;
}

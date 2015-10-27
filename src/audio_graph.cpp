#include "audio_graph.hpp"
#include "mixer_node.hpp"
#include "settings_file.hpp"

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
    AudioGraphClip *clip;
    GenesisAudioFile *audio_file;
    int frame_pos;

    AudioClipVoice voices[AUDIO_CLIP_POLYPHONY];
    int next_note_index;
};

struct AudioClipEventNodeContext {
    AudioGraphClip *clip;
    double pos;
    bool detect_ongoing_notes;
};

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
    node->userdata = audio_clip_context;
    if (!node->userdata) {
        audio_clip_node_destroy(node);
        return GenesisErrorNoMem;
    }
    audio_clip_context->clip = (AudioGraphClip*)genesis_node_descriptor_userdata(node_descr);
    audio_clip_context->audio_file = audio_clip_context->clip->audio_clip->audio_asset->audio_file;
    return 0;
}

static void audio_clip_node_seek(struct GenesisNode *node) {
    struct AudioClipNodeContext *context = (struct AudioClipNodeContext*)node->userdata;
    struct GenesisPipeline *pipeline = genesis_node_pipeline(node);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);
    int frame_rate = genesis_audio_port_sample_rate(audio_out_port);
    context->frame_pos = genesis_whole_notes_to_frames(pipeline, node->timestamp, frame_rate);
    for (int voice_i = 0; voice_i < AUDIO_CLIP_POLYPHONY; voice_i += 1) {
        context->voices[voice_i].active = false;
    }
}

static void audio_clip_node_run(struct GenesisNode *node) {
    struct AudioClipNodeContext *context = (struct AudioClipNodeContext*)node->userdata;
    struct GenesisPipeline *pipeline = genesis_node_pipeline(node);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);
    struct GenesisPort *events_in_port = genesis_node_port(node, 1);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct SoundIoChannelLayout *channel_layout =
        genesis_audio_port_channel_layout(audio_out_port);
    int frame_rate = genesis_audio_port_sample_rate(audio_out_port);
    int channel_count = channel_layout->channel_count;
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    int frame_at_start = context->frame_pos;
    double whole_note_at_start = genesis_frames_to_whole_notes(pipeline, frame_at_start, frame_rate);
    int wanted_frame_at_end = frame_at_start + output_frame_count;
    double wanted_whole_note_at_end = genesis_frames_to_whole_notes(pipeline, wanted_frame_at_end, frame_rate);
    double event_time_requested = wanted_whole_note_at_end - whole_note_at_start;
    assert(event_time_requested >= 0.0);

    int event_count;
    double event_buf_size;
    genesis_events_in_port_fill_count(events_in_port, event_time_requested, &event_count, &event_buf_size);

    double whole_note_at_end = whole_note_at_start + event_buf_size;
    int frame_at_event_end = genesis_whole_notes_to_frames(pipeline, whole_note_at_end, frame_rate);
    int input_frame_count = frame_at_event_end - frame_at_start;
    int frame_count = min(output_frame_count, input_frame_count);
    int frame_at_consume_end = frame_at_start + frame_count;
    double whole_note_at_consume_end = genesis_frames_to_whole_notes(pipeline, frame_at_consume_end, frame_rate);
    double event_whole_notes_consumed = whole_note_at_consume_end - whole_note_at_start;

    GenesisMidiEvent *event = genesis_events_in_port_read_ptr(events_in_port);
    int event_index;
    for (event_index = 0; event_index < event_count; event_index += 1, event += 1) {
        int frame_at_this_event_start = genesis_whole_notes_to_frames(pipeline, event->start, frame_rate);
        int frames_until_start = frame_at_this_event_start - frame_at_start;
        int frame_index_offset = 0;
        if (frames_until_start < 0) {
            frame_index_offset = -frames_until_start;
            frames_until_start = 0;
        }
        if (frames_until_start >= frame_count ||
            event->data.segment_data.start + frame_index_offset >= event->data.segment_data.end)
        {
            break;
        }
        if (event->event_type == GenesisMidiEventTypeSegment) {
            AudioClipVoice *voice = find_next_voice(context);

            voice->active = true;
            voice->frames_until_start = frames_until_start;
            voice->frame_index = event->data.segment_data.start;
            voice->frame_end = event->data.segment_data.end;
            for (int ch = 0; ch < channel_count; ch += 1) {
                struct AudioClipNodeChannel *channel = &voice->channels[ch];
                channel->iter = genesis_audio_file_iterator(context->audio_file, ch,
                        frame_index_offset);
                channel->offset = 0;
            }
        }
    }
    genesis_events_in_port_advance_read_ptr(events_in_port, event_index, event_whole_notes_consumed);

    // set everything to silence and then we'll add samples in
    memset(out_buf, 0, frame_count * bytes_per_frame);

    for (int voice_i = 0; voice_i < AUDIO_CLIP_POLYPHONY; voice_i += 1) {
        AudioClipVoice *voice = &context->voices[voice_i];
        if (!voice->active)
            continue;

        int out_frame_count = min(frame_count, frame_count - voice->frames_until_start);
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

    context->frame_pos += frame_count;
    genesis_audio_out_port_advance_write_ptr(audio_out_port, frame_count);
}

static void audio_clip_event_node_destroy(struct GenesisNode *node) {
    AudioClipEventNodeContext *audio_clip_event_node_context = (AudioClipEventNodeContext*)node->userdata;
    destroy(audio_clip_event_node_context, 1);
}

static int audio_clip_event_node_create(struct GenesisNode *node) {
    const GenesisNodeDescriptor *node_descr = genesis_node_descriptor(node);
    AudioClipEventNodeContext *audio_clip_event_node_context = create_zero<AudioClipEventNodeContext>();
    node->userdata = audio_clip_event_node_context;
    if (!node->userdata) {
        audio_clip_node_destroy(node);
        return GenesisErrorNoMem;
    }
    audio_clip_event_node_context->clip =
        (AudioGraphClip*) genesis_node_descriptor_userdata(node_descr);
    return 0;
}

static void audio_clip_event_node_seek(struct GenesisNode *node) {
    AudioClipEventNodeContext *audio_clip_event_node_context = (AudioClipEventNodeContext*)node->userdata;
    audio_clip_event_node_context->pos = node->timestamp;
    audio_clip_event_node_context->detect_ongoing_notes = true;
}

static void audio_clip_event_node_run(struct GenesisNode *node) {
    AudioClipEventNodeContext *context = (AudioClipEventNodeContext*)node->userdata;
    AudioGraphClip *clip = context->clip;
    AudioGraph *ag = clip->audio_graph;
    struct GenesisPort *events_out_port = genesis_node_port(node, 0);

    int event_count;
    double event_time_requested;
    genesis_events_out_port_free_count(events_out_port, &event_count, &event_time_requested);
    GenesisMidiEvent *event_buf = genesis_events_out_port_write_ptr(events_out_port);

    double end_pos = context->pos + event_time_requested;

    int event_index = 0;
    if (ag->is_playing) {
        List<GenesisMidiEvent> *event_list = clip->events.get_read_ptr();
        for (int i = 0; i < event_list->length(); i += 1) {
            GenesisMidiEvent *event = &event_list->at(i);
            if ((event->start >= context->pos || context->detect_ongoing_notes) &&
                    event->start < end_pos)
            {
                *event_buf = *event;
                event_buf += 1;
                event_index += 1;

                if (event_index >= event_count) {
                    event_time_requested = event->start - context->pos;
                    break;
                }
            }
        }
    }
    context->detect_ongoing_notes = false;
    context->pos += event_time_requested;
    genesis_events_out_port_advance_write_ptr(events_out_port, event_index, event_time_requested);
}

static void audio_file_node_run(struct GenesisNode *node) {
    const struct GenesisNodeDescriptor *node_descriptor = genesis_node_descriptor(node);
    struct AudioGraph *ag = (struct AudioGraph *)genesis_node_descriptor_userdata(node_descriptor);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct SoundIoChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    int channel_count = channel_layout->channel_count;
    float *out_samples = genesis_audio_out_port_write_ptr(audio_out_port);

    int audio_file_frames_left = ag->audio_file_frame_count - ag->audio_file_frame_index;
    int frames_to_advance = min(output_frame_count, audio_file_frames_left);
    assert(frames_to_advance >= 0);

    for (int ch = 0; ch < channel_count; ch += 1) {
        struct PlayChannelContext *channel_context = &ag->audio_file_channel_context[ch];
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

    ag->audio_file_frame_index += frames_to_advance;
    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_frame_count);
}

static void render_node_run(struct GenesisNode *node) {
    const struct GenesisNodeDescriptor *node_descriptor = genesis_node_descriptor(node);
    struct AudioGraph *ag = (struct AudioGraph *)genesis_node_descriptor_userdata(node_descriptor);
    struct GenesisPort *audio_in_port = genesis_node_port(node, 0);

    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    GenesisAudioFileStream *afs = ag->render_stream;


    int frames_left = ag->render_frame_count - ag->render_frame_index;
    int write_count = min(input_frame_count, frames_left);

    if (write_count > 0) {
        int err;
        if ((err = genesis_audio_file_stream_write(afs, in_buf, write_count))) {
            panic("TODO handle this error");
        }

        long new_index = ag->render_frame_index.load() + write_count;
        assert(new_index <= ag->render_frame_count);
        bool done = new_index == ag->render_frame_count;

        if (done) {
            if ((err = genesis_audio_file_stream_close(afs))) {
                panic("TODO handle this error");
            }
        }

        ag->render_frame_index.store(new_index);
        ag->play_head_changed_flag.clear();

        if (done) {
            os_cond_signal(ag->render_cond, nullptr);
        }

        genesis_audio_in_port_advance_read_ptr(audio_in_port, write_count);
    }
}

static void stop_pipeline(AudioGraph *ag) {
    genesis_pipeline_stop(ag->pipeline);
    genesis_node_disconnect_all_ports(ag->master_node);

    genesis_node_destroy(ag->resample_node);
    ag->resample_node = nullptr;

    genesis_node_destroy(ag->mixer_node);
    ag->mixer_node = nullptr;

    genesis_node_descriptor_destroy(ag->mixer_descr);
    ag->mixer_descr = nullptr;

    for (int i = 0; i < ag->audio_clip_list.length(); i += 1) {
        AudioGraphClip *clip = ag->audio_clip_list.at(i);
        genesis_node_disconnect_all_ports(clip->node);

        genesis_node_destroy(clip->resample_node);
        clip->resample_node = nullptr;
    }
}

void audio_graph_start_pipeline(AudioGraph *ag) {
    int err;

    ag->start_play_head_pos = ag->play_head_pos;

    if (genesis_pipeline_is_running(ag->pipeline))
        return;

    int target_sample_rate = genesis_pipeline_get_sample_rate(ag->pipeline);
    SoundIoChannelLayout *target_channel_layout = genesis_pipeline_get_channel_layout(ag->pipeline);

    int audio_file_node_count = ag->audio_file_port_descr ? 1 : 0;

    if (audio_file_node_count >= 1) {

        if (ag->audio_file_node) {
            genesis_node_destroy(ag->audio_file_node);
            ag->audio_file_node = nullptr;
        }

        if (ag->preview_audio_file) {
            // Set channel layout
            const struct SoundIoChannelLayout *channel_layout =
                genesis_audio_file_channel_layout(ag->preview_audio_file);
            genesis_audio_port_descriptor_set_channel_layout(
                    ag->audio_file_port_descr, channel_layout, true, -1);

            // Set sample rate
            int sample_rate = genesis_audio_file_sample_rate(ag->preview_audio_file);
            genesis_audio_port_descriptor_set_sample_rate(ag->audio_file_port_descr, sample_rate, true, -1);

        } else {
            genesis_audio_port_descriptor_set_channel_layout(
                    ag->audio_file_port_descr, target_channel_layout, true, -1);
            genesis_audio_port_descriptor_set_sample_rate(
                    ag->audio_file_port_descr, target_sample_rate, true, -1);
        }
        ag->audio_file_node = ok_mem(genesis_node_descriptor_create_node(ag->audio_file_descr));
    }


    int resample_audio_out_index = genesis_node_descriptor_find_port_index(ag->resample_descr, "audio_out");
    assert(resample_audio_out_index >= 0);

    // one for each of the audio clips and one for the sample file preview node
    int mix_port_count = audio_file_node_count + ag->audio_clip_list.length();

    ok_or_panic(create_mixer_descriptor(ag->pipeline, mix_port_count, &ag->mixer_descr));
    ag->mixer_node = ok_mem(genesis_node_descriptor_create_node(ag->mixer_descr));

    ok_or_panic(genesis_connect_audio_nodes(ag->mixer_node, ag->master_node));

    // We start on mixer port index 1 because index 0 is the audio out. Index 1 is
    // the first audio in.
    int next_mixer_port = 1;
    if (audio_file_node_count >= 1) {
        int audio_out_port_index = genesis_node_descriptor_find_port_index(ag->audio_file_descr, "audio_out");
        if (audio_out_port_index < 0)
            panic("port not found");

        GenesisPort *audio_out_port = genesis_node_port(ag->audio_file_node, audio_out_port_index);
        GenesisPort *audio_in_port = genesis_node_port(ag->mixer_node, next_mixer_port++);

        if ((err = genesis_connect_ports(audio_out_port, audio_in_port))) {
            if (err == GenesisErrorIncompatibleChannelLayouts ||
                err == GenesisErrorIncompatibleSampleRates)
            {
                ag->resample_node = ok_mem(genesis_node_descriptor_create_node(ag->resample_descr));
                ok_or_panic(genesis_connect_audio_nodes(ag->audio_file_node, ag->resample_node));

                GenesisPort *audio_out_port = genesis_node_port(ag->resample_node, resample_audio_out_index);
                ok_or_panic(genesis_connect_ports(audio_out_port, audio_in_port));
            } else {
                ok_or_panic(err);
            }
        }
    }

    for (int i = 0; i < ag->audio_clip_list.length(); i += 1) {
        AudioGraphClip *clip = ag->audio_clip_list.at(i);

        int audio_out_port_index = genesis_node_descriptor_find_port_index(clip->node_descr, "audio_out");
        if (audio_out_port_index < 0)
            panic("port not found");

        GenesisPort *audio_out_port = genesis_node_port(clip->node, audio_out_port_index);
        GenesisPort *audio_in_port = genesis_node_port(ag->mixer_node, next_mixer_port++);

        if ((err = genesis_connect_ports(audio_out_port, audio_in_port))) {
            if (err == GenesisErrorIncompatibleChannelLayouts ||
                err == GenesisErrorIncompatibleSampleRates)
            {
                clip->resample_node = ok_mem(genesis_node_descriptor_create_node(ag->resample_descr));
                ok_or_panic(genesis_connect_audio_nodes(clip->node, clip->resample_node));

                GenesisPort *audio_out_port = genesis_node_port(clip->resample_node,
                        resample_audio_out_index);
                ok_or_panic(genesis_connect_ports(audio_out_port, audio_in_port));
            } else {
                ok_or_panic(err);
            }
        }

        GenesisPort *events_in_port = genesis_node_port(clip->node, 1);
        GenesisPort *events_out_port = genesis_node_port(clip->event_node, 0);

        ok_or_panic(genesis_connect_ports(events_out_port, events_in_port));
    }


    fprintf(stderr, "\nStarting pipeline...\n");
    genesis_debug_print_pipeline(ag->pipeline);

    double start_time = ag->play_head_pos;

    assert(next_mixer_port == mix_port_count + 1);
    if ((err = genesis_pipeline_start(ag->pipeline, start_time)))
        panic("unable to start pipeline: %s", genesis_strerror(err));
}

static void play_audio_file(AudioGraph *ag, GenesisAudioFile *audio_file, bool is_asset) {
    // TODO atomically modify the pipeline instead of stopping and starting
    stop_pipeline(ag);

    if (ag->preview_audio_file && !ag->preview_audio_file_is_asset) {
        genesis_audio_file_destroy(ag->preview_audio_file);
        ag->preview_audio_file = nullptr;
    } else if (ag->preview_audio_file && ag->preview_audio_file_is_asset) {
        ag->preview_audio_file = nullptr;
    }

    assert(!ag->preview_audio_file);
    ag->preview_audio_file = audio_file;
    ag->preview_audio_file_is_asset = is_asset;

    ag->audio_file_frame_count = 0;
    ag->audio_file_frame_index = 0;

    if (ag->preview_audio_file) {
        ag->audio_file_frame_count = genesis_audio_file_frame_count(audio_file);
        const struct SoundIoChannelLayout *channel_layout =
            genesis_audio_file_channel_layout(ag->preview_audio_file);
        for (int ch = 0; ch < channel_layout->channel_count; ch += 1) {
            struct PlayChannelContext *channel_context = &ag->audio_file_channel_context[ch];
            channel_context->offset = 0;
            channel_context->iter = genesis_audio_file_iterator(ag->preview_audio_file, ch, 0);
        }
    }

    audio_graph_start_pipeline(ag);
}

static SoundIoDevice *get_device_for_id(AudioGraph *ag, DeviceId device_id) {
    assert(device_id >= 1);
    assert(device_id < device_id_count());
    SettingsFileDeviceId *sf_device_id = &ag->settings_file->device_designations.at(device_id);

    if (sf_device_id->backend == SoundIoBackendNone)
        return genesis_get_default_output_device(ag->pipeline->context);

    SoundIoDevice *device = genesis_find_output_device(ag->pipeline->context,
            sf_device_id->backend, sf_device_id->device_id.raw(), sf_device_id->is_raw);

    if (device) {
        if (device->probe_error) {
            soundio_device_unref(device);
            return genesis_get_default_output_device(ag->pipeline->context);
        }
        return device;
    }

    return genesis_get_default_output_device(ag->pipeline->context);
}

static int init_playback_node(AudioGraph *ag) {
    MixerLine *master_mixer_line = ag->project->mixer_line_list.at(0);
    Effect *first_effect = master_mixer_line->effects.at(0);

    assert(first_effect->effect_type == EffectTypeSend);
    EffectSend *effect_send = &first_effect->effect.send;

    assert(effect_send->send_type == EffectSendTypeDevice);
    EffectSendDevice *send_device = &effect_send->send.device;

    SoundIoDevice *audio_device = get_device_for_id(ag, (DeviceId)send_device->device_id);
    if (!audio_device) {
        return GenesisErrorDeviceNotFound;
    }

    GenesisNodeDescriptor *playback_node_descr;
    int err;
    if ((err = genesis_audio_device_create_node_descriptor(ag->pipeline,
                    audio_device, &playback_node_descr)))
    {
        return err;
    }

    assert(!ag->master_node);
    ag->master_node = ok_mem(genesis_node_descriptor_create_node(playback_node_descr));

    soundio_device_unref(audio_device);

    return 0;
}

static void underrun_callback(void *userdata) {
    AudioGraph *ag = (AudioGraph *)userdata;
    ag->events.trigger(EventBufferUnderrun);
}

static void add_audio_node_to_audio_clip(AudioGraph *ag, AudioGraphClip *clip) {
    assert(!clip->node_descr);
    assert(!clip->node);

    ok_or_panic(project_ensure_audio_asset_loaded(ag->project, clip->audio_clip->audio_asset));
    GenesisAudioFile *audio_file = clip->audio_clip->audio_asset->audio_file;

    const struct SoundIoChannelLayout *channel_layout =
        genesis_audio_file_channel_layout(audio_file);
    int sample_rate = genesis_audio_file_sample_rate(audio_file);

    const char *name = "audio_clip";
    char *description = create_formatted_str("Audio Clip: %s",
            clip->audio_clip->name.encode().raw());
    GenesisNodeDescriptor *node_descr = ok_mem(genesis_create_node_descriptor(ag->pipeline, 2, name, description));
    free(description); description = nullptr;

    genesis_node_descriptor_set_userdata(node_descr, clip);

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

    clip->node_descr = node_descr;
    clip->node = ok_mem(genesis_node_descriptor_create_node(node_descr));
}

static void add_event_node_to_audio_clip(AudioGraph *ag, AudioGraphClip *clip) {
    assert(!clip->event_node_descr);
    assert(!clip->event_node);

    const char *name = "audio_clip_events";
    char *description = create_formatted_str("Events for Audio Clip: %s",
            clip->audio_clip->name.encode().raw());
    GenesisNodeDescriptor *node_descr =
        ok_mem(genesis_create_node_descriptor(ag->pipeline, 1, name, description));
    free(description);
    description = nullptr;

    genesis_node_descriptor_set_userdata(node_descr, clip);

    struct GenesisPortDescriptor *events_out_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeEventsOut, "events_out");

    if (!events_out_port)
        panic("unable to create ports");

    genesis_node_descriptor_set_run_callback(node_descr, audio_clip_event_node_run);
    genesis_node_descriptor_set_seek_callback(node_descr, audio_clip_event_node_seek);
    genesis_node_descriptor_set_create_callback(node_descr, audio_clip_event_node_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, audio_clip_event_node_destroy);

    clip->event_node_descr = node_descr;
    clip->event_node = ok_mem(genesis_node_descriptor_create_node(node_descr));
}

static void add_nodes_to_audio_clip(AudioGraph *ag, AudioGraphClip *clip) {
    add_audio_node_to_audio_clip(ag, clip);
    add_event_node_to_audio_clip(ag, clip);
}

static void refresh_audio_clips(AudioGraph *ag) {
    Project *project = ag->project;
    int ag_i = 0;
    int project_i = 0;
    for (;;) {
        AudioGraphClip *ag_clip = nullptr;
        AudioClip *project_clip = nullptr;
        if (ag_i < ag->audio_clip_list.length())
            ag_clip = ag->audio_clip_list.at(ag_i);
        if (project_i < project->audio_clip_list.length())
            project_clip = project->audio_clip_list.at(project_i);
        if (ag_clip && project_clip && ag_clip->audio_clip == project_clip) {
            ag_i += 1;
            project_i += 1;
        } else if (project_clip && !ag_clip) {
            ag_clip = ok_mem(create_zero<AudioGraphClip>());
            ag_clip->audio_clip = project_clip;
            ag_clip->audio_graph = ag;
            add_nodes_to_audio_clip(ag, ag_clip);
            ok_or_panic(ag->audio_clip_list.append(ag_clip));
            ag_i += 1;
            project_i += 1;
        } else if (!project_clip && ag_clip) {
            panic("TODO destroy audio graph clip");
            ag->audio_clip_list.swap_remove(ag_i);
        } else if (!project_clip && !ag_clip) {
            break;
        } else {
            panic("TODO replace nodes");
        }
    }
}

static void refresh_audio_clip_segments(AudioGraph *ag) {
    for (int clip_i = 0; clip_i < ag->audio_clip_list.length(); clip_i += 1) {
        AudioGraphClip *clip = ag->audio_clip_list.at(clip_i);
        clip->audio_clip->userdata = clip;
        clip->events_write_ptr = clip->events.write_begin();
        clip->events_write_ptr->clear();
    }

    auto it = ag->project->audio_clip_segments.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        AudioClipSegment *segment = entry->value;
        AudioClip *audio_clip = segment->audio_clip;
        AudioGraphClip *clip = (AudioGraphClip *)audio_clip->userdata;
        assert(clip);
        ok_or_panic(clip->events_write_ptr->add_one());
        GenesisMidiEvent *event = &clip->events_write_ptr->last();
        event->event_type = GenesisMidiEventTypeSegment;
        event->start = segment->pos;
        event->data.segment_data.start = segment->start;
        event->data.segment_data.end = segment->end;
    }

    for (int i = 0; i < ag->audio_clip_list.length(); i += 1) {
        AudioGraphClip *clip = ag->audio_clip_list.at(i);
        clip->events.write_end();
        clip->events_write_ptr = nullptr;
    }
}

static void on_project_audio_clips_changed(Event, void *userdata) {
    AudioGraph *ag = (AudioGraph *) userdata;
    refresh_audio_clips(ag);
}

static void on_project_audio_clip_segments_changed(Event, void *userdata) {
    AudioGraph *ag = (AudioGraph *) userdata;
    refresh_audio_clip_segments(ag);
}

static AudioGraph *audio_graph_create_common(Project *project, GenesisContext *genesis_context,
        double latency)
{
    GenesisPipeline *pipeline;
    ok_or_panic(genesis_pipeline_create(genesis_context, &pipeline));

    genesis_pipeline_set_latency(pipeline, latency);
    genesis_pipeline_set_sample_rate(pipeline, project->sample_rate);
    genesis_pipeline_set_channel_layout(pipeline, &project->channel_layout);

    AudioGraph *ag = ok_mem(create_zero<AudioGraph>());
    ag->project = project;
    ag->pipeline = pipeline;
    ag->play_head_pos = 0.0;
    ag->is_playing = false;
    ag->play_head_changed_flag.clear();

    ag->resample_descr = genesis_node_descriptor_find(ag->pipeline, "resample");
    if (!ag->resample_descr)
        panic("unable to find resampler");

    genesis_pipeline_set_underrun_callback(pipeline, underrun_callback, ag);

    project->events.attach_handler(EventProjectAudioClipsChanged,
            on_project_audio_clips_changed, ag);
    project->events.attach_handler(EventProjectAudioClipSegmentsChanged,
            on_project_audio_clip_segments_changed, ag);


    refresh_audio_clips(ag);
    refresh_audio_clip_segments(ag);

    return ag;

}

int audio_graph_create_playback(Project *project, GenesisContext *genesis_context,
        SettingsFile *settings_file, AudioGraph **out_audio_graph)
{
    AudioGraph *ag = audio_graph_create_common(project, genesis_context, settings_file->latency);

    ag->settings_file = settings_file;

    ag->audio_file_descr = genesis_create_node_descriptor(ag->pipeline,
            1, "audio_file", "Audio file playback.");
    if (!ag->audio_file_descr)
        panic("unable to create audio file node descriptor");

    genesis_node_descriptor_set_userdata(ag->audio_file_descr, ag);
    genesis_node_descriptor_set_run_callback(ag->audio_file_descr, audio_file_node_run);
    ag->audio_file_port_descr = genesis_node_descriptor_create_port(
            ag->audio_file_descr, 0, GenesisPortTypeAudioOut, "audio_out");
    if (!ag->audio_file_port_descr)
        panic("unable to create audio out port descriptor");
    ag->audio_file_node = nullptr;


    init_playback_node(ag);

    play_audio_file(ag, nullptr, true);


    *out_audio_graph = ag;
    return 0;
}

int audio_graph_create_render(Project *project, GenesisContext *genesis_context,
        const GenesisExportFormat *export_format, const ByteBuffer &out_path,
        AudioGraph **out_audio_graph)
{
    AudioGraph *ag = audio_graph_create_common(project, genesis_context, 0.10);

    ag->render_export_format = *export_format;
    ag->render_out_path = out_path;

    ag->render_frame_index = 0;
    ag->render_frame_count = project_get_duration_frames(project);
    ag->render_cond = ok_mem(os_cond_create());
    ag->is_playing = true;

    ag->render_descr = genesis_create_node_descriptor(ag->pipeline,
            1, "render", "Master render node.");
    if (!ag->render_descr)
        panic("unable to create render node descriptor");

    genesis_node_descriptor_set_userdata(ag->render_descr, ag);
    genesis_node_descriptor_set_run_callback(ag->render_descr, render_node_run);
    ag->render_port_descr = genesis_node_descriptor_create_port(
            ag->render_descr, 0, GenesisPortTypeAudioIn, "audio_in");
    if (!ag->render_port_descr)
        panic("unable to create render port descriptor");
    genesis_audio_port_descriptor_set_channel_layout(ag->render_port_descr,
            &project->channel_layout, true, -1);
    genesis_audio_port_descriptor_set_sample_rate(ag->render_port_descr,
            export_format->sample_rate, true, -1);
    genesis_audio_port_descriptor_set_is_sink(ag->render_port_descr, true);

    ag->master_node = ok_mem(genesis_node_descriptor_create_node(ag->render_descr));

    ag->render_stream = ok_mem(genesis_audio_file_stream_create(ag->pipeline->context));

    int render_sample_rate = genesis_audio_file_codec_best_sample_rate(export_format->codec,
            export_format->sample_rate);

    genesis_audio_file_stream_set_sample_rate(ag->render_stream, render_sample_rate);
    genesis_audio_file_stream_set_channel_layout(ag->render_stream, &project->channel_layout);

    ByteBuffer encoded;
    encoded = project->tag_title.encode();
    genesis_audio_file_stream_set_tag(ag->render_stream, "title", -1, encoded.raw(), encoded.length());

    encoded = project->tag_artist.encode();
    genesis_audio_file_stream_set_tag(ag->render_stream, "artist", -1, encoded.raw(), encoded.length());

    encoded = project->tag_album_artist.encode();
    genesis_audio_file_stream_set_tag(ag->render_stream, "album_artist", -1, encoded.raw(), encoded.length());

    encoded = project->tag_album.encode();
    genesis_audio_file_stream_set_tag(ag->render_stream, "album", -1, encoded.raw(), encoded.length());

    // TODO looks like I messed up the year tag; it should actually be ISO 8601 "date"
    // TODO so we need to write the date tag here, not year.

    genesis_audio_file_stream_set_export_format(ag->render_stream, export_format);

    int err;
    if ((err = genesis_audio_file_stream_open(ag->render_stream, out_path.raw(),
                    out_path.length())))
    {
        audio_graph_destroy(ag);
        return err;
    }

    *out_audio_graph = ag;
    return 0;
}

static void audio_graph_clip_destroy(AudioGraphClip *clip) {
    if (!clip)
        return;

    if (clip->event_node)
        genesis_node_destroy(clip->event_node);

    if (clip->event_node_descr)
        genesis_node_descriptor_destroy(clip->event_node_descr);

    if (clip->node)
        genesis_node_destroy(clip->node);

    if (clip->node_descr)
        genesis_node_descriptor_destroy(clip->node_descr);

    destroy(clip, 1);
}

void audio_graph_destroy(AudioGraph *ag) {
    if (!ag)
        return;

    if (ag->pipeline) {
        genesis_pipeline_stop(ag->pipeline);
        genesis_node_destroy(ag->master_node);
        ag->master_node = nullptr;
    }

    ag->project->events.detach_handler(EventProjectAudioClipsChanged,
            on_project_audio_clips_changed);
    ag->project->events.detach_handler(EventProjectAudioClipSegmentsChanged,
            on_project_audio_clip_segments_changed);

    while (ag->audio_clip_list.length()) {
        AudioGraphClip *clip = ag->audio_clip_list.pop();
        audio_graph_clip_destroy(clip);
    }

    os_cond_destroy(ag->render_cond);
}

void audio_graph_play_sample_file(AudioGraph *ag, const ByteBuffer &path) {
    GenesisAudioFile *audio_file;
    int err;
    if ((err = genesis_audio_file_load(ag->pipeline->context, path.raw(), &audio_file))) {
        fprintf(stderr, "unable to load audio file: %s\n", genesis_strerror(err));
        return;
    }

    play_audio_file(ag, audio_file, false);
}

void audio_graph_play_audio_asset(AudioGraph *ag, AudioAsset *audio_asset) {
    int err;
    if ((err = project_ensure_audio_asset_loaded(ag->project, audio_asset))) {
        if (err == GenesisErrorDecodingAudio) {
            fprintf(stderr, "Error decoding audio\n");
            return;
        } else {
            ok_or_panic(err);
        }
    }

    play_audio_file(ag, audio_asset->audio_file, true);
}

bool audio_graph_is_playing(AudioGraph *ag) {
    return ag->is_playing;
}

void audio_graph_set_play_head(AudioGraph *ag, double target_pos) {
    bool want_pipeline_running = genesis_pipeline_is_running(ag->pipeline);
    if (want_pipeline_running) {
        genesis_pipeline_pause(ag->pipeline);
    }

    double pos = max(0.0, target_pos);
    ag->start_play_head_pos = pos;
    ag->play_head_pos = pos;

    genesis_pipeline_seek(ag->pipeline, target_pos);

    if (want_pipeline_running) {
        ok_or_panic(genesis_pipeline_resume(ag->pipeline));
    }

    ag->events.trigger(EventAudioGraphPlayHeadChanged);
}

static double get_playing_play_pos(AudioGraph *ag) {
    GenesisPort *audio_in_port = genesis_node_port(ag->master_node, 0);
    int sample_rate = genesis_audio_port_sample_rate(audio_in_port);
    long frame_at_start = genesis_whole_notes_to_frames(ag->pipeline, ag->start_play_head_pos,
            sample_rate);
    long frame_at_end = frame_at_start + genesis_node_playback_offset(ag->master_node);
    double new_play_head_pos = genesis_frames_to_whole_notes(ag->pipeline, frame_at_end,
            sample_rate);

    double device_latency = genesis_node_playback_latency(ag->master_node);

    double result = max(0.0, new_play_head_pos - device_latency);

    return result;
}

void audio_graph_pause(AudioGraph *ag) {
    if (!ag->is_playing.exchange(false))
        return;
    ag->play_head_pos = get_playing_play_pos(ag);
    ag->events.trigger(EventAudioGraphPlayingChanged);
}

void audio_graph_play(AudioGraph *ag) {
    if (ag->is_playing.exchange(true))
        return;
    audio_graph_start_pipeline(ag);
    ag->events.trigger(EventAudioGraphPlayingChanged);
}

void audio_graph_restart_playback(AudioGraph *ag) {
    audio_graph_set_play_head(ag, ag->start_play_head_pos);
    audio_graph_play(ag);
}

void audio_graph_recover_stream(AudioGraph *ag, double new_latency) {
    ag->play_head_pos = audio_graph_play_head_pos(ag);
    stop_pipeline(ag);
    genesis_pipeline_set_latency(ag->pipeline, new_latency);
    audio_graph_start_pipeline(ag);
}

void audio_graph_change_sample_rate(AudioGraph *ag, int new_sample_rate) {
    ag->play_head_pos = audio_graph_play_head_pos(ag);
    stop_pipeline(ag);


    GenesisNodeDescriptor *playback_node_descr = genesis_node_descriptor(ag->master_node);

    genesis_node_destroy(ag->master_node);
    ag->master_node = nullptr;

    genesis_node_descriptor_destroy(playback_node_descr);
    playback_node_descr = nullptr;

    genesis_pipeline_set_sample_rate(ag->pipeline, new_sample_rate);

    init_playback_node(ag);

    audio_graph_start_pipeline(ag);
}

void audio_graph_recover_sound_backend_disconnect(AudioGraph *ag) {
    if (!ag->master_node)
        return;
    ag->play_head_pos = audio_graph_play_head_pos(ag);

    GenesisNodeDescriptor *playback_node_descr = genesis_node_descriptor(ag->master_node);


    stop_pipeline(ag);


    genesis_node_destroy(ag->master_node);
    ag->master_node = nullptr;

    genesis_node_descriptor_destroy(playback_node_descr);
    playback_node_descr = nullptr;

    init_playback_node(ag);

    audio_graph_start_pipeline(ag);
}

void audio_graph_stop_playback(AudioGraph *ag) {
    stop_pipeline(ag);
    ag->is_playing = false;
    ag->play_head_pos = 0.0;
    ag->start_play_head_pos = 0.0;
    ag->events.trigger(EventAudioGraphPlayHeadChanged);
    ag->events.trigger(EventAudioGraphPlayingChanged);
}

void audio_graph_flush_events(AudioGraph *ag) {
    if ((!ag->render_stream && ag->is_playing) || !ag->play_head_changed_flag.test_and_set()) {
        ag->events.trigger(EventAudioGraphPlayHeadChanged);
    }
}

double audio_graph_play_head_pos(AudioGraph *ag) {
    assert(!ag->render_stream);

    bool is_playing = ag->is_playing.load();

    double play_head_pos = is_playing ? get_playing_play_pos(ag) : ag->play_head_pos;
    return play_head_pos;
}

double audio_graph_get_latency(AudioGraph *audio_graph) {
    return genesis_pipeline_get_latency(audio_graph->pipeline);
}

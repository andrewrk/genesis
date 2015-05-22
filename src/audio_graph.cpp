#include "audio_graph.hpp"

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

static void connect_audio_nodes(Project *project, GenesisNode *source, GenesisNode *dest) {
    int err = genesis_connect_audio_nodes(source, dest);
    if (!err)
        return;
    if (err == GenesisErrorIncompatibleChannelLayouts || err == GenesisErrorIncompatibleSampleRates) {
        project->resample_node = genesis_node_descriptor_create_node(project->resample_descr);
        if (!project->resample_node)
            panic("unable to create resample node");

        if ((err = genesis_connect_audio_nodes(source, project->resample_node)))
            panic("unable to connect source to resampler: %s", genesis_error_string(err));
        if ((err = genesis_connect_audio_nodes(project->resample_node, dest)))
            panic("unable to connect resampler to dest: %s", genesis_error_string(err));

        return;
    }
    panic("unable to connect source to dest: %s", genesis_error_string(err));
}

int project_set_up_audio_graph(Project *project) {
    int err;

    project->play_head_pos = 0.0;

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

    return 0;
}

void project_tear_down_audio_graph(Project *project) {
    if (project->genesis_context) {
        genesis_stop_pipeline(project->genesis_context);
        genesis_node_destroy(project->playback_node);
        project->playback_node = nullptr;
    }
}

static void play_audio_file(Project *project, GenesisAudioFile *audio_file, bool is_asset) {
    const struct GenesisChannelLayout *channel_layout = genesis_audio_file_channel_layout(audio_file);
    int sample_rate = genesis_audio_file_sample_rate(audio_file);

    genesis_stop_pipeline(project->genesis_context);

    genesis_node_disconnect_all_ports(project->playback_node);

    if (project->resample_node) {
        genesis_node_destroy(project->resample_node);
        project->resample_node = nullptr;
    }

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

    project->audio_file_frame_count = genesis_audio_file_frame_count(project->audio_file);
    project->audio_file_frame_index = 0;
    for (int ch = 0; ch < channel_layout->channel_count; ch += 1) {
        struct PlayChannelContext *channel_context = &project->audio_file_channel_context[ch];
        channel_context->offset = 0;
        channel_context->iter = genesis_audio_file_iterator(project->audio_file, ch, 0);
    }

    connect_audio_nodes(project, project->audio_file_node, project->playback_node);

    int err;
    if ((err = genesis_start_pipeline(project->genesis_context)))
        panic("unable to start pipeline: %s", genesis_error_string(err));
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

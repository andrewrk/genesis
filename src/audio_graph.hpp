#ifndef AUDIO_GRAPH_HPP
#define AUDIO_GRAPH_HPP

#include "project.hpp"
#include "atomic_value.hpp"
#include "atomic_double.hpp"
#include "atomics.hpp"
#include "midi_hardware.hpp"
#include "settings_file.hpp"
#include "event_dispatcher.hpp"

struct EventList {
    List<GenesisMidiEvent> events;
};

struct AudioGraph;

struct AudioGraphClip {
    AudioGraph *audio_graph;
    AudioClip *audio_clip;
    GenesisNodeDescriptor *node_descr;
    GenesisNode *node;
    GenesisNodeDescriptor *event_node_descr;
    GenesisNode *event_node;
    GenesisNode *resample_node;
    AtomicValue<List<GenesisMidiEvent>> events;
    List<GenesisMidiEvent> *events_write_ptr;
};

struct AudioGraph {
    Project *project;
    EventDispatcher events;

    List<AudioGraphClip*> audio_clip_list;

    GenesisPipeline *pipeline;
    SettingsFile *settings_file;
    GenesisNodeDescriptor *resample_descr;
    GenesisNodeDescriptor *mixer_descr;
    GenesisNode *resample_node;
    GenesisNode *mixer_node;
    GenesisNode *master_node;

    long audio_file_frame_count;
    long audio_file_frame_index;
    PlayChannelContext audio_file_channel_context[GENESIS_MAX_CHANNELS];

    GenesisPortDescriptor *audio_file_port_descr;
    GenesisNodeDescriptor *audio_file_descr;
    GenesisNode *audio_file_node;
    GenesisAudioFile *preview_audio_file;
    bool preview_audio_file_is_asset;

    GenesisNodeDescriptor *render_descr;
    GenesisPortDescriptor *render_port_descr;
    ByteBuffer render_out_path;
    GenesisExportFormat render_export_format;
    GenesisAudioFileStream *render_stream;
    atomic_long render_frame_index;
    long render_frame_count;
    atomic_flag render_updated_flag;
    OsCond *render_cond;

    double start_play_head_pos;
    AtomicDouble play_head_pos;
    atomic_bool is_playing;
    atomic_flag play_head_changed_flag;
};

int audio_graph_create_playback(Project *project, GenesisContext *genesis_context,
        SettingsFile *settings_file, AudioGraph **out_audio_graph);
int audio_graph_create_render(Project *project, GenesisContext *genesis_context,
        const GenesisExportFormat *export_format, const ByteBuffer &out_path,
        AudioGraph **out_audio_graph);
void audio_graph_destroy(AudioGraph *audio_graph);

void audio_graph_start_pipeline(AudioGraph *audio_graph);

double audio_graph_get_latency(AudioGraph *audio_graph);

void audio_graph_play_sample_file(AudioGraph *audio_graph, const ByteBuffer &path);
void audio_graph_play_audio_asset(AudioGraph *audio_graph, AudioAsset *audio_asset);

void audio_graph_set_play_head(AudioGraph *audio_graph, double pos);

bool audio_graph_is_playing(AudioGraph *audio_graph);
void audio_graph_pause(AudioGraph *audio_graph);
void audio_graph_play(AudioGraph *audio_graph);
void audio_graph_restart_playback(AudioGraph *audio_graph);
void audio_graph_stop_playback(AudioGraph *audio_graph);

void audio_graph_recover_stream(AudioGraph *audio_graph, double new_latency);
void audio_graph_recover_sound_backend_disconnect(AudioGraph *audio_graph);
void audio_graph_change_sample_rate(AudioGraph *audio_graph, int new_sample_rate);

void audio_graph_flush_events(AudioGraph *audio_graph);
double audio_graph_play_head_pos(AudioGraph *audio_graph);

#endif

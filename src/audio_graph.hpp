#ifndef AUDIO_GRAPH_HPP
#define AUDIO_GRAPH_HPP

#include "project.hpp"

int project_set_up_audio_graph(Project *project);
void project_tear_down_audio_graph(Project *project);
void project_play_sample_file(Project *project, const ByteBuffer &path);
void project_play_audio_asset(Project *project, AudioAsset *audio_asset);

void project_set_play_head(Project *project, double pos);

bool project_is_playing(Project *project);
void project_pause(Project *project);
void project_play(Project *project);
void project_restart_playback(Project *project);
void project_stop_playback(Project *project);

void project_flush_events(Project *project);

#endif

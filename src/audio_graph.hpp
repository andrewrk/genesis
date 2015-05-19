#ifndef AUDIO_GRAPH_HPP
#define AUDIO_GRAPH_HPP

#include "project.hpp"

int project_set_up_audio_graph(Project *project);
void project_tear_down_audio_graph(Project *project);
void project_play_sample_file(Project *project, const ByteBuffer &path);
void project_play_audio_asset(Project *project, AudioAsset *audio_asset);

#endif

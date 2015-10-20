#ifndef GENESIS_RENDER_JOB
#define GENESIS_RENDER_JOB

#include "project.hpp"

struct AudioGraph;
struct GenesisContext;
class Gui;

struct RenderJob {
    Project *project;
    AudioGraph *audio_graph;
    GenesisContext *genesis_context;
    Gui *gui;
    bool is_complete;
};

void render_job_init(RenderJob *rj, Project *project, GenesisContext *genesis_context, Gui *gui);
void render_job_deinit(RenderJob *rj);

void render_job_start(RenderJob *rj, const GenesisExportFormat *export_format, const ByteBuffer &out_path);
float render_job_progress(RenderJob *rj);

void render_job_stop(RenderJob *rj);
bool render_job_is_complete(RenderJob *rj);

#endif

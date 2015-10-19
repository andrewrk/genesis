#ifndef GENESIS_RENDER_JOB
#define GENESIS_RENDER_JOB

#include "project.hpp"

struct AudioGraph;
struct GenesisContext;

struct RenderJob {
    Project *project;
    AudioGraph *audio_graph;
    GenesisContext *genesis_context;
};

void render_job_init(RenderJob *rj, Project *project);
void render_job_deinit(RenderJob *rj);

void render_job_start(RenderJob *rj, const GenesisExportFormat *export_format, const ByteBuffer &out_path);

float render_job_stop(RenderJob *rj);

#endif

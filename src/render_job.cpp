#include "render_job.hpp"
#include "audio_graph.hpp"

void render_job_init(RenderJob *rj, Project *project, GenesisContext *genesis_context) {
    rj->project = project;
    rj->audio_graph = nullptr;
    rj->genesis_context = genesis_context;
}

void render_job_deinit(RenderJob *rj) {
    render_job_stop(rj);
    audio_graph_destroy(rj->audio_graph);
}

void render_job_start(RenderJob *rj, const GenesisExportFormat *export_format,
        const ByteBuffer &out_path)
{
    audio_graph_create_render(rj->project, rj->genesis_context, export_format, out_path,
            &rj->audio_graph);
}

float render_job_stop(RenderJob *rj) {
    panic("TODO");
}

float render_job_progress(RenderJob *rj) {
    panic("TODO");
}


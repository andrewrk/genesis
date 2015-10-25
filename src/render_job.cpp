#include "genesis.h"
#include "byte_buffer.hpp"
#include "render_job.hpp"
#include "audio_graph.hpp"
#include "gui.hpp"

static void on_render_job_updated(Event, void *userdata) {
    RenderJob *rj = (RenderJob *)userdata;
    assert(rj);

    if (rj->audio_graph->render_frame_index == rj->audio_graph->render_frame_count) {
        rj->is_complete = true;
        render_job_stop(rj);
    }

    rj->gui->events.trigger(EventRenderJobsUpdated);
}

void render_job_init(RenderJob *rj, Project *project, GenesisContext *genesis_context, Gui *gui) {
    assert(rj);
    rj->project = project;
    rj->audio_graph = nullptr;
    rj->genesis_context = genesis_context;
    rj->gui = gui;
    rj->is_complete = false;
}

void render_job_deinit(RenderJob *rj) {
    assert(rj);
    render_job_stop(rj);
}

void render_job_start(RenderJob *rj, const GenesisExportFormat *export_format,
        const ByteBuffer &out_path)
{
    assert(rj);
    audio_graph_create_render(rj->project, rj->genesis_context, export_format, out_path,
            &rj->audio_graph);

    rj->audio_graph->events.attach_handler(EventAudioGraphPlayHeadChanged, on_render_job_updated, rj);

    audio_graph_start_pipeline(rj->audio_graph);
}

void render_job_stop(RenderJob *rj) {
    assert(rj);
    audio_graph_destroy(rj->audio_graph);
    rj->audio_graph = nullptr;
}

float render_job_progress(RenderJob *rj) {
    assert(rj);
    assert(rj->audio_graph);
    double nominator = rj->audio_graph->render_frame_index;
    double denominator = rj->audio_graph->render_frame_count;
    return nominator / denominator;
}

bool render_job_is_complete(RenderJob *rj) {
    assert(rj);
    return rj->is_complete;
}

void render_job_flush_events(RenderJob *rj) {
    if (rj->audio_graph)
        audio_graph_flush_events(rj->audio_graph);
}

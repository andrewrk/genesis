#include "track_editor_widget.hpp"
#include "project.hpp"

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    // TODO
}

void TrackEditorWidget::update_model() {
    auto it = project->tracks.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        Track *track = entry->value;
        fprintf(stderr, "track %s\n", track->name.encode().raw());
    }
}

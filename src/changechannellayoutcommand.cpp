#include "changechannellayoutcommand.h"

#include "project.h"
#include "projectwindow.h"

ChangeChannelLayoutCommand::ChangeChannelLayoutCommand(
        uint64_t layout, Project *project,
        ProjectWindow *window, QUndoCommand *parent) :
    QUndoCommand(parent),
    project(project),
    window(window)
{
    newChannelLayout = layout;
    oldChannelLayout = project->getChannelLayout();
    this->setText(QObject::tr("Set Channel Layout to %1").arg(genesis_get_channel_layout_string(layout)));
}

ChangeChannelLayoutCommand::~ChangeChannelLayoutCommand()
{

}

void ChangeChannelLayoutCommand::undo()
{
    project->setChannelLayout(oldChannelLayout);
    window->updateChannelLayoutUi();
}

void ChangeChannelLayoutCommand::redo()
{
    project->setChannelLayout(newChannelLayout);
    window->updateChannelLayoutUi();
}

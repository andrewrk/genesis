#ifndef CHANGECHANNELLAYOUTCOMMAND_H
#define CHANGECHANNELLAYOUTCOMMAND_H

#include <QUndoCommand>

#include "channel_layout.h"

class Project;
class ProjectWindow;
class ChangeChannelLayoutCommand : public QUndoCommand
{
public:
    ChangeChannelLayoutCommand(uint64_t layout, Project *project, ProjectWindow *window, QUndoCommand *parent = 0);
    ~ChangeChannelLayoutCommand();

    void undo();
    void redo();
private:
    Project *project;
    ProjectWindow *window;
    uint64_t newChannelLayout;
    uint64_t oldChannelLayout;
};

#endif // CHANGECHANNELLAYOUTCOMMAND_H

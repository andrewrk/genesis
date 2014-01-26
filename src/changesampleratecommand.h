#ifndef CHANGESAMPLERATECOMMAND_H
#define CHANGESAMPLERATECOMMAND_H

#include <QUndoCommand>

class Project;
class ProjectWindow;
class ChangeSampleRateCommand : public QUndoCommand
{
public:
    ChangeSampleRateCommand(int sampleRate, Project *project, ProjectWindow *window, QUndoCommand *parent = 0);
    ~ChangeSampleRateCommand();

    void undo();
    void redo();
private:
    Project *project;
    ProjectWindow *window;
    int newSampleRate;
    int oldSampleRate;
};

#endif // CHANGESAMPLERATECOMMAND_H

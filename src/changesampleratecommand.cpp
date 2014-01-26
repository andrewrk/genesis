#include "changesampleratecommand.h"

#include "project.h"
#include "projectwindow.h"


ChangeSampleRateCommand::ChangeSampleRateCommand(
        int sampleRate, Project *project,
        ProjectWindow *window, QUndoCommand *parent) :
    QUndoCommand(parent),
    project(project),
    window(window)
{
    newSampleRate = sampleRate;
    oldSampleRate = project->getSampleRate();
    this->setText(QObject::tr("Set Sample Rate to %1").arg(newSampleRate));
}

ChangeSampleRateCommand::~ChangeSampleRateCommand()
{

}

void ChangeSampleRateCommand::undo()
{
    project->setSampleRate(oldSampleRate);
    window->updateSampleRateUi();
}

void ChangeSampleRateCommand::redo()
{
    project->setSampleRate(newSampleRate);
    window->updateSampleRateUi();
}

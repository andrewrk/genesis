#include "projectwindow.h"
#include "ui_projectwindow.h"

#include "changesampleratecommand.h"

static const int sampleRateCount = 7;
static const int sampleRates[sampleRateCount] = {
    8000,
    22050,
    44100,
    48000,
    88200,
    96000,
    352800,
};

ProjectWindow::ProjectWindow(Project *project, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ProjectWindow),
    project(project)
{
	ui->setupUi(this);

    ui->menuView->addAction(ui->undoRedoDock->toggleViewAction());

    undoStack = new QUndoStack(this);

    undoView = new QUndoView(undoStack);
    ui->undoRedoDock->setWidget(undoView);

    bool ok;
    for (int i = 0; i < sampleRateCount; i += 1) {
        int rate = sampleRates[i];
        QAction *action = new QAction(tr("%1 Hz").arg(QString::number(rate)), this);
        action->setData(rate);
        action->setCheckable(true);
        sampleRateActions.append(action);
        ok = connect(action, SIGNAL(triggered()), this, SLOT(changeSampleRate()));
        Q_ASSERT(ok);
    }
    ui->menuSampleRate->addActions(sampleRateActions);
    updateSampleRateUi();

    ok = connect(undoStack, SIGNAL(canRedoChanged(bool)), this, SLOT(updateUndoRedoMenuText()));
    Q_ASSERT(ok);
    ok = connect(undoStack, SIGNAL(canUndoChanged(bool)), this, SLOT(updateUndoRedoMenuText()));
    Q_ASSERT(ok);
    ok = connect(undoStack, SIGNAL(redoTextChanged(QString)), this, SLOT(updateUndoRedoMenuText()));
    Q_ASSERT(ok);
    ok = connect(undoStack, SIGNAL(undoTextChanged(QString)), this, SLOT(updateUndoRedoMenuText()));
    Q_ASSERT(ok);
}

ProjectWindow::~ProjectWindow()
{
	delete ui;
    delete project;
}

void ProjectWindow::updateSampleRateUi()
{
    int projectRate = project->getSampleRate();
    foreach (QAction *action, sampleRateActions) {
        int actionRate = action->data().value<int>();
        action->setChecked(actionRate == projectRate);
    }
}

void ProjectWindow::changeSampleRate()
{
    QAction *action = (QAction *) sender();
    int rate = action->data().value<int>();
    if (rate == project->getSampleRate()) return;
    undoStack->push(new ChangeSampleRateCommand(rate, project, this));
    updateSampleRateUi();
}

void ProjectWindow::updateUndoRedoMenuText()
{
    ui->actionUndo->setText(tr("&Undo %1").arg(undoStack->undoText()));
    ui->actionRedo->setText(tr("&Redo %1").arg(undoStack->redoText()));
    ui->actionUndo->setEnabled(undoStack->canUndo());
    ui->actionRedo->setEnabled(undoStack->canRedo());
}

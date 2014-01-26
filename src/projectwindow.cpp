#include "projectwindow.h"
#include "ui_projectwindow.h"

#include "changesampleratecommand.h"
#include "changechannellayoutcommand.h"

#include "channel_layout.h"

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

static const int chLayoutCount = 27;
static const uint64_t chLayouts[chLayoutCount] = {
    GENESIS_CH_LAYOUT_MONO,
    GENESIS_CH_LAYOUT_STEREO,
    GENESIS_CH_LAYOUT_2POINT1,
    GENESIS_CH_LAYOUT_2_1,
    GENESIS_CH_LAYOUT_SURROUND,
    GENESIS_CH_LAYOUT_3POINT1,
    GENESIS_CH_LAYOUT_4POINT0,
    GENESIS_CH_LAYOUT_4POINT1,
    GENESIS_CH_LAYOUT_2_2,
    GENESIS_CH_LAYOUT_QUAD,
    GENESIS_CH_LAYOUT_5POINT0,
    GENESIS_CH_LAYOUT_5POINT1,
    GENESIS_CH_LAYOUT_5POINT0_BACK,
    GENESIS_CH_LAYOUT_5POINT1_BACK,
    GENESIS_CH_LAYOUT_6POINT0,
    GENESIS_CH_LAYOUT_6POINT0_FRONT,
    GENESIS_CH_LAYOUT_HEXAGONAL,
    GENESIS_CH_LAYOUT_6POINT1,
    GENESIS_CH_LAYOUT_6POINT1_BACK,
    GENESIS_CH_LAYOUT_6POINT1_FRONT,
    GENESIS_CH_LAYOUT_7POINT0,
    GENESIS_CH_LAYOUT_7POINT0_FRONT,
    GENESIS_CH_LAYOUT_7POINT1,
    GENESIS_CH_LAYOUT_7POINT1_WIDE,
    GENESIS_CH_LAYOUT_7POINT1_WIDE_BACK,
    GENESIS_CH_LAYOUT_OCTAGONAL,
    GENESIS_CH_LAYOUT_STEREO_DOWNMIX,
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

    for (int i = 0; i < chLayoutCount; i += 1) {
        uint64_t layout = chLayouts[i];
        int channelCount = genesis_get_channel_layout_nb_channels(layout);
        QAction *action = new QAction(tr("%1 (%2 channels)").arg(
                                          genesis_get_channel_layout_string(layout),
                                          QString::number(channelCount)), this);
        action->setData(QVariant((qlonglong)layout));
        action->setCheckable(true);
        channelLayoutActions.append(action);
        ok = connect(action, SIGNAL(triggered()), this, SLOT(changeChannelLayout()));
        Q_ASSERT(ok);
    }
    ui->menuChannelLayout->addActions(channelLayoutActions);
    updateChannelLayoutUi();

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

void ProjectWindow::updateChannelLayoutUi()
{
    uint64_t projectLayout = project->getChannelLayout();
    foreach (QAction *action, channelLayoutActions) {
        uint64_t actionLayout = action->data().value<uint64_t>();
        action->setChecked(actionLayout == projectLayout);
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

void ProjectWindow::changeChannelLayout()
{
    QAction *action = (QAction *) sender();
    uint64_t layout = action->data().value<uint64_t>();
    if (layout == project->getChannelLayout()) return;
    undoStack->push(new ChangeChannelLayoutCommand(layout, project, this));
    updateChannelLayoutUi();
}

void ProjectWindow::updateUndoRedoMenuText()
{
    ui->actionUndo->setText(tr("&Undo %1").arg(undoStack->undoText()));
    ui->actionRedo->setText(tr("&Redo %1").arg(undoStack->redoText()));
    ui->actionUndo->setEnabled(undoStack->canUndo());
    ui->actionRedo->setEnabled(undoStack->canRedo());
}

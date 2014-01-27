#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "lv2selectorwindow.h"
#include "playbackmodule.h"
#include "preferenceswindow.h"

#include "project.h"
#include "projectwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::createNewProject()
{
    Project *project = new Project();

    // add a PlaybackModule to the project so that we can hear the sound coming out
    PlaybackModule *playbackModule = new PlaybackModule(
                project->getSampleRate(), project->getChannelLayout(),
                PlaybackModule::defaultOutputDeviceIndex());
    project->addRootModule(playbackModule);

    ProjectWindow *window = new ProjectWindow(project, this);
    window->show();
}


void MainWindow::begin()
{
    PlaybackModule::initialize();
}

static Lv2SelectorWindow *lv2Window = NULL;

void MainWindow::on_actionLv2PluginBrowser_triggered()
{
	if (!lv2Window)
		lv2Window = new Lv2SelectorWindow(this);
	lv2Window->hide();
	lv2Window->show();
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

void MainWindow::on_actionNewProject_triggered()
{
    createNewProject();
}

static PreferencesWindow *prefWindow = NULL;

void MainWindow::on_actionPreferences_triggered()
{
    prefWindow = new PreferencesWindow(this);
    prefWindow->hide();
    prefWindow->show();
}

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "lv2selectorwindow.h"
#include "soundengine.h"
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

void MainWindow::begin()
{
    SoundEngine::initialize();
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
    Project *project = new Project();
    ProjectWindow *window = new ProjectWindow(project, this);
    window->show();
}

static PreferencesWindow *prefWindow = NULL;

void MainWindow::on_actionPreferences_triggered()
{
    prefWindow = new PreferencesWindow(this);
    prefWindow->hide();
    prefWindow->show();
}

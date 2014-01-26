#include "projectwindow.h"
#include "ui_projectwindow.h"

ProjectWindow::ProjectWindow(GenesisProject *project, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ProjectWindow),
    project(project)
{
	ui->setupUi(this);

    undoStack = new QUndoStack(this);

    undoView = new QUndoView(undoStack);
    ui->undoRedoDock->setWidget(undoView);

}

ProjectWindow::~ProjectWindow()
{
	delete ui;
    genesis_project_close(project);
}

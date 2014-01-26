#include "projectwindow.h"
#include "ui_projectwindow.h"

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

}

ProjectWindow::~ProjectWindow()
{
	delete ui;
    delete project;
}

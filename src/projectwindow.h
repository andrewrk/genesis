#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include <QMainWindow>
#include <QUndoStack>
#include <QUndoView>

#include "project.h"

namespace Ui {
class ProjectWindow;
}

class ProjectWindow : public QMainWindow
{
	Q_OBJECT

public:
    explicit ProjectWindow(Project *project, QWidget *parent = 0);
	~ProjectWindow();

private:
	Ui::ProjectWindow *ui;

    Project *project;

    QUndoStack *undoStack;
    QUndoView *undoView;

};

#endif // PROJECTWINDOW_H

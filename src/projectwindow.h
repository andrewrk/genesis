#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include <QMainWindow>
#include <QUndoStack>
#include <QUndoView>
#include <QList>

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


    void updateSampleRateUi();

private:
	Ui::ProjectWindow *ui;

    Project *project;

    QUndoStack *undoStack;
    QUndoView *undoView;

    QList<QAction *> sampleRateActions;


private slots:
    void changeSampleRate();

    void updateUndoRedoMenuText();

};

#endif // PROJECTWINDOW_H

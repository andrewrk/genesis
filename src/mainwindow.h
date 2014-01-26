#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void begin();

private slots:
	void on_actionLv2PluginBrowser_triggered();

	void on_actionQuit_triggered();

	void on_actionNewProject_triggered();

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

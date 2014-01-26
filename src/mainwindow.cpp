#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "lv2selectorwindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	ui->actionExit->setShortcut(QKeySequence(Qt::ALT, Qt::Key_F4));
}

MainWindow::~MainWindow()
{
	delete ui;
}

static Lv2SelectorWindow *lv2Window = NULL;

void MainWindow::on_actionLv2PluginBrowser_triggered()
{
	if (!lv2Window)
		lv2Window = new Lv2SelectorWindow(this);
	lv2Window->hide();
	lv2Window->show();
}

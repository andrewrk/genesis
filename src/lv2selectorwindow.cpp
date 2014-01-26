#include "lv2selectorwindow.h"
#include "ui_lv2selectorwindow.h"

Lv2SelectorWindow::Lv2SelectorWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::Lv2SelectorWindow)
{
	ui->setupUi(this);
}

Lv2SelectorWindow::~Lv2SelectorWindow()
{
	delete ui;
}

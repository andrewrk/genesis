#ifndef LV2SELECTORWINDOW_H
#define LV2SELECTORWINDOW_H

#include <QDialog>

namespace Ui {
class Lv2SelectorWindow;
}

class Lv2SelectorWindow : public QDialog
{
	Q_OBJECT

public:
	explicit Lv2SelectorWindow(QWidget *parent = 0);
	~Lv2SelectorWindow();

private:
	Ui::Lv2SelectorWindow *ui;
};

#endif // LV2SELECTORWINDOW_H

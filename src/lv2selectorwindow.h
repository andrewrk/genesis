#ifndef LV2SELECTORWINDOW_H
#define LV2SELECTORWINDOW_H

#include <QDialog>
#include <QListWidgetItem>


#include "lilv/lilv.h"

namespace Ui {
class Lv2SelectorWindow;
}

class Lv2SelectorWindow : public QDialog
{
	Q_OBJECT

public:
	explicit Lv2SelectorWindow(QWidget *parent = 0);
	~Lv2SelectorWindow();

private slots:
	void on_pluginList_itemSelectionChanged();

    void on_pushButton_clicked();

    void on_filterText_textChanged(const QString &arg1);

private:
	Ui::Lv2SelectorWindow *ui;

	LilvWorld *world;

    void listPlugins(QString filter);


};

#endif // LV2SELECTORWINDOW_H

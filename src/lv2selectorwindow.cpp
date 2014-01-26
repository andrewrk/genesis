#include "lv2selectorwindow.h"
#include "ui_lv2selectorwindow.h"


Lv2SelectorWindow::Lv2SelectorWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::Lv2SelectorWindow)
{
	ui->setupUi(this);

	world = lilv_world_new();
	lilv_world_load_all(world);

	const LilvPlugins *plugins = lilv_world_get_all_plugins(world);
	for (LilvIter* iter = lilv_plugins_begin(plugins);
		 !lilv_plugins_is_end(plugins, iter);
		 iter = lilv_plugins_next(plugins, iter))
	{
		const LilvPlugin *p = lilv_plugins_get(plugins, iter);
		LilvNode *n = lilv_plugin_get_name(p);
		QListWidgetItem *item = new QListWidgetItem(lilv_node_as_string(n), ui->pluginList);
		item->setData(Qt::UserRole, qVariantFromValue((void *)p));
		lilv_node_free(n);
	}
}

Lv2SelectorWindow::~Lv2SelectorWindow()
{
	delete ui;
	lilv_world_free(world);
}

static QString nodeToString(LilvNode *node) {
	QString str(lilv_node_as_string(node));
	if (node)
		lilv_node_free(node);
	return str;
}

static QString nodeToString(const LilvNode *node) {
	return QString(lilv_node_as_string(node));
}

void Lv2SelectorWindow::on_pluginList_itemSelectionChanged()
{
	QList<QListWidgetItem *> selection = ui->pluginList->selectedItems();
	if (selection.size() == 0) {
		ui->description->setText("");
		return;
	} else if (selection.size() > 1) {
		return;
	}

	QListWidgetItem *item = selection.at(0);
	const LilvPlugin *p = reinterpret_cast<LilvPlugin *>(item->data(Qt::UserRole).value<void *>());
	ui->description->setText(QString(
								 "URI: %1\n"
								 "Project: %2\n"
								 "Author: %3 <%4>\n"
								 "Homepage: %5")
							.arg(
								 nodeToString(lilv_plugin_get_uri(p)),
								 nodeToString(lilv_plugin_get_project(p)),
								 nodeToString(lilv_plugin_get_author_name(p)),
								 nodeToString(lilv_plugin_get_author_email(p)),
								 nodeToString(lilv_plugin_get_author_homepage(p))));
}

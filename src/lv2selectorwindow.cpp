#include "lv2selectorwindow.h"
#include "ui_lv2selectorwindow.h"

#include <QVector>
#include <QString>

static QVector<QString> mapped_uris;

static LV2_URID mapUri(LV2_URID_Map_Handle handle, const char* uri) {
    QString str(uri);
    int index = mapped_uris.indexOf(str);

    if (index >= 0)
        return index;

    index = mapped_uris.size();
    mapped_uris.append(str);
    return index;
}

static const char* unmapUri(LV2_URID_Unmap_Handle handle, LV2_URID urid) {
    if (urid >= (unsigned)mapped_uris.size())
        return NULL;
    return mapped_uris.at(urid).toStdString().c_str();
}

static LV2_URID_Unmap unmap_struct = {
    NULL,
    unmapUri,
};

static LV2_URID_Map map_struct = {
    NULL,
    mapUri,
};

static const LV2_Feature map_feature = {
    "http://lv2plug.in/ns/ext/urid#map",
    &map_struct,
};

static const LV2_Feature unmap_feature = {
    "http://lv2plug.in/ns/ext/urid#unmap",
    &unmap_struct,
};

static const LV2_Feature *supported_features[] = {
    &map_feature,
    &unmap_feature,
    NULL,
};


Lv2SelectorWindow::Lv2SelectorWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::Lv2SelectorWindow)
{
	ui->setupUi(this);

	world = lilv_world_new();
	lilv_world_load_all(world);

    listPlugins("");

}

Lv2SelectorWindow::~Lv2SelectorWindow()
{
	delete ui;
	lilv_world_free(world);
}

static QString nodeToString(LilvNode *node) {
	QString str(lilv_node_as_string(node));
    lilv_node_free(node);
	return str;
}

static QString nodeToString(const LilvNode *node) {
	return QString(lilv_node_as_string(node));
}

static QString nodeToUri(const LilvNode *node) {
    return QString(lilv_node_as_uri(node));
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
    LilvNodes* required_features = lilv_plugin_get_required_features(p);
    QStringList features;
    for (LilvIter* iter = lilv_nodes_begin(required_features);
         !lilv_nodes_is_end(required_features, iter);
         iter = lilv_nodes_next(required_features, iter))
    {

        const LilvNode *feature = lilv_nodes_get(required_features, iter);

        features.append(nodeToUri(feature));
    }
    QStringList ports;
    int port_count = lilv_plugin_get_num_ports(p);
    for (int i = 0; i < port_count; i += 1) {
        const LilvPort *port = lilv_plugin_get_port_by_index(p, i);
        const LilvNodes *classes = lilv_port_get_classes(p, port);
        QStringList classes_strings;
        for (LilvIter* iter = lilv_nodes_begin(classes);
             !lilv_nodes_is_end(classes, iter);
             iter = lilv_nodes_next(classes, iter))
        {
            classes_strings.append(nodeToString(lilv_nodes_get(classes, iter)));
        }
        ports.append(QString(
                         "symbol: %1 "
                         "classes: (%2) ").arg(
                         nodeToString(lilv_port_get_symbol(p, port)),
                         classes_strings.join(", ")));
    }

    ui->description->setText(QString(
                                 "URI: %1\n"
                                 "Project: %2\n"
                                 "Author: %3 <%4>\n"
                                 "Homepage: %5\n"
                                 "Features: %6\n"
                                 "Ports: %7")
                            .arg(
                                 nodeToString(lilv_plugin_get_uri(p)),
                                 nodeToString(lilv_plugin_get_project(p)),
                                 nodeToString(lilv_plugin_get_author_name(p)),
                                 nodeToString(lilv_plugin_get_author_email(p)),
                                 nodeToString(lilv_plugin_get_author_homepage(p)),
                                 features.join("\n"),
                                 ports.join("\n")));

}

void Lv2SelectorWindow::on_pushButton_clicked()
{
    QList<QListWidgetItem *> selection = ui->pluginList->selectedItems();
    QListWidgetItem *item = selection.at(0);
    const LilvPlugin *p = reinterpret_cast<LilvPlugin *>(item->data(Qt::UserRole).value<void *>());
    LilvInstance *instance = lilv_plugin_instantiate(p, 44100, supported_features);

}

void Lv2SelectorWindow::on_filterText_textChanged(const QString &arg1)
{
    listPlugins(arg1);
}

void Lv2SelectorWindow::listPlugins(QString filter)
{
    ui->pluginList->clear();
    const LilvPlugins *plugins = lilv_world_get_all_plugins(world);
    for (LilvIter* iter = lilv_plugins_begin(plugins);
         !lilv_plugins_is_end(plugins, iter);
         iter = lilv_plugins_next(plugins, iter))
    {
        const LilvPlugin *p = lilv_plugins_get(plugins, iter);
        LilvNode *n = lilv_plugin_get_name(p);
        QString name = nodeToString(n);
        if (name.toLower().indexOf(filter.toLower()) >= 0) {
            QListWidgetItem *item = new QListWidgetItem(name, ui->pluginList);
            item->setData(Qt::UserRole, qVariantFromValue((void *)p));
        }
    }

}

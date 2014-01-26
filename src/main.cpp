#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{


	QApplication a(argc, argv);
	a.setOrganizationName("andrewrk");
	a.setOrganizationDomain("andrewkelley.me");
	a.setApplicationName("genesis");

	MainWindow w;
	w.show();
	w.begin();

	return a.exec();
}

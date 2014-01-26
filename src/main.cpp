#include "mainwindow.h"
#include <QApplication>

static int usage(const char *arg0) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Available options:\n"
            "  --help        show this text\n"
            "  --version     print version and exit\n"
            , arg0);
    return -1;
}

static const char *appName = "genesis";
static const char *orgName = "andrewrk";
static const char *orgDomain = "andrewkelley.me";

int main(int argc, char *argv[])
{
    const char *arg0 = argv[0];
    for (int i = 1; i < argc; i += 1) {
        const char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            arg += 2;
            if (strcmp(arg, "version") == 0) {
                printf("%s version 0.0.0\n", appName);
                return 0;
            } else if (strcmp(arg, "help") == 0) {
                return usage(arg0);
            }
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", arg);
            return usage(arg0);
        }
    }

    QApplication a(argc, argv);
    QApplication::setOrganizationName(orgName);
    QApplication::setOrganizationDomain(orgDomain);
    QApplication::setApplicationName(appName);

	MainWindow w;
	w.show();
	w.begin();

	return a.exec();
}

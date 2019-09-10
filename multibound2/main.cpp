#include "mainwindow.h"
#include <QApplication>

#include "data/config.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    MultiBound::Config::load();

    MultiBound::MainWindow w;
    w.show();

    return a.exec();
}

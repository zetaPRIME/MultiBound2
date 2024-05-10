#include "util.h" // part of utility header

// version tracking and update checking

#include <QCoreApplication>
#include <QDir>
#include <QDateTime>

const QString MultiBound::Util::version = qs("v0.5.5");

// somewhat naive github version checking for Windows only
void MultiBound::Util::checkForUpdates() {
    //auto binDate = QFileInfo(QCoreApplication::applicationFilePath()).birthTime();
}

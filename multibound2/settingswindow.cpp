#include "settingswindow.h"
#include "ui_settingswindow.h"

using MultiBound::SettingsWindow;

QPointer<SettingsWindow> SettingsWindow::instance;

SettingsWindow::SettingsWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsWindow) {
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    if (instance) instance->close();
    instance = this;
}

SettingsWindow::~SettingsWindow() {
    delete ui;
}

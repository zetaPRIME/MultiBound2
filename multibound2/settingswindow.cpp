#include "settingswindow.h"
#include "ui_settingswindow.h"

#include "mainwindow.h"

#include "data/config.h"

#include <QTimer>

using MultiBound::SettingsWindow;

QPointer<SettingsWindow> SettingsWindow::instance;

SettingsWindow::SettingsWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsWindow) {
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    if (instance) instance->close();
    instance = this;

    connect(this, &QDialog::accepted, this, &SettingsWindow::apply);

    // set up initial values
    ui->openSBEnabled->setChecked(Config::openSBEnabled);
    ui->openSBUseCIBuild->setChecked(Config::openSBUseCIBuild);
    ui->openSBUseDevBranch->setChecked(Config::openSBUseDevBranch);

    ui->steamcmdEnabled->setChecked(Config::steamcmdEnabled);
    ui->steamcmdUpdateSteamMods->setChecked(Config::steamcmdUpdateSteamMods);

    //ui->workshopDecryptionKey->setText(Config::workshopDecryptionKey);

    // and some interactions
    ui->steamcmdUpdateSteamMods->setEnabled(ui->steamcmdEnabled->isChecked());
    connect(ui->steamcmdEnabled, &QCheckBox::stateChanged, this, [this] {
        ui->steamcmdUpdateSteamMods->setEnabled(ui->steamcmdEnabled->isChecked());
    });

    connect(ui->btnUpdateCI, &QPushButton::clicked, this, [this] {
        if (!MainWindow::instance->isInteractive()) return; // guard
        QPointer<QPushButton> btn = ui->btnUpdateCI;
        btn->setEnabled(false);
        MainWindow::instance->updateCIBuild();
        if (btn) btn->setEnabled(true);
    });
}

SettingsWindow::~SettingsWindow() {
    delete ui;
}

void SettingsWindow::apply() {
    bool enablingOSB = ui->openSBEnabled->isChecked() && !Config::openSBEnabled;

    Config::openSBEnabled = ui->openSBEnabled->isChecked();
    Config::openSBUseCIBuild = ui->openSBUseCIBuild->isChecked();
    Config::openSBUseDevBranch = ui->openSBUseDevBranch->isChecked();

    Config::steamcmdEnabled = ui->steamcmdEnabled->isChecked();
    Config::steamcmdUpdateSteamMods = ui->steamcmdUpdateSteamMods->isChecked();

    //Config::workshopDecryptionKey = ui->workshopDecryptionKey->text().toLower().trimmed();

    Config::save();
    emit MainWindow::instance->refreshSettings();

    if (enablingOSB) QTimer::singleShot(0, MainWindow::instance, [] {
        MainWindow::instance->checkUpdates(true);
    });
}

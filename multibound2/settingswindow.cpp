#include "settingswindow.h"
#include "ui_settingswindow.h"

#include "mainwindow.h"

#include "data/config.h"

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
    ui->openSBUseDevBranch->setChecked(Config::openSBUseDevBranch);

    ui->steamcmdEnabled->setChecked(Config::steamcmdEnabled);
    ui->steamcmdUpdateSteamMods->setChecked(Config::steamcmdUpdateSteamMods);

    //ui->workshopDecryptionKey->setText(Config::workshopDecryptionKey);

    // and some interactions
    ui->steamcmdUpdateSteamMods->setEnabled(ui->steamcmdEnabled->isChecked());
    connect(ui->steamcmdEnabled, &QCheckBox::stateChanged, this, [this] {
        ui->steamcmdUpdateSteamMods->setEnabled(ui->steamcmdEnabled->isChecked());
    });
}

SettingsWindow::~SettingsWindow() {
    delete ui;
}

void SettingsWindow::apply() {
    Config::openSBEnabled = ui->openSBEnabled->isChecked();
    Config::openSBUseDevBranch = ui->openSBUseDevBranch->isChecked();

    Config::steamcmdEnabled = ui->steamcmdEnabled->isChecked();
    Config::steamcmdUpdateSteamMods = ui->steamcmdUpdateSteamMods->isChecked();

    //Config::workshopDecryptionKey = ui->workshopDecryptionKey->text().toLower().trimmed();

    Config::save();
    emit MainWindow::instance->refreshSettings();
}

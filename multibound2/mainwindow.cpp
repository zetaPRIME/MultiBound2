#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "data/config.h"
#include "data/instance.h"

#include "util.h"

#include <memory>

#include <QDebug>

#include <QDir>
#include <QFile>
#include <QJsonDocument>

using MultiBound::MainWindow;
using MultiBound::Instance;

namespace {
    inline Instance* instanceFromItem(QListWidgetItem* itm) { return static_cast<Instance*>(itm->data(Qt::UserRole).value<void*>()); }
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->launchButton, &QPushButton::pressed, this, [this] { launch(); });
    connect(ui->instanceList, &QListWidget::doubleClicked, this, [this](const QModelIndex& ind) { if (ind.isValid()) launch(); });

    connect(ui->instanceList, &QListWidget::customContextMenuRequested, this, [this] {
        // testing
        if (auto sel = ui->instanceList->selectedItems(); sel.count() > 0) {
            auto csf = instanceFromItem(sel[0]);
            setEnabled(false);
            Util::updateFromWorkshop(csf);
            setEnabled(true);
            refresh();
        }
    });

    refresh();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::refresh() {
    QString selName;
    if (auto sel = ui->instanceList->selectedItems(); sel.count() > 0) selName = sel[0]->text();

    ui->instanceList->clear();

    // load instances
    instances.clear();
    QDir d(Config::instanceRoot);
    auto lst = d.entryList(QDir::Dirs);
    instances.reserve(static_cast<size_t>(lst.count()));
    for (auto idir : lst) {
        auto inst = Instance::loadFrom(idir);
        if (!inst) continue;
        instances.push_back(inst);
        auto itm = new QListWidgetItem(inst->displayName(), ui->instanceList);
        itm->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(inst.get())));//reinterpret_cast<qintptr>(inst.get()));
    }

}

void MainWindow::launch() {
    if (auto sel = ui->instanceList->selectedItems(); sel.count() > 0) {
        auto csf = instanceFromItem(sel[0]);
        hide();
        csf->launch();
        show();
    }
}












//

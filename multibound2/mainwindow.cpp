#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "settingswindow.h"

#include "data/config.h"
#include "data/instance.h"

#include "util.h"

#include "uitools.h"

#include <memory>

#include <QDebug>
#include <QTimer>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include <QMenu>
#include <QShortcut>
#include <QClipboard>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>

#include <QDesktopServices>
#include <QUrl>

using MultiBound::MainWindow;
using MultiBound::Instance;

QPointer<MainWindow> MainWindow::instance;

namespace { // clazy:excludeall=non-pod-global-static
    inline Instance* instanceFromItem(QListWidgetItem* itm) { return static_cast<Instance*>(itm->data(Qt::UserRole).value<void*>()); }

    auto checkableEventFilter = new MultiBound::CheckableEventFilter();
    void bindConfig(QAction* a, bool& f) {
        a->setChecked(f);
        QObject::connect(a, &QAction::triggered, a, [&f](bool b) {
            f = b;
            MultiBound::Config::save();
        });
        QObject::connect(MultiBound::MainWindow::instance, &MultiBound::MainWindow::refreshSettings, a, [a, &f] {
            a->setChecked(f);
        });
    }

    bool firstShow = true;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {
    ui->setupUi(this);

    instance = this;

    connect(ui->launchButton, &QPushButton::pressed, this, [this] { launch(); });
    connect(ui->instanceList, &QListWidget::doubleClicked, this, [this](const QModelIndex& ind) { if (ind.isValid()) launch(); });
    connect(new QShortcut(QKeySequence("Return"), ui->instanceList), &QShortcut::activated, this, [this] { launch(); });

    // hook up menus
    for (auto m : ui->menuBar->children()) if (qobject_cast<QMenu*>(m)) m->installEventFilter(checkableEventFilter);
    ui->actionRefresh->shortcuts() << QKeySequence("F5");
    connect(ui->actionRefresh, &QAction::triggered, this, [this] { refresh(); });
    connect(ui->actionExit, &QAction::triggered, this, [this] { close(); });
    connect(ui->actionFromCollection, &QAction::triggered, this, [this] { newFromWorkshop(); });

    // settings
    bindConfig(ui->actionUpdateSteamMods, Config::steamcmdUpdateSteamMods);
    connect(ui->actionSettings, &QAction::triggered, this, [ ] {
        if (SettingsWindow::instance) {
            SettingsWindow::instance->show();
            SettingsWindow::instance->raise();
            SettingsWindow::instance->activateWindow();
        } else (new SettingsWindow)->show();
    });

    // and context menu
    connect(ui->instanceList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pt) {
        auto m = new QMenu(this);

        if (auto i = ui->instanceList->itemAt(pt); i) {
            auto inst = instanceFromItem(i);
            m->addAction(qs("&Launch instance"), this, [this, inst] { launch(inst); });
            if (auto id = inst->workshopId(); !id.isEmpty()) {
                m->addAction(qs("&Update from Workshop collection"), m, [this, inst] { updateFromWorkshop(inst); });
                m->addAction(qs("Open &Workshop link..."), m, [id] {
                    QDesktopServices::openUrl(QUrl(Util::workshopLinkFromId(id)));
                });
            }
            m->addAction(qs("&Open instance directory..."), m, [inst] { QDesktopServices::openUrl(QUrl::fromLocalFile(inst->path)); });
            m->addSeparator();
        }

        // carry over options from menu bar
        m->addMenu(ui->menuNewInstance);
        m->addSeparator();
        m->addAction(ui->actionRefresh);

        m->setAttribute(Qt::WA_DeleteOnClose);
        m->popup(ui->instanceList->mapToGlobal(pt));
    });

    connect(new QShortcut(QKeySequence(QKeySequence::Paste), this), &QShortcut::activated, this, [this] {
        auto txt = QApplication::clipboard()->text();
        auto id = Util::workshopIdFromLink(txt);
        if (id.isEmpty()) return;
        newFromWorkshop(id);
    });

    if (auto fi = QFileInfo(Config::starboundPath); !fi.isFile() || !fi.isExecutable()) {
        // prompt for working file path if executable missing
        auto d = fi.dir();
        if (!d.exists()) { // walk up until dir exists
            auto cd = qs("..");
            while (!d.cd(cd)) cd.append(qs("/.."));
        }

        auto fn = QFileDialog::getOpenFileName(this, qs("Locate Starbound executable..."), d.absolutePath());
        if (fn.isEmpty()) {
            QMetaObject::invokeMethod(this, [this] { close(); QApplication::quit(); }, Qt::ConnectionType::QueuedConnection);
        } else { // set new path and refresh
            Config::starboundPath = fn;
            Config::save();
            Config::load();
        }
    }

    refresh();

    Util::updateStatus = [this] (QString msg) {
        ui->statusBar->setVisible(!msg.isEmpty()); // only show status bar when a message is shown
        ui->statusBar->showMessage(msg);
    };

    Util::updateStatus("");

    // add version to title
    setWindowTitle(QString("%1 %2").arg(windowTitle(), Util::version));
}

MainWindow::~MainWindow() {
    delete ui;
    if (SettingsWindow::instance) SettingsWindow::instance->close();
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);

    if (firstShow) { // only check for updates and such on launch, not on return from game
        firstShow = false;

        // execute the rest after showing
        QTimer::singleShot(0, this, [this] {
            if (!Config::openSBOffered) {
                auto res = QMessageBox::question(this, "OpenStarbound integration", "Would you like to enable OpenStarbound support? OpenStarbound contains various performance and quality-of-life improvements over the vanilla client, and enables mod features which are simply not possible otherwise.\n\n(MultiBound will handle installing and updating its own copy of OpenStarbound.)", QMessageBox::Yes, QMessageBox::No);
                Config::openSBEnabled = res == QMessageBox::Yes;
                Config::openSBOffered = true;
                Config::save();
            }

            // then check updates
            checkUpdates();
        });
    }
}

bool MainWindow::isInteractive() { return ui->centralWidget->isEnabled(); }
void MainWindow::setInteractive(bool b) {
    ui->menuBar->setEnabled(b);
    ui->centralWidget->setEnabled(b);

    if (b) unsetCursor();
    else setCursor(Qt::WaitCursor);
}

void MainWindow::checkUpdates(bool osbOnly) {
    if (!osbOnly) {
        // TODO figure out update check on windows
        setInteractive(false);
        Util::checkForUpdates();
        setInteractive(true);
    }

    // OpenStarbound update check
    if (Config::openSBEnabled) {
        setInteractive(false);
        Util::openSBCheckForUpdates();
        setInteractive(true);
    }
}

void MainWindow::updateCIBuild() {
    setInteractive(false);
    Util::openSBUpdateCI();
    setInteractive(true);
}

void MainWindow::refresh(const QString& focusPath) {
    QString selPath;
    if (!focusPath.isEmpty()) selPath = focusPath;
    else if (auto sel = ui->instanceList->selectedItems(); sel.count() > 0) selPath = instanceFromItem(sel[0])->path;

    ui->instanceList->clear();

    QListWidgetItem* selItm = nullptr;

    // load instances
    instances.clear();
    QDir d(Config::instanceRoot);
    auto lst = d.entryList(QDir::Dirs);
    instances.reserve(static_cast<size_t>(lst.count()));
    for (auto& idir : lst) {
        auto inst = Instance::loadFrom(idir);
        if (!inst) continue;
        instances.push_back(inst);
        auto itm = new QListWidgetItem(inst->displayName(), ui->instanceList);
        itm->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(inst.get())));//reinterpret_cast<qintptr>(inst.get()));
        if (inst->path == selPath) selItm = itm;
    }

    if (selItm) {
        ui->instanceList->setCurrentRow(ui->instanceList->row(selItm));
        ui->instanceList->scrollToItem(selItm);
    }

}

void MainWindow::launch(Instance* inst) {
    if (!inst) { inst = selectedInstance(); if (!inst) return; }
    hide();
    inst->launch();
    show();
}

void MainWindow::updateFromWorkshop(Instance* inst) {
    if (!inst) { inst = selectedInstance(); if (!inst) return; }
    setInteractive(false);
    Util::updateFromWorkshop(inst);
    setInteractive(true);
    refresh(inst->path);
}

void MainWindow::newFromWorkshop(const QString& id_) {
    auto id = id_;
    if (id.isEmpty()) { // prompt
        bool ok = false;
        auto link = QInputDialog::getText(this, qs("Enter collection link"), qs("Enter a link to a Steam Workshop collection:"), QLineEdit::Normal, QString(), &ok);
        id = Util::workshopIdFromLink(link);
        if (!ok || id.isEmpty()) return;
    }

    setInteractive(false);
    auto inst = findWorkshopId(id);
    if (inst) {
        return updateFromWorkshop(inst);
    } else {
        auto ni = std::make_shared<Instance>();
        ni->json = QJsonDocument::fromJson(qs("{\"info\" : { \"workshopId\" : \"%1\" }, \"savePath\" : \"inst:/storage/\", \"assetSources\" : [ \"inst:/mods/\" ] }").arg(id).toUtf8()).object();
        Util::updateFromWorkshop(ni.get(), false);
        if (!ni->displayName().isEmpty()) {
            bool ok = false;
            auto name = QInputDialog::getText(this, qs("Directory name?"), qs("Enter a directory name for your new instance:"), QLineEdit::Normal, ni->displayName(), &ok);
            if (ok && !name.isEmpty()) {
                auto path = Util::splicePath(Config::instanceRoot, name);
                if (QDir(path).exists()) {
                    QMessageBox::warning(this, qs("Error creating instance"), qs("Directory already exists."));
                } else {
                    ni->path = path;
                    ni->save();
                    refresh(ni->path);
                }
            }
        }
    }
    setInteractive(true);
}

Instance* MainWindow::selectedInstance() {
    if (auto sel = ui->instanceList->selectedItems(); sel.count() > 0) return instanceFromItem(sel[0]);
    return nullptr;
}

Instance* MainWindow::findWorkshopId(const QString& id) {
    if (id.isEmpty()) return nullptr;
    for (auto& i : instances) if (i->workshopId() == id) return i.get();
    return nullptr;
}












//

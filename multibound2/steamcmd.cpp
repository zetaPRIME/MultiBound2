#include "util.h" // part of utility header

#include "data/config.h"
#include "data/instance.h"

#include "inclib/vdf_parser.hpp"

#include <QJsonDocument>
#include <QJsonArray>

#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QBoxLayout>
#include <QLabel>
#include <QMessageBox>

#include <QMetaObject>
#include <QProcess>
#include <QFile>
#include <QStandardPaths>
#include <QEventLoop>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDataStream>

#include <QSet>

namespace { // clazy:excludeall=non-pod-global-static
    const auto updMsg = qs("Updating mods...");
    QProcess* scp;
    QEventLoop ev;

    bool scFail = true;

    QString scConfigPath;

    // these are only saved for the session
    QString triedUserName;
    QString triedPassword;
}

bool MultiBound::Util::initSteamCmd() {
    if (!Config::steamcmdEnabled) return false; // don't bother if it's disabled
    if (scp) {
        if (scFail) return false; // could not init steamcmd
        return true; // already initialized
    }

    scp = new QProcess();
    QObject::connect(scp, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &ev, &QEventLoop::quit);
#if defined(Q_OS_WIN)
    QDir scd(Config::configPath);
    scd.mkpath("steamcmd"); // make sure it exists
    scd.cd("steamcmd");

    if (!scd.exists("steamcmd.exe")) { // download and install if not present
        updateStatus(qs("Downloading steamcmd..."));
        scp->start("powershell", QStringList() << "curl -o"<< scd.absoluteFilePath("steamcmd.zip") << "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip");
        auto scz = scd.absoluteFilePath("steamcmd.zip");

        ev.exec();
        scp->start("powershell", QStringList() << "Expand-Archive"<< "-DestinationPath" << scd.absolutePath() << "-LiteralPath" << scz);
        ev.exec();
        if (scp->exitCode() != 0) return false; // extraction failed for some reason or another, abort

        // finish installing
        updateStatus(qs("Installing steamcmd..."));
        scp->setWorkingDirectory(scd.absolutePath());
        scp->start(scd.absoluteFilePath("steamcmd.exe"), QStringList() << "+exit");
        ev.exec();

        scp->readAll(); // and clean up buffer
    }
    scp->setProgram(scd.absoluteFilePath("steamcmd.exe"));
    scp->setWorkingDirectory(scd.absolutePath());
    scConfigPath = Util::splicePath(scd.absolutePath(), "/config/config.vdf");
#else
    // test with standard "which" command on linux and mac
    if (scp->execute("which", QStringList() << "steamcmd") == 0) { // use system steamcmd if present
        scp->readAll(); // clear buffer
        scp->setProgram("steamcmd");
        QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        scConfigPath = Util::splicePath(home, "/.steam/steam/config/config.vdf");
    } else { // use our own copy, and install automatically if necessary
        scp->readAll(); // clear buffer

        QDir scd(Config::configPath);
        scd.mkpath("steamcmd"); // make sure it exists
        scd.cd("steamcmd");

        if (!scd.exists("steamcmd.sh")) { // install local copy
            updateStatus(qs("Downloading steamcmd..."));
#if defined(Q_OS_MACOS)
            auto scUrl = qs("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_osx.tar.gz");
#else
            auto scUrl = qs("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz");
#endif
            scp->setWorkingDirectory(scd.absolutePath());
            scp->start("bash", QStringList() << "-c" << qs("curl -sqL \"%1\" | tar -xzvf -").arg(scUrl));
            ev.exec();
            if (scp->exitCode() != 0) return false; // something went wrong, abort

            // finish installing
            updateStatus(qs("Installing steamcmd..."));
            scp->setWorkingDirectory(scd.absolutePath());
            scp->start(scd.absoluteFilePath("steamcmd.sh"), QStringList() << "+exit");
            ev.exec();

            scp->readAll(); // and clean up buffer
        }
        scp->setProgram(scd.absoluteFilePath("steamcmd.sh"));
        scp->setWorkingDirectory(scd.absolutePath());
        scConfigPath = Util::splicePath(scd.absolutePath(), "/config/config.vdf");
    }
#endif
    if (!scConfigPath.isEmpty() && !Config::workshopDecryptionKey.isEmpty()) {
        QDir cpd(scConfigPath);
        cpd.cdUp();
        cpd.mkpath(".");

        { QFile f(scConfigPath); f.open(QFile::ReadWrite); } // ensure the file exists
        std::ifstream ifs(scConfigPath.toStdString());
        auto doc = tyti::vdf::read(ifs);
        ifs.close();

        doc.set_name("InstallConfigStore"); // ensure root name is correct

        auto depot = Util::vdfPath(&doc, QStringList() << "Software" << "Valve" << "Steam" << "depots" << "211820", true);
        depot->add_attribute("DecryptionKey", Config::workshopDecryptionKey.toStdString());

        std::ofstream ofs(scConfigPath.toStdString());
        tyti::vdf::write(ofs, doc);
        ofs.flush();
        ofs.close();
    }
    scFail = false;
    return true;
}

void MultiBound::Util::updateMods(MultiBound::Instance* inst) {
    if (!initSteamCmd()) return;

    updateStatus(updMsg);

    auto& json = inst->json;

    QSet<QString> workshop;
    QSet<QString> workshopExclude;

    auto sources = json["assetSources"].toArray();
    for (auto s : sources) {
        if (s.isObject()) {
            auto src = s.toObject();
            auto type = src["type"].toString();
            if (type == "mod") {
                if (auto id = src["workshopId"]; id.isString()) workshop.insert(id.toString());
            } else if (type == "workshopAuto") {
                if (auto id = src["id"]; id.isString()) workshop.insert(id.toString());
            } else if (type == "workshopExclude") {
                if (auto id = src["id"]; id.isString()) workshopExclude.insert(id.toString());
            }
        }
    }

    int wsc = 0;

    bool ws = !Config::workshopRoot.isEmpty(); // workshop active; valid workshop root?
    bool wsUpd = Config::steamcmdUpdateSteamMods; // should update workshop-subscribed mods?

    QString wsScript, dlScript;
    QTextStream wss(&wsScript), dls(&dlScript);

    if (ws) { // if valid workshop, force proper root in case steamcmd defaults are incorrect
        QDir wsr(Config::workshopRoot);
        for (int i = 0; i < 4; i++) wsr.cdUp();
        wss << qs("force_install_dir ") << wsr.absolutePath() << qs("\n");
    }
    wss << qs("login anonymous\n"); // log in after forcing install dir

    dls << qs("force_install_dir ") << Config::steamcmdDLRoot << qs("\n");
    dls << qs("login anonymous\n");

    auto eh = qs("workshop_download_item 211820 ");
    for (auto& id : workshop) {
        if (!workshopExclude.contains(id)) {
            if (ws && QDir(Config::workshopRoot).exists(id)) {
                if (wsUpd) { wss << eh << id << qs("\n"); wsc++; }
            } else {dls << eh << id << qs("\n"); wsc++; }
        }
    }

    QString endcap = qs("_dummy\n"); // force last download attempt to fully print
    wss << endcap;
    wss.flush();
    dls << endcap;
    dls.flush();

    QString scriptPath = QDir(Config::steamcmdDLRoot).filePath("steamcmd.txt");
    {
        QFile sf(scriptPath);
        sf.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
        (QTextStream(&sf)) << wsScript;
        sf.close();
    }
    QString scriptPath2 = QDir(Config::steamcmdDLRoot).filePath("steamcmd2.txt");
    {
        QFile sf(scriptPath2);
        sf.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
        (QTextStream(&sf)) << dlScript;
        sf.close();
    }

    std::unique_ptr<QObject> ctx(new QObject());

    int wsp = 0;
    updateStatus(qs("%1 (%2/%3)").arg(updMsg).arg(wsp).arg(wsc));
    QObject::connect(scp, &QProcess::readyRead, ctx.get(), [wsc, &wsp, &ctx] { // clazy:exclude=lambda-in-connect
        while (scp->canReadLine()) {
            QString l = scp->readLine();
            //qDebug() << l;
            if (l.startsWith(qs("Success. Downloaded item"))) {
                wsp++;
                updateStatus(qs("%1 (%2/%3)").arg(updMsg).arg(wsp).arg(wsc));
            } else if (l.contains("Missing decryption key") || l.contains("failed (Failure).")) {
                ctx.reset();
                scp->close(); // abort
                return;
            }
        }
    });

    /* / run workshop-update script */ {
        QStringList args;
        args << "+runscript" << scriptPath << "+quit";
        scp->setArguments(args);
        scp->start();
        ev.exec();
    } /* and then the custom-download one */ if (ctx) {
        QStringList args;
        args << "+runscript" << scriptPath2 << "+quit";
        scp->setArguments(args);
        scp->start();
        ev.exec();
    }

    if (!ctx) { // missing decryption key
        QMessageBox::critical(nullptr, " ", "Failed to download one or more mods. This may be due to Steam maintenance, or due to a missing decryption key. (If you have the Steam version of Starbound, you should have this if your machine has ever downloaded a Workshop mod.)");
        //if (setUpDecryptionKey()) updateMods(inst);
    }
}

bool MultiBound::Util::setUpDecryptionKey() {
    if (!initSteamCmd()) return false;

    std::unique_ptr<QDialog> dlg(new QDialog());
    dlg->setModal(true);
    dlg->setWindowTitle("Log into Steam");

    auto dl = new QVBoxLayout();
    dlg->setLayout(dl);

    auto usnl = new QHBoxLayout();
    dl->addLayout(usnl);
    auto usnc = new QLabel("Username");
    usnl->addWidget(usnc);
    auto usn = new QLineEdit(triedUserName);
    usnl->addWidget(usn);

    auto pwl = new QHBoxLayout();
    dl->addLayout(pwl);
    auto pwc = new QLabel("Password");
    pwl->addWidget(pwc);
    auto pw = new QLineEdit(triedPassword);
    pwl->addWidget(pw);
    pw->setEchoMode(QLineEdit::Password);

    auto lw = 64;
    usnc->setFixedWidth(lw);
    pwc->setFixedWidth(lw);

    auto inf = new QLabel("In order to download mods, steamcmd needs to obtain the decryption key by logging into a Steam account that owns Starbound. You should only need to do this once.\n\nYour login information is kept only for this session, and is never saved to disk.");
    dl->addWidget(inf);
    inf->setMaximumWidth(500);
    inf->setWordWrap(true);

    auto bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dl->addWidget(bbox);
    QObject::connect(bbox, &QDialogButtonBox::rejected, dlg.get(), &QDialog::reject);
    QObject::connect(bbox, &QDialogButtonBox::accepted, dlg.get(), [=, dlg = dlg.get()] {
        dlg->accept();
    });

    dlg->exec();

    if (dlg->result() != QDialog::Accepted) return false;
    triedUserName = usn->text();
    triedPassword = pw->text();
    if (triedUserName.isEmpty()) return false;

    QStringList args;
    args << "+force_install_dir" << Config::steamcmdDLRoot;
    args << "+login" << triedUserName << triedPassword;
    args << "+workshop_download_item" << "211820" << "2512589532"; // we'll use Stardust Core Lite for this
    args << "+_dummy"; // dummy line
    args << "+exit";

    std::unique_ptr<QObject> ctx(new QObject());

    QObject::connect(scp, &QProcess::readyRead, ctx.get(), [&ctx] { // clazy:exclude=lambda-in-connect
        while (scp->canReadLine()) {
            QString l = scp->readLine();
            //qDebug() << l;
            if (l.contains("Missing decryption key")) {
                ctx.reset();
                scp->close(); // abort
                QMessageBox::critical(nullptr, "Setup Failed", "Could not fetch decryption key (account does not own Starbound).");
                return;
            } else if (l.contains("FAILED (Invalid Password)")) {
                ctx.reset();
                scp->close(); // abort
                QMessageBox::critical(nullptr, "Setup Failed", "Could not fetch decryption key (invalid credentials).");
                return;
            }
        }
    });


    scp->setArguments(args);
    scp->start();
    ev.exec();

    if (!ctx) return false; // failure
    return true;
}
























//

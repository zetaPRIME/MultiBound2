#pragma once

#include <QDialog>
#include <QPointer>

namespace MultiBound {
    namespace Ui { class SettingsWindow; }

    class SettingsWindow : public QDialog {
        Q_OBJECT

    public:
        explicit SettingsWindow(QWidget *parent = nullptr);
        ~SettingsWindow();

        static QPointer<SettingsWindow> instance;

    private:
        Ui::SettingsWindow *ui;
    };

}

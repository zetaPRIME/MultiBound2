#pragma once

#include <QDialog>
#include <QPointer>

namespace MultiBound {
    namespace Ui { class SettingsWindow; }

    class SettingsWindow : public QDialog {
        Q_OBJECT

    public:
        static QPointer<SettingsWindow> instance;

        explicit SettingsWindow(QWidget *parent = nullptr);
        ~SettingsWindow();

    private:
        Ui::SettingsWindow *ui;

        void apply();
    };

}

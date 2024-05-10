#pragma once

#include <QMainWindow>
#include <QPointer>

#include <memory>
#include <vector>

class QListWidgetItem;

namespace MultiBound {
    namespace Ui { class MainWindow; }
    class Instance;
    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        static QPointer<MainWindow> instance;
        std::vector<std::shared_ptr<Instance>> instances;

        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow();

        void showEvent(QShowEvent*) override;

        void setInteractive(bool);

        void refresh(const QString& focusPath = { });
        void launch(Instance* inst = nullptr);

        void updateFromWorkshop(Instance* inst = nullptr);
        void newFromWorkshop(const QString& = { });

        Instance* selectedInstance();
        Instance* findWorkshopId(const QString&);

    private:
        Ui::MainWindow *ui;

    signals:
        void refreshSettings();

    };

}

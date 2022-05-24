#pragma once

#include <QMainWindow>

#include <memory>
#include <vector>

class QListWidgetItem;

namespace MultiBound {
    namespace Ui { class MainWindow; }
    class Instance;
    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        std::vector<std::shared_ptr<Instance>> instances;

        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow();

        void setInteractive(bool);

        void refresh(const QString& focusPath = QStringLiteral());
        void launch(Instance* inst = nullptr);

        void updateFromWorkshop(Instance* inst = nullptr);
        void newFromWorkshop(const QString& = QStringLiteral());

        Instance* selectedInstance();
        Instance* findWorkshopId(const QString&);

    private:
        Ui::MainWindow *ui;

    };

}

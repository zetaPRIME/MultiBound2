#pragma once

#include <QMainWindow>

#include <memory>
#include <vector>

class QListWidgetItem;

namespace Ui {
    class MainWindow;
}

namespace MultiBound {
    class Instance;
    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        std::vector<std::shared_ptr<Instance>> instances;

        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

        void refresh();
        void launch();

    private:
        Ui::MainWindow *ui;

    };

}

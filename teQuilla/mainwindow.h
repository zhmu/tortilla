#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include "qtorrentstablemodel.h"
#include "overseer.h"
#include <QTimer>

namespace Ui
{
    class MainWindowClass;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Overseer* o, QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindowClass *ui;
    Overseer* overseer;
    QTorrentsTableModel *model;
    QTimer* timer;
private slots:
    void updateModel();
};

#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include "qtorrentstablemodel.h"
#include "overseer.h"
#include "http.h"
#include <QTimer>
#include <QTableView>

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
    void on_btnRefreshLog_clicked();
    void btnAddTorrent_clicked();
    void btnDelTorrent_clicked();
    void btnStart_clicked();
    void btnStop_clicked();
    void update();
    void updateModel();
    void updateLog();
};

#endif // MAINWINDOW_H

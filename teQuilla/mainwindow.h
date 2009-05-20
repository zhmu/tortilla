#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include "qtorrentstablemodel.h"
#include "overseer.h"
#include "http.h"
#include <QTimer>
#include <QTableView>
#include "qpeerstablemodel.h"

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
    bool getPieceDone(qreal offset, qreal ratio, std::vector<PieceInfo>& pieces);
    Ui::MainWindowClass *ui;
    Overseer* overseer;
    QTorrentsTableModel *torrentsModel;
    QPeersTableModel *peersModel;
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

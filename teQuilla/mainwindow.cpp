#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDirModel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass)
{
    ui->setupUi(this);
    std::vector<Torrent*> vt;
    QTorrentsTableModel *model = new QTorrentsTableModel(vt, this);
    ui->tableTorrents->setModel(model);
}

MainWindow::~MainWindow()
{
    delete ui;
}

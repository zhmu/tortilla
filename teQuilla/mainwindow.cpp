#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDirModel>


MainWindow::MainWindow(Overseer* o, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass),overseer(o)
{
    ui->setupUi(this);
    std::vector<Torrent*> vt = overseer->getTorrents();
    model = new QTorrentsTableModel(vt, this);
    ui->tableTorrents->setModel(model);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateModel()));
    timer->start(1000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateModel()
{
    std::vector<Torrent*> vt = overseer->getTorrents();
    model->updateData(vt);
}

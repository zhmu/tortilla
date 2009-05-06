#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDirModel>
#include <QToolButton>
#include <QSplitter>

MainWindow::MainWindow(Overseer* o, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass),overseer(o)
{
    ui->setupUi(this);
    std::vector<Torrent*> vt = overseer->getTorrents();
    model = new QTorrentsTableModel(vt, this);
    ui->tableTorrents->setModel(model);
    QToolButton* tb = new QToolButton(ui->mainToolBar);
    QIcon icon;
    icon.addFile("/home/dwight/projects/tortilla/teQuilla/icon/plus.png", QSize(10,10), QIcon::Normal, QIcon::Off);
    tb->setIcon(icon);
    ui->mainToolBar->addWidget(tb);

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

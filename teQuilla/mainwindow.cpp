#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDirModel>
#include <QToolButton>
#include <iostream>
#include <fstream>
#include <sstream>
#include <QScrollBar>
#include <QFileDialog>
 #include <QErrorMessage>
#include <math.h>
#include <assert.h>

using namespace std;

MainWindow::MainWindow(Overseer* o, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass),overseer(o)
{
    ui->setupUi(this);

    /* Setup the tableview's model */
    torrentsModel = new QTorrentsTableModel(o, this);
    ui->tableTorrents->setModel(torrentsModel);
    peersModel = new QPeersTableModel(0,this);
    ui->tablePeers->setModel(peersModel);

    /* Setup the toolbar which looks like:
       add del sep start stop
    */
    QAction* act_addTorrent = ui->mainToolBar->addAction(QIcon("./icon/plus.png"), "Add torrents from disk");
    connect(act_addTorrent, SIGNAL(triggered()), this, SLOT(btnAddTorrent_clicked()));
    QAction* act_delTorrent = ui->mainToolBar->addAction(QIcon("./icon/minus.png"), "Delete torrent entry");
    connect(act_delTorrent, SIGNAL(triggered()), this, SLOT(btnDelTorrent_clicked()));
    ui->mainToolBar->addSeparator();
    QAction* act_start = ui->mainToolBar->addAction(QIcon("./icon/play.png"), "Start all torrents");
    connect(act_start, SIGNAL(triggered()), this, SLOT(btnStart_clicked()));
    QAction* act_stop = ui->mainToolBar->addAction(QIcon("./icon/stop.png"), "Stop all torrents");
    connect(act_stop, SIGNAL(triggered()), this, SLOT(btnStop_clicked()));

    /* Setup periodic timer */
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update()
{
    updateModel();
}

void MainWindow::updateModel()
{
    torrentsModel->updateData();
    btnStart_clicked();
    /* XXX: peers tab visible? */
    if (true)
    {
        QModelIndexList indexes = ui->tableTorrents->selectionModel()->selectedRows(0);
        /* Torrent selected? */
        if (indexes.count() == 0)
            peersModel->updateData(0);
        else
            peersModel->updateData(overseer->getTorrents().at((*indexes.begin()).row()));
    }
}

void MainWindow::updateLog()
{    
    ui->textBrowserLog->clear();
    QString path = QDir::currentPath() + "/trace.log";

    ifstream is;
    is.open(path.toStdString().data(), ios::in);

    if (is)
    {
        QString s;
        while (is)
        {
            char line[1024];
            is.getline(line,1024,'\n');
            s += QString(line) + "\n";
        }
        ui->textBrowserLog->setText(s);
    }
    else
    {
        ui->textBrowserLog->setText("Logfile not available [" + path + "]");
    }
    /* Set scrollbar position to EOF */
    int sbPos = ui->textBrowserLog->verticalScrollBar()->maximum();
    ui->textBrowserLog->verticalScrollBar()->setValue(sbPos);
}

void MainWindow::on_btnRefreshLog_clicked()
{
    updateLog();
}

void MainWindow::btnAddTorrent_clicked()
{
     QStringList torrents = QFileDialog::getOpenFileNames(
                         this,
                         "Select a torrent to add...",
                         QDir::currentPath(),
                         "Torrents (*.torrent)");

    for (QStringList::iterator it=torrents.begin(); it!=torrents.end(); it++)   {
       ui->textBrowserLog->setText("Adding: " + *it + "\n");
       ifstream is;
       is.open((*it).toStdString().data(), ios::binary);
       if (is)
       {
          /* This validates the torrent metadata XXX: catch exception */
          try   {
            Metadata* md = new Metadata(is);
            overseer->addTorrent(new Torrent(overseer,md));
            delete md;
          }
            catch (exception e) {
              /* XXX: not working */
              //QErrorMessage errDlg(this);
              //errDlg.showMessage("Torrent metadata seems to be corrupt");
          }
          is.close();
       }
       else
       {
           /* XXX: not working */
           //QErrorMessage errDlg(this);
           //errDlg.showMessage("Unable to open file: " + *it);
       }
   }
}

void MainWindow::btnDelTorrent_clicked()
{
    QModelIndexList indexes = ui->tableTorrents->selectionModel()->selectedRows(0);
    QModelIndex index;

    foreach (index, indexes)    {
        ui->tableTorrents->model()->removeRow(index.row(),QModelIndex());
    }

    /* Force an update, if we don't do this one might hit delete twice
        on the same entry which doesn't really exist, though the data source
        is changed. This means you would remove the new entry that is not yet
        displayed.
    */
    updateModel();
}

bool MainWindow::getPieceDone(qreal offset, qreal size, vector<PieceInfo>& pieces)
{
    bool b = true;
    // XXX needs refinement over partly pieces ready (chunk-checking)
    for(int i=0; i<(offset+size) && i<pieces.size(); i++) {
        b &= pieces.at(i).getHave() && !pieces.at(i).isHashing();
    }

    return b;
}

void MainWindow::btnStart_clicked()
{
    //overseer->start();
    QModelIndexList indexes = ui->tableTorrents->selectionModel()->selectedRows(0);

    if (indexes.count() == 0)
        return;

    Torrent* t = overseer->getTorrents().at((*indexes.begin()).row());
    vector<PieceInfo> pieces = t->getPieceDetails();

    QGraphicsScene* scene = new QGraphicsScene(this);
    int h = ui->graphicsView->height()-5;
    int w = ui->graphicsView->width()-5;
    int c_pieces = pieces.size();
    qreal ratio = (qreal)c_pieces/w;
    qreal offset = 0.0;

    cout << "Pieces: " << c_pieces
         << " width: " << w
         << " ratio: " << ratio << endl;

   for(int i=0; i<w; i++)   {
       bool b = getPieceDone(offset,ratio,pieces);
       cout << c_pieces << ", " << offset << ", " << ratio << endl;
       cout << w << ", now at " << i << endl;
       offset+=ratio;

        if (b)
            scene->addLine(i,0,i,h,QPen(Qt::green));
   }

    // Link the scene to the view
    ui->graphicsView->setScene(scene);
    // Set the viewport to display the whole scene
    ui->graphicsView->setSceneRect(0,0,w,h);
    // Update the scene
    ui->graphicsView->show();
}

void MainWindow::btnStop_clicked()
{
    overseer->stop();
}

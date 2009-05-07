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

using namespace std;

MainWindow::MainWindow(Overseer* o, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass),overseer(o)
{
    ui->setupUi(this);

    /* Setup the tableview's model */
    model = new QTorrentsTableModel(o, this);
    ui->tableTorrents->setModel(model);

    /* Setup the toolbar which looks like:
       add del sep
    */
    QAction* act_addTorrent = ui->mainToolBar->addAction(QIcon("./icon/plus.png"), "Add torrents from disk");
    connect(act_addTorrent, SIGNAL(triggered()), this, SLOT(btnAddTorrent_clicked()));
    QAction* act_delTorrent = ui->mainToolBar->addAction(QIcon("./icon/minus.png"), "Delete torrent entry");
    connect(act_delTorrent, SIGNAL(triggered()), this, SLOT(btnAddTorrent_clicked()));
    ui->mainToolBar->addSeparator();

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
    //std::vector<Torrent*> vt = overseer->getTorrents();

    //std::cout << vt.size();
    //std::cout.flush();
    model->updateData();
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

#include <QtGui/QApplication>
#include "mainwindow.h"
#include "tracer.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

Tracer* tracer = NULL;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    tracer = new Tracer();
    Overseer* overseer = new Overseer(9999,tracer);

    // XXX: generalise, single hardcoded torrent for testing
    vector<Metadata*> metadatas;
    ifstream is;
    is.open("/home/dwight/projects/tortilla/teQuilla/test.torrent", ios::binary);
    metadatas.push_back(new Metadata(is));
    Metadata* md = *metadatas.begin();
    overseer->addTorrent(new Torrent(overseer,md));
    delete md;

    overseer->setUploadRate(16 * 1024);
    overseer->waitHashingComplete();
    overseer->start();

    MainWindow w(overseer);
    w.show();

    int r = a.exec();
    overseer->stop();
    return r;
}

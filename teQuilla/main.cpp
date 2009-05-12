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

    overseer->setUploadRate(16 * 1024);
    overseer->start();

    MainWindow w(overseer);
    w.show();

    int r = a.exec();
    overseer->stop();
    return r;
}

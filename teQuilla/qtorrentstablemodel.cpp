#include "qtorrentstablemodel.h"

using namespace std;

#define COLUMNS 6


QTorrentsTableModel::QTorrentsTableModel(vector<Torrent*> torrents, QObject* parent) : QAbstractTableModel(parent)
{
    typedef vector<Torrent*>::iterator vti;
    uint32_t rx=0,tx=0;
    uint64_t filesize=0, bytesleft=0;
    uint64_t uploaded=0, downloaded=0;

    for(vti it=torrents.begin(); it!=torrents.end(); it++) {
        torrent_info *info = new torrent_info();
        (*it)->getRateCounters(&rx,&tx);
        downloaded = (*it)->getBytesDownloaded();
        uploaded = (*it)->getBytesUploaded();
        bytesleft = (*it)->getBytesLeft();
        filesize = (*it)->getTotalSize();
        info->append((*it)->getName().data());
        info->append(QString("%1 kB/s").arg((double)tx/1024,0,'n',2));
        info->append(QString("%1 kB/s").arg((double)rx/1024,0,'n',2));
        info->append(QString("%1 kB").arg(downloaded/1024));
        info->append(QString("%1 kB").arg(uploaded/1024));
        info->append(QString("%1 / %2 MB").arg((double)bytesleft/1024/1024,0,'n',2).arg((double)filesize/1024/1024,0,'n',2));
        table_data.append(info);
    }
}

void
QTorrentsTableModel::updateData(std::vector<Torrent*> torrents)
{
    // XXX: dump current data instead of updating
    for(QList<torrent_info*>::iterator it=table_data.begin(); it!=table_data.end(); it++)   {
        delete *it;
    }
    table_data.clear();

    typedef vector<Torrent*>::iterator vti;
    uint32_t rx=0,tx=0;
    uint64_t filesize=0, bytesleft=0;
    uint64_t uploaded=0, downloaded=0;
    uint32_t c=0;
    for(vti it=torrents.begin(); it!=torrents.end(); it++) {
        torrent_info *info = new torrent_info();
        (*it)->getRateCounters(&rx,&tx);
        downloaded = (*it)->getBytesDownloaded();
        uploaded = (*it)->getBytesUploaded();
        bytesleft = (*it)->getBytesLeft();
        filesize = (*it)->getTotalSize();
        info->append((*it)->getName().data());
        info->append(QString("%1 kB/s").arg((double)tx/1024,0,'n',2));
        info->append(QString("%1 kB/s").arg((double)rx/1024,0,'n',2));
        info->append(QString("%1 kB").arg(downloaded/1024));
        info->append(QString("%1 kB").arg(uploaded/1024));
        info->append(QString("%1 / %2 MB").arg((double)bytesleft/1024/1024,0,'n',2).arg((double)filesize/1024/1024,0,'n',2));
        table_data.append(info);
    }
    reset();
    //emit dataChanged(QModelIndex(), QModelIndex());
}

int
QTorrentsTableModel::rowCount(const QModelIndex & /* parent */) const
{
    return table_data.size();
}

int
QTorrentsTableModel::columnCount(const QModelIndex & /* parent */) const
{
    return COLUMNS;
}

QVariant
QTorrentsTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const torrent_info* row = table_data.at(index.row());
    return row->at(index.column());
}

QVariant
QTorrentsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)   {
        switch (section)    {
            case 0: return QString("Name");
            case 1: return QString("TX");
            case 2: return QString("RX");
            case 3: return QString("Downloaded");
            case 4: return QString("Uploaded");
            case 5: return QString("Left");
            default: return QVariant();
        }
    }

    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section+1;
    }

    return QVariant();
}

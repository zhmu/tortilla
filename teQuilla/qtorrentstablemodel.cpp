#include "qtorrentstablemodel.h"

using namespace std;

#define COLUMNS 2


QTorrentsTableModel::QTorrentsTableModel(vector<Torrent*> torrents, QObject* parent) : QAbstractTableModel(parent)
{
    typedef vector<Torrent*>::iterator vti;
    for(vti it=torrents.begin(); it!=torrents.end(); it++) {
        torrent_info *info = new torrent_info();
        info->append((*it)->getName().data() );
        table_data.append(*info);
    }
    torrent_info *info = new torrent_info();
    info->append("Ik ben een torrent!");
    info->append("Echt waar");
    table_data.append(*info);
    table_data.append(*info);
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

    const torrent_info& row = table_data.at(index.row());
    return row.at(index.column());
}

QVariant
QTorrentsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)   {
        switch (section)    {
            case 0: return QString("Name");
            case 1: return QString("Info");
            default: return QVariant();
        }
    }

    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section+1;
    }

    return QVariant();
}

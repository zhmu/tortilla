#include "qpeerstablemodel.h"

#define COLUMNS 10

using namespace std;

void
QPeersTableModel::updateData(Torrent* t)
{
    torrent = t;

    int prev_size = table_data.count();
    // XXX: dumping current data instead of updating
    for(QList<peer_info*>::iterator it=table_data.begin(); it!=table_data.end(); it++)   {
        delete *it;
    }
    table_data.clear();
    //endRemoveRows();

    if (torrent == 0)   {
        reset();
        return;
    }

    typedef vector<PeerInfo>::iterator vpi;
    uint32_t rx=0,tx=0;

    vector<PeerInfo> peers = torrent->getPeerDetails();
  //  beginInsertRows(QModelIndex(),0,torrents.size()-1);
    for(vpi it=peers.begin(); it!=peers.end(); it++) {
        peer_info *info = new peer_info();
        info->append(QString("%1").arg(it->getEndpoint().data()));
        info->append((it->getNumPieces() / (float)torrent->getNumPieces())*100.0f);
        info->append(it->isIncoming()?"in":"out");
        tx = it->getTxRate();
        rx = it->getRxRate();
        info->append(QString("%1 kB/s").arg((double)tx/1024,0,'n',2));
        info->append(QString("%1 kB/s").arg((double)rx/1024,0,'n',2));
        info->append(it->isSnubbed());
        info->append(it->isPeerInterested());
        info->append(it->isPeerChoked());
        info->append(it->areInterested());
        info->append(it->areChoking());
        table_data.append(info);
    }
  //  endInsertRows();

    if (prev_size != table_data.count())
        reset();
    else
        emit dataChanged(QModelIndex(), QModelIndex());
}

int
QPeersTableModel::rowCount(const QModelIndex &/*parent*/) const
{
    if (torrent == 0)
        return 0;

    return torrent->getPeerDetails().size();
}

int
QPeersTableModel::columnCount(const QModelIndex &/*parent*/) const
{
    return COLUMNS;
}

QVariant
QPeersTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)   {
        switch (section)    {
            case 0: return QString("Endpoint");
            case 1: return QString("Done");
            case 2: return QString("Dir");
            case 3: return QString("RX");
            case 4: return QString("TX");
            case 5: return QString("Snubbed");
            case 6: return QString("Peer_int");
            case 7: return QString("Peer_chk");
            case 8: return QString("Are_int");
            case 9: return QString("Are_chk");
            default: return QVariant();
        }
    }

    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section+1;
    }

    return QVariant();
}

QVariant
QPeersTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const peer_info* row = table_data.at(index.row());
    return row->at(index.column());
}

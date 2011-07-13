#ifndef QPEERSTABLEMODEL_H
#define QPEERSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "torrent.h"

class QPeersTableModel : public QAbstractTableModel
{
public:
    QPeersTableModel(Torrent* t=0, QObject* parent=0) : QAbstractTableModel(parent), torrent(t) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void updateData(Torrent* t);
private:
    typedef QList<QVariant> peer_info;
    QList<peer_info*> table_data;
    Torrent* torrent;
};

#endif // QPEERSTABLEMODEL_H

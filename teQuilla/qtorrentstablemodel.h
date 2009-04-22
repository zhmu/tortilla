#ifndef QTORRENTSTABLEMODEL_H
#define QTORRENTSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "torrent.h"


class QTorrentsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    QTorrentsTableModel(std::vector<Torrent*> torrents, QObject* parent=0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void updateData(std::vector<Torrent*> torrents);
private:
    typedef QList<QString> torrent_info;
    QList<torrent_info*> table_data;
};

#endif // QTORRENTSTABLEMODEL_H

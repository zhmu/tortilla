#ifndef QTORRENTSTABLEMODEL_H
#define QTORRENTSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "overseer.h"


class QTorrentsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    QTorrentsTableModel(Overseer* o, QObject* parent=0) : QAbstractTableModel(parent), overseer(o) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void updateData();
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex());
private:
    typedef QList<QString> torrent_info;
    QList<torrent_info*> table_data;
    Overseer* overseer;
};

#endif // QTORRENTSTABLEMODEL_H

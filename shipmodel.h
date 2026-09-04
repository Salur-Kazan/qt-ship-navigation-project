#ifndef SHIPMODEL_H
#define SHIPMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

#include "database.h"

class ShipModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ShipRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        FlagRole,
        LatitudeRole,
        LongitudeRole,
        SpeedRole,
        CourseRole,
        ShipClassRole,
        TypeRole,
        AffiliationRole
    };

    explicit ShipModel(QObject *parent = nullptr);

    int rowCount(
        const QModelIndex &parent = QModelIndex()
        ) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole
        ) const override;

    QHash<int, QByteArray> roleNames() const override;

    QVector<Ship> getShips() const;
    void setShips(const QVector<Ship>& ships);

    Q_INVOKABLE QVariantList getShipsForQml() const;

    Q_INVOKABLE bool setShipLocation(
        const QString& id,
        double latitude,
        double longitude,
        int course
        );

    Q_INVOKABLE bool saveShipLocation(
        const QString& id
        );

    Q_INVOKABLE bool setShipSpeed(
        const QString& id,
        int speed
        );

signals:

    void shipLocationSaveRequested(
        const QString& id,
        double latitude,
        double longitude,
        int course
        );;

private:
    QVector<Ship> m_ships;
};

#endif
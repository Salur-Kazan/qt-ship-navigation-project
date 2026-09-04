#include "shipmodel.h"

#include <cmath>
#include <QThreadPool>

#include "worker.h"

ShipModel::ShipModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ShipModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_ships.size();
}

QVector<Ship> ShipModel::getShips() const
{
    return m_ships;
}

QVariantList ShipModel::getShipsForQml() const
{
    QVariantList result;

    for (const Ship& ship : m_ships)
    {
        QVariantMap map;

        map["id"] = ship.id;
        map["name"] = ship.name;
        map["flag"] = ship.flag;
        map["latitude"] = ship.latitude;
        map["longitude"] = ship.longitude;
        map["speed"] = ship.speed;
        map["course"] = ship.course;
        map["shipClass"] = ship.shipClass;
        map["type"] = ship.type;
        map["affiliation"] = ship.affiliation;

        result.append(map);
    }

    return result;
}

#include <cmath>

bool ShipModel::setShipLocation(
    const QString& id,
    double latitude,
    double longitude,
    int course)
{
    if (!std::isfinite(latitude) ||
        !std::isfinite(longitude))
    {
        qWarning()
        << "Invalid ship coordinates:"
        << id
        << latitude
        << longitude;

        return false;
    }

    if (latitude < -90.0 ||
        latitude > 90.0 ||
        longitude < -180.0 ||
        longitude > 180.0)
    {
        qWarning()
        << "Out-of-range ship coordinates:"
        << id
        << latitude
        << longitude;

        return false;
    }

    for (int i = 0; i < m_ships.size(); ++i)
    {
        if (m_ships[i].id != id)
            continue;

        m_ships[i].latitude = latitude;
        m_ships[i].longitude = longitude;

        if (course >= 0) {
            m_ships[i].course = course;
        }

        QModelIndex modelIndex =
            index(i);

        emit dataChanged(
            modelIndex,
            modelIndex,
            {
                LatitudeRole,
                LongitudeRole,
                CourseRole
            }
            );

        return true;
    }

    qWarning()
        << "Ship not found in model:"
        << id;

    return false;
}

bool ShipModel::saveShipLocation(
    const QString& id)
{
    for (const Ship& ship : m_ships)
    {
        if (ship.id != id)
            continue;

        emit shipLocationSaveRequested(
            ship.id,
            ship.latitude,
            ship.longitude,
            ship.course
            );

        return true;
    }

    qWarning()
        << "Cannot save unknown ship:"
        << id;

    return false;
}

/*
 * ============================================================
 * SET SHIP SPEED
 *
 * Changes the local ShipModel immediately and asynchronously
 * writes the new value to MongoDB through worker.
 * ============================================================
 */

bool ShipModel::setShipSpeed(
    const QString& id,
    int speed)
{
    if (id.isEmpty())
    {
        return false;
    }

    if (speed < 0)
    {
        qWarning()
        << "Invalid ship speed:"
        << id
        << speed;

        return false;
    }

    bool found = false;

    for (int i = 0;
         i < m_ships.size();
         ++i)
    {
        if (m_ships[i].id != id)
            continue;

        /*
         * Update local model immediately.
         *
         * This prevents the QML panel/marker from continuing
         * to use the old value.
         */
        m_ships[i].speed =
            speed;

        QModelIndex modelIndex =
            index(i);

        emit dataChanged(
            modelIndex,
            modelIndex,
            {
                SpeedRole
            }
            );

        found = true;

        break;
    }

    if (!found)
    {
        qWarning()
        << "Ship not found in model while updating speed:"
        << id;

        return false;
    }

    /*
     * Perform MongoDB update in the thread pool.
     */

    worker* newWorker =
        new worker(
            worker::Operation::UpdateShipSpeed,
            QVector<Ship>{},
            id,
            0.0,
            0.0,
            QString{},
            QVariantList{},
            speed
            );

    QThreadPool::globalInstance()->start(
        newWorker
        );

    return true;
}

QVariant ShipModel::data(
    const QModelIndex &index,
    int role
    ) const
{
    if (!index.isValid())
        return {};

    if (index.row() < 0 ||
        index.row() >= m_ships.size())
    {
        return {};
    }

    const Ship& ship = m_ships.at(index.row());

    switch (role)
    {
    case IdRole:
        return ship.id;

    case NameRole:
        return ship.name;

    case FlagRole:
        return ship.flag;

    case LatitudeRole:
        return ship.latitude;

    case LongitudeRole:
        return ship.longitude;

    case SpeedRole:
        return ship.speed;

    case CourseRole:
        return ship.course;

    case ShipClassRole:
        return ship.shipClass;

    case TypeRole:
        return ship.type;

    case AffiliationRole:
        return ship.affiliation;

    default:
        return {};
    }
}

QHash<int, QByteArray> ShipModel::roleNames() const
{
    return {
        { IdRole,          "id" },
        { NameRole,        "name" },
        { FlagRole,        "flag" },
        { LatitudeRole,    "latitude" },
        { LongitudeRole,   "longitude" },
        { SpeedRole,       "speed" },
        { CourseRole,      "course" },
        { ShipClassRole,   "shipClass" },
        { TypeRole,        "type" },
        { AffiliationRole, "affiliation" }
    };
}

void ShipModel::setShips(const QVector<Ship>& ships)
{
    beginResetModel();

    m_ships = ships;

    endResetModel();
}
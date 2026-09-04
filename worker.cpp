#include "worker.h"

#include <QDebug>
#include <QThread>

worker::worker(
    Operation operation,
    const QVector<Ship>& currentShips,
    const QString& shipId,
    double latitude,
    double longitude,
    const QString& routeId,
    const QVariantList& routePath,
    int speed,
    int stoppedWaypointIndex,
    int course,
    QObject* parent
    )
    : QObject(parent)
    , operation(operation)
    , currentShips(currentShips)
    , shipId(shipId)
    , routeId(routeId)
    , latitude(latitude)
    , longitude(longitude)
    , routePath(routePath)
    , speed(speed)
    , stoppedWaypointIndex(stoppedWaypointIndex)
    , course(course)
{
    setAutoDelete(true);
}
worker::~worker()
{
}

void worker::run()
{
    qDebug()
    << "Worker thread:"
    << QThread::currentThreadId();

    switch (operation)
    {
    case Operation::GetShips:

        getShips();

        break;

    case Operation::SaveShipLocation:

        saveShipLocation();

        break;

    case Operation::UpdateShipSpeed:

        updateShipSpeed();

        break;

    case Operation::SaveRoute:
    {
        database db;

        const bool success =
            db.saveRoute(
                shipId,
                routeId,
                latitude,
                longitude,
                routePath
                );

        qDebug()
            << "SaveRoute:"
            << "ship:"
            << shipId
            << "route:"
            << routeId
            << "success:"
            << success;

        break;
    }

    case Operation::UpdateRouteProgress:
    {
        database db;

        /*
         * updateRouteProgress() now updates BOTH:
         *
         * routes.currentLat/currentLon
         * ships.latitude/longitude
         *
         * Therefore the normal ship refresh cannot restore an
         * old position.
         */
        const bool success =
            db.updateRouteProgress(
                shipId,
                routeId,
                latitude,
                longitude,
                course
                );

        if (!success)
        {
            qDebug()
            << "UpdateRouteProgress ignored/failed:"
            << "ship:"
            << shipId
            << "route:"
            << routeId
            << "position:"
            << latitude
            << longitude;
        }

        break;
    }

    case Operation::MarkFinished:
    {
        database db;

        /*
         * markRouteFinished() now also updates the ships
         * collection with the final position.
         */
        const bool success =
            db.markRouteFinished(
                shipId,
                routeId,
                latitude,
                longitude,
                stoppedWaypointIndex,
                course
                );

        qDebug()
            << "MarkFinished:"
            << "ship:"
            << shipId
            << "route:"
            << routeId
            << "final:"
            << latitude
            << longitude
            << "success:"
            << success;

        break;
    }

    case Operation::DeleteRoute:
    {
        database db;

        const bool success =
            db.deleteRoute(
                shipId,
                routeId
                );

        qDebug()
            << "DeleteRoute:"
            << shipId
            << routeId
            << success;

        break;
    }
    }
}


/*
 * ============================================================
 * GET SHIPS + RESTORE ACTIVE ROUTES
 * ============================================================
 */

void worker::getShips()
{
    database databaseObject;

    QVector<Ship> ships;

    if (!databaseObject.getShips(ships))
    {
        return;
    }

    QHash<QString, ActiveRouteData> activeRoutes;

    if (databaseObject.getActiveRoutes(
            activeRoutes))
    {
        static bool isFirstRun = true;

        QVariantList restoreList;

        for (Ship& ship :
             ships)
        {
            if (!activeRoutes.contains(
                    ship.id))
            {
                /*
                 * No active route.
                 *
                 * IMPORTANT:
                 * At this point ship.latitude/longitude comes
                 * directly from the ships collection, which is
                 * now kept synchronized by route progress/finish.
                 */
                continue;
            }

            const ActiveRouteData route =
                activeRoutes.value(
                    ship.id
                    );

            /*
             * Restore the database's last known route position
             * into the ship model.
             */

            ship.latitude =
                route.currentLat;

            ship.longitude =
                route.currentLon;

            /*
             * Only restore Navigation once.
             */

            if (isFirstRun)
            {
                QVariantMap routeMap;

                routeMap["shipId"] =
                    route.shipId;

                routeMap["routeId"] =
                    route.routeId;

                routeMap["lat"] =
                    route.currentLat;

                routeMap["lon"] =
                    route.currentLon;

                routeMap["speed"] =
                    ship.speed;

                routeMap["course"] =
                    ship.course;

                routeMap["path"] =
                    route.path;

                restoreList.append(
                    routeMap
                    );
            }
        }

        if (isFirstRun &&
            !restoreList.isEmpty())
        {
            isFirstRun = false;

            emit routesRestored(
                restoreList
                );
        }
    }

    emit finished(
        ships
        );
}


/*
 * ============================================================
 * SAVE SHIP LOCATION
 * ============================================================
 */

void worker::saveShipLocation()
{
    database databaseObject;

    const bool success =
        databaseObject.updateShipLocation(
            shipId,
            latitude,
            longitude,
            course
            );

    emit saveFinished(
        shipId,
        success
        );
}

/*
 * ============================================================
 * UPDATE SHIP SPEED
 * ============================================================
 */

void worker::updateShipSpeed()
{
    database databaseObject;

    const bool success =
        databaseObject.updateShipSpeed(
            shipId,
            speed
            );

    qDebug()
        << "UpdateShipSpeed:"
        << "ship:"
        << shipId
        << "speed:"
        << speed
        << "success:"
        << success;
}
#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QVector>
#include <QVariantList>
#include <QHash>

    struct Ship
{
    QString id;

    QString name;
    QString flag;

    double latitude = 0.0;
    double longitude = 0.0;

    int speed = 0;
    int course = 0;

    QString shipClass;
    QString type;
    QString affiliation;

    bool operator==(const Ship& other) const
    {
        return id == other.id &&
               name == other.name &&
               flag == other.flag &&
               latitude == other.latitude &&
               longitude == other.longitude &&
               speed == other.speed &&
               course == other.course &&
               shipClass == other.shipClass &&
               type == other.type &&
               affiliation == other.affiliation;
    }
};

struct ActiveRouteData
{
    QString routeId;
    QString shipId;

    double currentLat = 0.0;
    double currentLon = 0.0;

    QVariantList path;
};

class database
{
public:

    database();

    bool getShips(QVector<Ship>& ships);

    bool updateShipLocation(
        const QString& id,
        double latitude,
        double longitude,
        int course
        );
    bool updateShipSpeed(
        const QString& id,
        int speed
        );

    /*
     * Always creates a NEW route document.
     *
     * routeId uniquely identifies this trip.
     *
     * Any older active route for the same ship is deleted
     * after the new route has been successfully inserted.
     *
     * The ship's current database position is also synchronized
     * with currentLat/currentLon.
     */
    bool saveRoute(
        const QString& shipId,
        const QString& routeId,
        double currentLat,
        double currentLon,
        const QVariantList& path
        );

    /*
     * Updates only this exact route AND synchronizes the ship's
     * latitude/longitude in the ships collection.
     */
    bool updateRouteProgress(
        const QString& shipId,
        const QString& routeId,
        double currentLat,
        double currentLon,
        int course
        );

    /*
     * Marks only this exact route as finished AND stores the
     * final ship position in the ships collection.
     */
    bool markRouteFinished(const QString& shipId, const QString& routeId, double finalLat, double finalLon, int stoppedWaypointIndex, int course);

    /*
     * Deletes only this exact active route.
     *
     * Used for explicit user cancellation.
     */
    bool deleteRoute(
        const QString& shipId,
        const QString& routeId
        );

    /*
     * Reads currently active routes.
     *
     * There should be only one active route per ship.
     */
    bool getActiveRoutes(
        QHash<QString, ActiveRouteData>& routes
        );

    /*
     * Reads finished route history.
     */
    QVariantList getFinishedRoutesForShip(
        const QString& shipId
        );
};

#endif // DATABASE_H
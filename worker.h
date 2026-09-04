#ifndef WORKER_H
#define WORKER_H

#include <QRunnable>
#include <QObject>
#include <QVector>
#include <QVariantList>

#include "database.h"

    class worker : public QObject, public QRunnable
{
    Q_OBJECT

public:

    enum class Operation
    {
        GetShips,
        UpdateShipSpeed,
        SaveShipLocation,

        SaveRoute,
        UpdateRouteProgress,
        MarkFinished,
        DeleteRoute
    };

    worker(
        Operation operation,
        const QVector<Ship>& currentShips = {},
        const QString& shipId = {},
        double latitude = 0.0,
        double longitude = 0.0,
        const QString& routeId = {},
        const QVariantList& routePath = {},
        int speed = 0,
        int stoppedWaypointIndex = std::numeric_limits<int>::max(),
        int course = 0,
        QObject* parent = nullptr
        );

    ~worker();

    void run() override;

signals:

    void finished(
        const QVector<Ship>& ships
        );

    void saveFinished(
        const QString& shipId,
        bool success
        );

    void routesRestored(
        const QVariantList& restoreData
        );

private:

    void getShips();

    void saveShipLocation();

    void updateShipSpeed();

    Operation operation;

    QVector<Ship> currentShips;

    QString shipId;

    QString routeId;

    double latitude = 0.0;

    double longitude = 0.0;

    QVariantList routePath;

    int speed = 0;

    int stoppedWaypointIndex = std::numeric_limits<int>::max();

    int course;
};

#endif // WORKER_H
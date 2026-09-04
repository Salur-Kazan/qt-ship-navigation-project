#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QQmlContext>
#include <QTimer>
#include <QThreadPool>
#include <QThread>
#include <QDebug>

#include <atomic>

#include "worker.h"
#include "shipmodel.h"
#include "navigation.h"


int main(
    int argc,
    char *argv[])
{
    /*
     * --------------------------------------------------------
     * MapLibre environment
     * --------------------------------------------------------
     */

    qputenv(
        "QT_PLUGIN_PATH",
        "/home/aysat/maplibre-qt-install/plugins"
        );

    qputenv(
        "QML2_IMPORT_PATH",
        "/home/aysat/maplibre-qt-install/qml"
        );


    QGuiApplication app(
        argc,
        argv
        );


    QQmlApplicationEngine engine;


    engine.addImportPath(
        "/home/aysat/maplibre-qt-install/qml"
        );


    ShipModel shipModel;

    Navigation navigation;


    /*
     * --------------------------------------------------------
     * QML context
     * --------------------------------------------------------
     */

    engine.rootContext()->setContextProperty(
        "shipModel",
        &shipModel
        );

    engine.rootContext()->setContextProperty(
        "navigation",
        &navigation
        );


    /*
     * --------------------------------------------------------
     * WATER SHAPEFILE
     * --------------------------------------------------------
     */

    const QString waterPath =
        QCoreApplication::applicationDirPath()
        +
        "/navigation_data/water_polygons.shp";


    if (!navigation.loadWaterPolygons(
            waterPath))
    {
        qWarning()
        << "Could not load:"
        << waterPath;
    }


    /*
     * --------------------------------------------------------
     * Prevent simultaneous DB refresh operations.
     * --------------------------------------------------------
     */

    std::atomic_bool refreshRunning =
        false;


    /*
     * --------------------------------------------------------
     * QML creation failure
     * --------------------------------------------------------
     */

    QObject::connect(
        &engine,

        &QQmlApplicationEngine::objectCreationFailed,

        &app,

        []()
        {
            QCoreApplication::exit(-1);
        },

        Qt::QueuedConnection
        );


    /*
     * --------------------------------------------------------
     * Load QML
     * --------------------------------------------------------
     */

    engine.loadFromModule(
        "RadarDatabase",
        "Main"
        );


    if (engine.rootObjects().isEmpty())
    {
        return -1;
    }


    /*
     * ========================================================
     * DATABASE REFRESH
     * ========================================================
     */

    auto startDatabaseRefresh =
        [&]()
    {
        qDebug()
        << "Main thread:"
        << QThread::currentThreadId();


        bool expected =
            false;


        if (!refreshRunning.compare_exchange_strong(
                expected,
                true))
        {
            return;
        }


        /*
             * Current model snapshot.
             */

        QVector<Ship> currentShips =
            shipModel.getShips();


        qDebug()
            << "Ships being sent to worker:"
            << currentShips.size();


        /*
             * Get ship/database information.
             */

        worker* newWorker =
            new worker(
                worker::Operation::GetShips
                );


        /*
             * ------------------------------------------------
             * RESTORE ACTIVE ROUTES
             * ------------------------------------------------
             */

        QObject::connect(
            newWorker,

            &worker::routesRestored,

            &app,

            [&](
                const QVariantList& routes)
            {
                for (const QVariant& value :
                     routes)
                {
                    const QVariantMap route =
                        value.toMap();


                    navigation.restoreNavigation(
                        route[
                            "shipId"
                    ].toString(),

                        route[
                            "routeId"
                    ].toString(),

                        route[
                            "lat"
                    ].toDouble(),

                        route[
                            "lon"
                    ].toDouble(),

                        route[
                            "speed"
                    ].toInt(),

                        route[
                            "path"
                    ].toList()
                        );
                }
            },

            Qt::QueuedConnection
            );


        /*
             * ------------------------------------------------
             * SHIPS RESULT
             * ------------------------------------------------
             */

        QObject::connect(
            newWorker,

            &worker::finished,

            &app,

            [&](
                const QVector<Ship>& ships)
            {
                refreshRunning =
                    false;

                shipModel.setShips(
                    ships
                    );
            },

            Qt::QueuedConnection
            );


        QThreadPool::globalInstance()
            ->start(
                newWorker
                );
    };

    /*
     * ========================================================
     * SAVE NEW ROUTE
     * ========================================================
     *
     * Only a USER-created route reaches this signal.
     *
     * Automatic reroutes reuse the existing route ID.
     */

    QObject::connect(
        &navigation,

        &Navigation::routeStartedNeedsSave,

        &app,

        [](
            const QString& shipId,
            const QString& routeId,
            double latitude,
            double longitude,
            const QVariantList& path)
        {
            worker* w =
                new worker(
                    worker::Operation::SaveRoute,
                    {},
                    shipId,
                    latitude,
                    longitude,
                    routeId,
                    path
                    );

            QThreadPool::globalInstance()
                ->start(
                    w
                    );
        }
        );


    /*
 * ========================================================
 * UPDATE ROUTE PROGRESS
 * ========================================================
 */

    QObject::connect(
        &navigation,

        &Navigation::routeProgressNeedsSave,

        &app,

        [](
            const QString& shipId,
            const QString& routeId,
            double latitude,
            double longitude,
            int course)
        {
            qDebug()
            << "Main:"
            << "route progress received"
            << "ship:"
            << shipId
            << "lat:"
            << latitude
            << "lon:"
            << longitude
            << "course:"
            << course;

            worker* w =
                new worker(
                    worker::Operation::UpdateRouteProgress,
                    {},
                    shipId,
                    latitude,
                    longitude,
                    routeId,
                    {},
                    0,
                    -1,
                    course
                    );

            QThreadPool::globalInstance()
                ->start(
                    w
                    );
        }
        );

    /*
     * ========================================================
     * FINISH ROUTE
     * ========================================================
     */

    QObject::connect(
        &navigation,

        &Navigation::routeFinishedNeedsClear,

        &app,

        [](
            const QString& shipId,
            const QString& routeId,
            double latitude,
            double longitude,
            int stoppedWaypointIndex,
            int course)
        {
            worker* w =
                new worker(
                    worker::Operation::MarkFinished,
                    {},
                    shipId,
                    latitude,
                    longitude,
                    routeId,
                    {},
                    0,
                    stoppedWaypointIndex,
                    course
                    );

            QThreadPool::globalInstance()
                ->start(
                    w
                    );
        }
        );


    /*
     * ========================================================
     * CANCEL / DELETE ROUTE
     * ========================================================
     */

    QObject::connect(
        &navigation,

        &Navigation::routeCancelledNeedsDelete,

        &app,

        [](
            const QString& shipId,
            const QString& routeId)
        {
            worker* w =
                new worker(
                    worker::Operation::DeleteRoute,
                    {},
                    shipId,
                    0.0,
                    0.0,
                    routeId
                    );

            QThreadPool::globalInstance()
                ->start(
                    w
                    );
        }
        );


    /*
     * ========================================================
     * DATABASE REFRESH TIMER
     * ========================================================
     */

    QTimer timer;


    QObject::connect(
        &timer,

        &QTimer::timeout,

        &app,

        startDatabaseRefresh
        );


    timer.start(
        1000
        );
    QObject::connect(
        &shipModel,
        &ShipModel::shipLocationSaveRequested,
        &app,
        [&](
            const QString& id,
            double latitude,
            double longitude,
            int course)
        {
            worker* saveWorker =
                new worker(
                    worker::Operation::SaveShipLocation,
                    {},
                    id,
                    latitude,
                    longitude,
                    QString{},
                    QVariantList{},
                    0,
                    -1,
                    course
                    );

            QObject::connect(
                saveWorker,
                &worker::saveFinished,
                &app,
                [](
                    const QString& id,
                    bool success)
                {
                    Q_UNUSED(id);
                    Q_UNUSED(success);
                },
                Qt::QueuedConnection
                );

            QThreadPool::globalInstance()
                ->start(
                    saveWorker
                    );
        }
        );

    /*
     * Initial database load.
     */

    startDatabaseRefresh();


    return app.exec();
}
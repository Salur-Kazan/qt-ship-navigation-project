#include "database.h"

#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <QUuid>
#include <QGeoCoordinate>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/array.hpp>

#include <cmath>

    namespace
{

    const QString mongoUri =
        "mongodb://127.0.0.1:27017";

    const QString databaseName =
        "RadarDatabase";

    const QString collectionName =
        "ships";

    const QString routesCollectionName =
        "routes";

    mongocxx::instance mongoInstance{};

    double extractBsonDouble(const bsoncxx::document::element& el)
    {
        if (!el) return 0.0;
        if (el.type() == bsoncxx::type::k_double) return el.get_double().value;
        if (el.type() == bsoncxx::type::k_int32) return static_cast<double>(el.get_int32().value);
        if (el.type() == bsoncxx::type::k_int64) return static_cast<double>(el.get_int64().value);
        return 0.0;
    }

    QString generateFallbackRouteId()
    {
        return QUuid::createUuid()
        .toString(QUuid::WithoutBraces);
    }

}


/*
 * ============================================================
 * BSON STRING -> QString
 * ============================================================
 */

static QString bsonStringToQString(
    const bsoncxx::types::b_string& value)
{
    return QString::fromUtf8(
        value.value.data(),
        static_cast<int>(value.value.size())
        );
}


/*
 * ============================================================
 * CONSTRUCTOR
 * ============================================================
 */

database::database()
{
}


/*
 * ============================================================
 * GET SHIPS
 * ============================================================
 */

bool database::getShips(QVector<Ship>& ships)
{
    qDebug()
    << "MongoDB thread:"
    << QThread::currentThreadId();

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto db =
            client[
                databaseName.toStdString()
        ];

        auto collection =
            db[
                collectionName.toStdString()
        ];

        auto cursor =
            collection.find({});

        ships.clear();

        for (const auto& document : cursor)
        {
            Ship ship{};

            /*
             * MongoDB _id
             */

            auto idElement =
                document["_id"];

            if (idElement &&
                idElement.type() ==
                    bsoncxx::type::k_oid)
            {
                ship.id =
                    QString::fromStdString(
                        idElement
                            .get_oid()
                            .value
                            .to_string()
                        );
            }

            /*
             * Name
             */

            auto name =
                document["name"];

            if (name &&
                name.type() ==
                    bsoncxx::type::k_string)
            {
                ship.name =
                    bsonStringToQString(
                        name.get_string()
                        );
            }

            /*
             * Flag
             */

            auto flag =
                document["flag"];

            if (flag &&
                flag.type() ==
                    bsoncxx::type::k_string)
            {
                ship.flag =
                    bsonStringToQString(
                        flag.get_string()
                        );
            }

            /*
             * Latitude
             */

            auto latitude =
                document["latitude"];

            if (latitude)
            {
                switch (latitude.type())
                {
                case bsoncxx::type::k_double:

                    ship.latitude =
                        latitude
                            .get_double()
                            .value;

                    break;

                case bsoncxx::type::k_int32:

                    ship.latitude =
                        static_cast<double>(
                            latitude
                                .get_int32()
                                .value
                            );

                    break;

                case bsoncxx::type::k_int64:

                    ship.latitude =
                        static_cast<double>(
                            latitude
                                .get_int64()
                                .value
                            );

                    break;

                default:
                    break;
                }
            }

            /*
             * Longitude
             */

            auto longitude =
                document["longitude"];

            if (longitude)
            {
                switch (longitude.type())
                {
                case bsoncxx::type::k_double:

                    ship.longitude =
                        longitude
                            .get_double()
                            .value;

                    break;

                case bsoncxx::type::k_int32:

                    ship.longitude =
                        static_cast<double>(
                            longitude
                                .get_int32()
                                .value
                            );

                    break;

                case bsoncxx::type::k_int64:

                    ship.longitude =
                        static_cast<double>(
                            longitude
                                .get_int64()
                                .value
                            );

                    break;

                default:
                    break;
                }
            }

            /*
             * Speed
             */

            auto speed =
                document["speed"];

            if (speed)
            {
                switch (speed.type())
                {
                case bsoncxx::type::k_int32:

                    ship.speed =
                        speed
                            .get_int32()
                            .value;

                    break;

                case bsoncxx::type::k_int64:

                    ship.speed =
                        static_cast<int>(
                            speed
                                .get_int64()
                                .value
                            );

                    break;

                case bsoncxx::type::k_double:

                    ship.speed =
                        static_cast<int>(
                            speed
                                .get_double()
                                .value
                            );

                    break;

                default:
                    break;
                }
            }

            /*
             * Course
             */

            auto course =
                document["course"];

            if (course)
            {
                switch (course.type())
                {
                case bsoncxx::type::k_int32:

                    ship.course =
                        course
                            .get_int32()
                            .value;

                    break;

                case bsoncxx::type::k_int64:

                    ship.course =
                        static_cast<int>(
                            course
                                .get_int64()
                                .value
                            );

                    break;

                case bsoncxx::type::k_double:

                    ship.course =
                        static_cast<int>(
                            course
                                .get_double()
                                .value
                            );

                    break;

                default:
                    break;
                }
            }

            /*
             * Ship class
             */

            auto shipClass =
                document["shipClass"];

            if (shipClass &&
                shipClass.type() ==
                    bsoncxx::type::k_string)
            {
                ship.shipClass =
                    bsonStringToQString(
                        shipClass.get_string()
                        );
            }

            /*
             * Type
             */

            auto type =
                document["type"];

            if (type &&
                type.type() ==
                    bsoncxx::type::k_string)
            {
                ship.type =
                    bsonStringToQString(
                        type.get_string()
                        );
            }

            /*
             * Affiliation
             */

            auto affiliation =
                document["affiliation"];

            if (affiliation &&
                affiliation.type() ==
                    bsoncxx::type::k_string)
            {
                ship.affiliation =
                    bsonStringToQString(
                        affiliation.get_string()
                        );
            }

            ships.append(
                ship
                );
        }

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB getShips error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * UPDATE SHIP LOCATION
 * ============================================================
 */

bool database::updateShipLocation(
    const QString& id,
    double latitude,
    double longitude,
    int course)
{
    if (id.isEmpty())
    {
        return false;
    }

    if (!std::isfinite(latitude) ||
        !std::isfinite(longitude))
    {
        qDebug()
        << "Refusing to write invalid coordinates:"
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
        qDebug()
        << "Refusing to write out-of-range coordinates:"
        << id
        << latitude
        << longitude;

        return false;
    }

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto db =
            client[
                databaseName.toStdString()
        ];

        auto collection =
            db[
                collectionName.toStdString()
        ];

        bsoncxx::oid objectId{
            id.toStdString()
        };

        auto filter =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "_id",
                    objectId
                    )
                );

        auto update =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp(
                            "latitude",
                            latitude
                            ),
                        bsoncxx::builder::basic::kvp(
                            "longitude",
                            longitude
                            ),
                        bsoncxx::builder::basic::kvp(
                            "course",
                            course
                            )
                        )
                    )
                );

        auto result =
            collection.update_one(
                filter.view(),
                update.view()
                );

        if (!result)
        {
            qDebug()
            << "MongoDB update failed for ship:"
            << id;

            return false;
        }

        if (result->matched_count() == 0)
        {
            qDebug()
            << "Ship not found in MongoDB:"
            << id;

            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB updateShipLocation error:"
        << e.what();

        return false;
    }
}

/*
 * ============================================================
 * UPDATE SHIP SPEED
 * ============================================================
 */

bool database::updateShipSpeed(
    const QString& id,
    int speed)
{
    if (id.isEmpty())
    {
        return false;
    }

    if (speed < 0)
    {
        qDebug()
        << "Refusing to write invalid ship speed:"
        << id
        << speed;

        return false;
    }

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto db =
            client[
                databaseName.toStdString()
        ];

        auto collection =
            db[
                collectionName.toStdString()
        ];

        bsoncxx::oid objectId{
            id.toStdString()
        };

        auto filter =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "_id",
                    objectId
                    )
                );

        auto update =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp(
                            "speed",
                            speed
                            )
                        )
                    )
                );

        auto result =
            collection.update_one(
                filter.view(),
                update.view()
                );

        if (!result)
        {
            qDebug()
            << "MongoDB speed update failed for ship:"
            << id;

            return false;
        }

        if (result->matched_count() == 0)
        {
            qDebug()
            << "Ship not found while updating speed:"
            << id;

            return false;
        }

        qDebug()
            << "Updated ship speed:"
            << id
            << speed;

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB updateShipSpeed error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * SAVE NEW ROUTE
 * ============================================================
 */

bool database::saveRoute(
    const QString& shipId,
    const QString& routeId,
    double currentLat,
    double currentLon,
    const QVariantList& path)
{
    if (shipId.isEmpty())
    {
        return false;
    }

    if (!std::isfinite(currentLat) ||
        !std::isfinite(currentLon))
    {
        return false;
    }

    if (currentLat < -90.0 ||
        currentLat > 90.0 ||
        currentLon < -180.0 ||
        currentLon > 180.0)
    {
        return false;
    }

    const QString actualRouteId =
        routeId.trimmed().isEmpty()
            ? generateFallbackRouteId()
            : routeId.trimmed();

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto db =
            client[
                databaseName.toStdString()
        ];

        auto routesCollection =
            db[
                routesCollectionName.toStdString()
        ];

        auto shipsCollection =
            db[
                collectionName.toStdString()
        ];

        /*
         * Build path.
         */

        bsoncxx::builder::basic::array pathArray;

        for (const QVariant& pointVariant :
             path)
        {
            const QVariantMap point =
                pointVariant.toMap();

            const double latitude =
                point.value(
                         "latitude")
                    .toDouble();

            const double longitude =
                point.value(
                         "longitude")
                    .toDouble();

            pathArray.append(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp(
                        "latitude",
                        latitude
                        ),

                    bsoncxx::builder::basic::kvp(
                        "longitude",
                        longitude
                        )
                    )
                );
        }

        /*
         * ------------------------------------------------------
         * ALWAYS INSERT.
         * ------------------------------------------------------
         */

        const auto document =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "routeId",
                    actualRouteId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "shipId",
                    shipId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "status",
                    "active"
                    ),

                bsoncxx::builder::basic::kvp(
                    "currentLat",
                    currentLat
                    ),

                bsoncxx::builder::basic::kvp(
                    "currentLon",
                    currentLon
                    ),

                bsoncxx::builder::basic::kvp(
                    "startTime",
                    QDateTime::currentDateTime()
                        .toString(
                            "dd/MM/yyyy HH:mm:ss")
                        .toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "startEpoch",
                    static_cast<int64_t>(
                        QDateTime::currentMSecsSinceEpoch())
                    ),

                bsoncxx::builder::basic::kvp(
                    "path",
                    pathArray.view()
                    )
                );

        const auto insertResult =
            routesCollection.insert_one(
                document.view()
                );

        if (!insertResult)
        {
            qDebug()
            << "Could not insert new route:"
            << actualRouteId;

            return false;
        }

        /*
         * ------------------------------------------------------
         * IMPORTANT:
         *
         * Delete EVERY older active route belonging to this
         * ship except the document we just inserted.
         *
         * This works correctly even when an automatic reroute
         * reuses the same routeId.
         * ------------------------------------------------------
         */

        if (insertResult->inserted_id().type() ==
            bsoncxx::type::k_oid)
        {
            const bsoncxx::oid newRouteObjectId =
                insertResult
                    ->inserted_id()
                    .get_oid()
                    .value;

            const auto oldActiveFilter =
                bsoncxx::builder::basic::make_document(

                    bsoncxx::builder::basic::kvp(
                        "shipId",
                        shipId.toStdString()
                        ),

                    bsoncxx::builder::basic::kvp(
                        "status",
                        "active"
                        ),

                    bsoncxx::builder::basic::kvp(
                        "_id",
                        bsoncxx::builder::basic::make_document(
                            bsoncxx::builder::basic::kvp(
                                "$ne",
                                newRouteObjectId
                                )
                            )
                        )
                    );

            const auto deleteResult =
                routesCollection.delete_many(
                    oldActiveFilter.view()
                    );

            if (deleteResult)
            {
                qDebug()
                << "New route saved:"
                << actualRouteId
                << "ship:"
                << shipId
                << "old active routes deleted:"
                << deleteResult->deleted_count();
            }
        }

        /*
         * ------------------------------------------------------
         * Synchronize ships collection.
         *
         * This is the important part for preventing the normal
         * ship refresh from restoring an old starting position.
         * ------------------------------------------------------
         */

        bsoncxx::oid shipObjectId{
            shipId.toStdString()
        };

        const auto shipFilter =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "_id",
                    shipObjectId
                    )
                );

        const auto shipUpdate =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(

                        bsoncxx::builder::basic::kvp(
                            "latitude",
                            currentLat
                            ),

                        bsoncxx::builder::basic::kvp(
                            "longitude",
                            currentLon
                            )
                        )
                    )
                );

        const auto shipUpdateResult =
            shipsCollection.update_one(
                shipFilter.view(),
                shipUpdate.view()
                );

        if (!shipUpdateResult ||
            shipUpdateResult->matched_count() == 0)
        {
            qDebug()
            << "WARNING: route saved but initial ship"
            << "position could not be synchronized:"
            << shipId;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB saveRoute error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * UPDATE ROUTE PROGRESS
 * ============================================================
 *
 * IMPORTANT FIX:
 *
 * Every route progress update now updates TWO places:
 *
 * 1. routes.currentLat/currentLon
 * 2. ships.latitude/longitude
 *
 * The second update prevents the normal ship refresh from
 * moving the marker back to its old position.
 * ============================================================
 */

bool database::updateRouteProgress(
    const QString& shipId,
    const QString& routeId,
    double currentLat,
    double currentLon,
    int course)
{
    if (shipId.isEmpty() ||
        routeId.isEmpty())
    {
        return false;
    }

    if (!std::isfinite(currentLat) ||
        !std::isfinite(currentLon))
    {
        return false;
    }

    if (currentLat < -90.0 ||
        currentLat > 90.0 ||
        currentLon < -180.0 ||
        currentLon > 180.0)
    {
        return false;
    }

    if (course < 0 || course > 359)
    {
        qDebug()
        << "Invalid course:"
        << shipId
        << course;

        return false;
    }

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto db =
            client[
                databaseName.toStdString()
        ];

        auto routesCollection =
            db[
                routesCollectionName.toStdString()
        ];

        auto shipsCollection =
            db[
                collectionName.toStdString()
        ];

        /*
         * ------------------------------------------------------
         * Update the active route.
         * ------------------------------------------------------
         */

        const auto routeFilter =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "shipId",
                    shipId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "routeId",
                    routeId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "status",
                    "active"
                    )
                );

        const auto routeUpdate =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(

                        bsoncxx::builder::basic::kvp(
                            "currentLat",
                            currentLat
                            ),

                        bsoncxx::builder::basic::kvp(
                            "currentLon",
                            currentLon
                            )
                        )
                    )
                );

        const auto routeResult =
            routesCollection.update_one(
                routeFilter.view(),
                routeUpdate.view()
                );

        if (!routeResult)
        {
            qDebug()
            << "Route progress database update failed:"
            << shipId
            << routeId;

            return false;
        }

        if (routeResult->matched_count() == 0)
        {
            qDebug()
            << "No active route matched progress update:"
            << shipId
            << routeId;

            return false;
        }

        /*
         * ------------------------------------------------------
         * Update the ship.
         *
         * THIS is where course must be written.
         * ------------------------------------------------------
         */

        bsoncxx::oid shipObjectId{
            shipId.toStdString()
        };

        const auto shipFilter =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "_id",
                    shipObjectId
                    )
                );

        const auto shipUpdate =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(

                        bsoncxx::builder::basic::kvp(
                            "latitude",
                            currentLat
                            ),

                        bsoncxx::builder::basic::kvp(
                            "longitude",
                            currentLon
                            ),

                        bsoncxx::builder::basic::kvp(
                            "course",
                            course
                            )
                        )
                    )
                );

        const auto shipResult =
            shipsCollection.update_one(
                shipFilter.view(),
                shipUpdate.view()
                );

        if (!shipResult)
        {
            qDebug()
            << "WARNING: route progress updated but ship"
            << "database update failed:"
            << shipId;

            return false;
        }

        if (shipResult->matched_count() == 0)
        {
            qDebug()
            << "WARNING: route progress updated but ship"
            << "was not found:"
            << shipId;

            return false;
        }

        qDebug()
            << "Route progress saved:"
            << shipId
            << "lat:"
            << currentLat
            << "lon:"
            << currentLon
            << "course:"
            << course;

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB updateRouteProgress error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * MARK ROUTE FINISHED
 * ============================================================
 *
 * IMPORTANT FIX:
 *
 * When the route finishes:
 *
 * 1. Mark route as finished.
 * 2. Write the final coordinate into ships.
 *
 * Therefore, after m_activeRoutes removes the route, the next
 * normal database refresh still sees the correct final position.
 * ============================================================
 */

bool database::markRouteFinished(
    const QString& shipId,
    const QString& routeId,
    double finalLat,
    double finalLon,
    int stoppedWaypointIndex,
    int course)
{
    Q_UNUSED(stoppedWaypointIndex);
    if (shipId.isEmpty() || routeId.isEmpty()) return false;
    if (!std::isfinite(finalLat) || !std::isfinite(finalLon)) return false;
    if (finalLat < -90.0 || finalLat > 90.0 || finalLon < -180.0 || finalLon > 180.0) return false;

    try
    {
        mongocxx::client client(mongocxx::uri(mongoUri.toStdString()));
        auto db = client[databaseName.toStdString()];
        auto routesCollection = db[routesCollectionName.toStdString()];
        auto shipsCollection = db[collectionName.toStdString()];

        const auto routeFilter = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("shipId", shipId.toStdString()),
            bsoncxx::builder::basic::kvp("routeId", routeId.toStdString()),
            bsoncxx::builder::basic::kvp("status", "active")
            );

        auto existingRoute = routesCollection.find_one(routeFilter.view());
        bsoncxx::builder::basic::array truncatedPathArray;
        bool pathTruncated = false;

        if (existingRoute)
        {
            auto pathElement = existingRoute->view()["path"];
            if (pathElement && pathElement.type() == bsoncxx::type::k_array)
            {
                auto pathArr = pathElement.get_array().value;

                double startLat = finalLat;
                double startLon = finalLon;
                bool hasStart = false;

                // 1. Get the starting coordinate of the route
                for (const auto& pt : pathArr)
                {
                    if (pt.type() == bsoncxx::type::k_document)
                    {
                        auto doc = pt.get_document().view();
                        if (doc["latitude"] && doc["longitude"])
                        {
                            startLat = extractBsonDouble(doc["latitude"]);
                            startLon = extractBsonDouble(doc["longitude"]);
                            hasStart = true;
                            break;
                        }
                    }
                }

                QGeoCoordinate startCoord(startLat, startLon);
                QGeoCoordinate finalCoord(finalLat, finalLon);

                // Maximum allowable distance from start to stop point
                double maxDistFromStart = hasStart ? startCoord.distanceTo(finalCoord) : 0.0;

                // 2. Only include waypoints strictly between Start and the Final Stop Position
                for (const auto& pt : pathArr)
                {
                    if (pt.type() == bsoncxx::type::k_document)
                    {
                        auto doc = pt.get_document().view();
                        if (doc["latitude"] && doc["longitude"])
                        {
                            double lat = extractBsonDouble(doc["latitude"]);
                            double lon = extractBsonDouble(doc["longitude"]);

                            QGeoCoordinate ptCoord(lat, lon);

                            // If this point is further from the start than the final stop position, stop adding points!
                            if (!hasStart || startCoord.distanceTo(ptCoord) <= maxDistFromStart + 15.0) // 15m tolerance
                            {
                                truncatedPathArray.append(bsoncxx::types::b_document{doc});
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                pathTruncated = true;
            }
        }

        // 3. Always ensure the exact final stop position is the final point in the path
        if (pathTruncated)
        {
            truncatedPathArray.append(
                bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("latitude", finalLat),
                    bsoncxx::builder::basic::kvp("longitude", finalLon)
                    )
                );
        }

        bsoncxx::builder::basic::document setBuilder{};
        setBuilder.append(
            bsoncxx::builder::basic::kvp("status", "finished"),
            bsoncxx::builder::basic::kvp("currentLat", finalLat),
            bsoncxx::builder::basic::kvp("currentLon", finalLon),
            bsoncxx::builder::basic::kvp(
                "endTime",
                QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss").toStdString()
                )
            );

        if (pathTruncated)
        {
            setBuilder.append(bsoncxx::builder::basic::kvp("path", truncatedPathArray.view()));
        }

        const auto routeUpdate = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("$set", setBuilder.view())
            );

        routesCollection.update_one(routeFilter.view(), routeUpdate.view());

        // Synchronize final ship position
        bsoncxx::oid shipObjectId{ shipId.toStdString() };
        const auto shipFilter = bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("_id", shipObjectId));
        const auto shipUpdate =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "$set",
                    bsoncxx::builder::basic::make_document(
                        bsoncxx::builder::basic::kvp(
                            "latitude",
                            finalLat
                            ),

                        bsoncxx::builder::basic::kvp(
                            "longitude",
                            finalLon
                            ),

                        bsoncxx::builder::basic::kvp(
                            "course",
                            course
                            )
                        )
                    )
                );
        shipsCollection.update_one(shipFilter.view(), shipUpdate.view());

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug() << "MongoDB markRouteFinished error:" << e.what();
        return false;
    }
}


/*
 * ============================================================
 * DELETE ACTIVE ROUTE
 * ============================================================
 */

bool database::deleteRoute(
    const QString& shipId,
    const QString& routeId)
{
    if (shipId.isEmpty() ||
        routeId.isEmpty())
    {
        return false;
    }

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto collection =
            client[
                databaseName.toStdString()
        ][
                routesCollectionName.toStdString()
        ];

        const auto filter =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "shipId",
                    shipId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "routeId",
                    routeId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "status",
                    "active"
                    )
                );

        const auto result =
            collection.delete_one(
                filter.view()
                );

        if (!result)
        {
            return false;
        }

        qDebug()
            << "Deleted active route:"
            << shipId
            << routeId;

        return result->deleted_count() > 0;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB deleteRoute error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * GET ACTIVE ROUTES
 * ============================================================
 */

bool database::getActiveRoutes(
    QHash<QString, ActiveRouteData>& routes)
{
    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto collection =
            client[
                databaseName.toStdString()
        ][
                routesCollectionName.toStdString()
        ];

        /*
         * IMPORTANT:
         *
         * Only "active".
         */

        const auto filter =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "status",
                    "active"
                    )
                );

        auto cursor =
            collection.find(
                filter.view()
                );

        routes.clear();

        for (const auto& doc :
             cursor)
        {
            auto shipIdElement =
                doc["shipId"];

            if (!shipIdElement ||
                shipIdElement.type() !=
                    bsoncxx::type::k_string)
            {
                continue;
            }

            ActiveRouteData route;

            route.shipId =
                bsonStringToQString(
                    shipIdElement.get_string()
                    );

            /*
             * routeId
             */

            auto routeIdElement =
                doc["routeId"];

            if (routeIdElement &&
                routeIdElement.type() ==
                    bsoncxx::type::k_string)
            {
                route.routeId =
                    bsonStringToQString(
                        routeIdElement.get_string()
                        );
            }

            /*
             * Legacy documents without routeId.
             */

            if (route.routeId.isEmpty())
            {
                auto mongoId =
                    doc["_id"];

                if (mongoId &&
                    mongoId.type() ==
                        bsoncxx::type::k_oid)
                {
                    route.routeId =
                        QString::fromStdString(
                            mongoId
                                .get_oid()
                                .value
                                .to_string()
                            );
                }
            }

            if (route.routeId.isEmpty())
            {
                continue;
            }

            /*
             * Current latitude
             */

            auto currentLat =
                doc["currentLat"];

            if (currentLat &&
                currentLat.type() ==
                    bsoncxx::type::k_double)
            {
                route.currentLat =
                    currentLat
                        .get_double()
                        .value;
            }
            else if (
                currentLat &&
                currentLat.type() ==
                    bsoncxx::type::k_int32)
            {
                route.currentLat =
                    static_cast<double>(
                        currentLat
                            .get_int32()
                            .value
                        );
            }
            else if (
                currentLat &&
                currentLat.type() ==
                    bsoncxx::type::k_int64)
            {
                route.currentLat =
                    static_cast<double>(
                        currentLat
                            .get_int64()
                            .value
                        );
            }

            /*
             * Current longitude
             */

            auto currentLon =
                doc["currentLon"];

            if (currentLon &&
                currentLon.type() ==
                    bsoncxx::type::k_double)
            {
                route.currentLon =
                    currentLon
                        .get_double()
                        .value;
            }
            else if (
                currentLon &&
                currentLon.type() ==
                    bsoncxx::type::k_int32)
            {
                route.currentLon =
                    static_cast<double>(
                        currentLon
                            .get_int32()
                            .value
                        );
            }
            else if (
                currentLon &&
                currentLon.type() ==
                    bsoncxx::type::k_int64)
            {
                route.currentLon =
                    static_cast<double>(
                        currentLon
                            .get_int64()
                            .value
                        );
            }

            /*
             * Path
             */

            auto pathElement =
                doc["path"];

            if (pathElement &&
                pathElement.type() ==
                    bsoncxx::type::k_array)
            {
                const auto pathArr =
                    pathElement
                        .get_array()
                        .value;

                for (const auto& element :
                     pathArr)
                {
                    if (element.type() !=
                        bsoncxx::type::k_document)
                    {
                        continue;
                    }

                    const auto pointDoc =
                        element
                            .get_document()
                            .view();

                    auto latElement =
                        pointDoc["latitude"];

                    auto lonElement =
                        pointDoc["longitude"];

                    if (!latElement ||
                        !lonElement)
                    {
                        continue;
                    }

                    double latitude = 0.0;
                    double longitude = 0.0;

                    if (latElement.type() ==
                        bsoncxx::type::k_double)
                    {
                        latitude =
                            latElement
                                .get_double()
                                .value;
                    }
                    else if (
                        latElement.type() ==
                        bsoncxx::type::k_int32)
                    {
                        latitude =
                            static_cast<double>(
                                latElement
                                    .get_int32()
                                    .value
                                );
                    }
                    else if (
                        latElement.type() ==
                        bsoncxx::type::k_int64)
                    {
                        latitude =
                            static_cast<double>(
                                latElement
                                    .get_int64()
                                    .value
                                );
                    }

                    if (lonElement.type() ==
                        bsoncxx::type::k_double)
                    {
                        longitude =
                            lonElement
                                .get_double()
                                .value;
                    }
                    else if (
                        lonElement.type() ==
                        bsoncxx::type::k_int32)
                    {
                        longitude =
                            static_cast<double>(
                                lonElement
                                    .get_int32()
                                    .value
                                );
                    }
                    else if (
                        lonElement.type() ==
                        bsoncxx::type::k_int64)
                    {
                        longitude =
                            static_cast<double>(
                                lonElement
                                    .get_int64()
                                    .value
                                );
                    }

                    QVariantMap point;

                    point["latitude"] =
                        latitude;

                    point["longitude"] =
                        longitude;

                    route.path.append(
                        point
                        );
                }
            }

            /*
             * One active route per ship.
             *
             * saveRoute() now guarantees this, but keeping the
             * QHash behavior here also protects against older
             * duplicate documents in MongoDB.
             */
            routes.insert(
                route.shipId,
                route
                );
        }

        qDebug()
            << "Loaded active routes:"
            << routes.size();

        return true;
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB getActiveRoutes error:"
        << e.what();

        return false;
    }
}


/*
 * ============================================================
 * GET FINISHED ROUTE HISTORY
 * ============================================================
 */

QVariantList database::getFinishedRoutesForShip(
    const QString& shipId)
{
    QVariantList result;

    if (shipId.isEmpty())
    {
        return result;
    }

    try
    {
        mongocxx::client client(
            mongocxx::uri(
                mongoUri.toStdString()
                )
            );

        auto collection =
            client[
                databaseName.toStdString()
        ][
                routesCollectionName.toStdString()
        ];

        const auto filter =
            bsoncxx::builder::basic::make_document(

                bsoncxx::builder::basic::kvp(
                    "shipId",
                    shipId.toStdString()
                    ),

                bsoncxx::builder::basic::kvp(
                    "status",
                    "finished"
                    )
                );

        mongocxx::options::find options;

        const auto sortDocument =
            bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp(
                    "startEpoch",
                    1
                    )
                );

        options.sort(
            sortDocument.view()
            );

        auto cursor =
            collection.find(
                filter.view(),
                options
                );

        for (const auto& doc :
             cursor)
        {
            QVariantMap routeMap;

            /*
             * routeId
             */

            auto routeId =
                doc["routeId"];

            if (routeId &&
                routeId.type() ==
                    bsoncxx::type::k_string)
            {
                routeMap["routeId"] =
                    bsonStringToQString(
                        routeId.get_string()
                        );
            }
            else
            {
                auto mongoId =
                    doc["_id"];

                if (mongoId &&
                    mongoId.type() ==
                        bsoncxx::type::k_oid)
                {
                    routeMap["routeId"] =
                        QString::fromStdString(
                            mongoId
                                .get_oid()
                                .value
                                .to_string()
                            );
                }
            }

            /*
             * startTime
             */

            auto startTime =
                doc["startTime"];

            if (startTime &&
                startTime.type() ==
                    bsoncxx::type::k_string)
            {
                routeMap["startTime"] =
                    bsonStringToQString(
                        startTime.get_string()
                        );
            }
            else
            {
                routeMap["startTime"] =
                    "Unknown Start";
            }

            /*
             * endTime
             */

            auto endTime =
                doc["endTime"];

            if (endTime &&
                endTime.type() ==
                    bsoncxx::type::k_string)
            {
                routeMap["endTime"] =
                    bsonStringToQString(
                        endTime.get_string()
                        );
            }
            else
            {
                routeMap["endTime"] =
                    "Unknown End";
            }

            /*
             * Path
             */

            QVariantList pathList;

            auto pathElement =
                doc["path"];

            if (pathElement &&
                pathElement.type() ==
                    bsoncxx::type::k_array)
            {
                const auto pathArr =
                    pathElement
                        .get_array()
                        .value;

                for (const auto& element :
                     pathArr)
                {
                    if (element.type() !=
                        bsoncxx::type::k_document)
                    {
                        continue;
                    }

                    const auto pointDoc =
                        element
                            .get_document()
                            .view();

                    auto lat =
                        pointDoc["latitude"];

                    auto lon =
                        pointDoc["longitude"];

                    if (!lat || !lon)
                    {
                        continue;
                    }

                    double latitude = 0.0;
                    double longitude = 0.0;

                    if (lat.type() ==
                        bsoncxx::type::k_double)
                    {
                        latitude =
                            lat
                                .get_double()
                                .value;
                    }
                    else if (
                        lat.type() ==
                        bsoncxx::type::k_int32)
                    {
                        latitude =
                            static_cast<double>(
                                lat
                                    .get_int32()
                                    .value
                                );
                    }
                    else if (
                        lat.type() ==
                        bsoncxx::type::k_int64)
                    {
                        latitude =
                            static_cast<double>(
                                lat
                                    .get_int64()
                                    .value
                                );
                    }

                    if (lon.type() ==
                        bsoncxx::type::k_double)
                    {
                        longitude =
                            lon
                                .get_double()
                                .value;
                    }
                    else if (
                        lon.type() ==
                        bsoncxx::type::k_int32)
                    {
                        longitude =
                            static_cast<double>(
                                lon
                                    .get_int32()
                                    .value
                                );
                    }
                    else if (
                        lon.type() ==
                        bsoncxx::type::k_int64)
                    {
                        longitude =
                            static_cast<double>(
                                lon
                                    .get_int64()
                                    .value
                                );
                    }

                    QVariantMap point;

                    point["latitude"] =
                        latitude;

                    point["longitude"] =
                        longitude;

                    pathList.append(
                        point
                        );
                }
            }

            routeMap["path"] =
                pathList;

            routeMap["display"] =
                routeMap["startTime"].toString()
                +
                "  ->  "
                +
                routeMap["endTime"].toString();

            result.append(
                routeMap
                );
        }
    }
    catch (const std::exception& e)
    {
        qDebug()
        << "MongoDB getFinishedRoutes error:"
        << e.what();
    }

    return result;
}
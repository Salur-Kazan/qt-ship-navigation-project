#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <QObject>
#include <QGeoCoordinate>
#include <QString>
#include <QHash>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <QDataStream>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QPointF>

#include <h3/h3api.h>

#include <array>
#include <QThreadPool>
#include <QVariantList>

#include <mutex>
#include <QSet>
#include <QtGlobal>

class GDALDataset;
class OGRLayer;
class OGRGeometry;
class OGRPoint;

class Navigation : public QObject
{
    Q_OBJECT

public:

    explicit Navigation(QObject *parent = nullptr);
    ~Navigation();

    /*
     * Water polygons
     */

    Q_INVOKABLE bool loadWaterPolygons(
        const QString &shpPath);

    Q_INVOKABLE bool isWater(
        double latitude,
        double longitude) const;

    Q_INVOKABLE QGeoCoordinate validatePosition(
        double latitude,
        double longitude);

    /*
     * H3 / Pathfinding
     */

    Q_INVOKABLE QVariantList h3DebugCells() const;

    Q_INVOKABLE void setGlobalMaxResolution(
        int maxRes);

    Q_INVOKABLE void addRefinementZone(
        double minLat,
        double minLon,
        double maxLat,
        double maxLon,
        int maxResolution);

    Q_INVOKABLE void refineGrid();

    Q_INVOKABLE QVariantList findPath(
        double startLatitude,
        double startLongitude,
        double targetLatitude,
        double targetLongitude,
        double cellSizeMeters = 500.0);

    Q_INVOKABLE void previewPath(
        const QString &shipId,
        double startLatitude,
        double startLongitude,
        double targetLatitude,
        double targetLongitude);

    /*
     * User-created navigation.
     *
     * isReroute defaults to false so existing QML calls
     * with six arguments continue to work.
     */
    Q_INVOKABLE bool startNavigation(
        const QString &shipId,
        double startLatitude,
        double startLongitude,
        double targetLatitude,
        double targetLongitude,
        double speedKnots,
        bool isReroute = false);

    Q_INVOKABLE void stopNavigation(
        const QString &shipId);

    Q_INVOKABLE QVariantList h3DebugCellsNear(
        double centerLat,
        double centerLon,
        int maxCount = 500) const;

    /*
     * Async water operations.
     */

    void loadWaterPolygonsAsync(
        const QString &shpPath
        );

    void isWaterAsync(
        double latitude,
        double longitude
        );

    void validatePositionAsync(
        double latitude,
        double longitude
        );


private:

    // ============================================================
    // H3 SEA GRAPH
    // ============================================================

    enum class CellState : quint8
    {
        Unknown = 0,
        PureWater = 1,
        Mixed = 2,
        Land = 3
    };

    struct SeaCell
    {
        H3Index h3 = H3_NULL;

        int resolution = 0;

        CellState state =
            CellState::Unknown;

        QGeoCoordinate center;
    };


    // ============================================================
    // ACTIVE ROUTE
    // ============================================================

    struct ActiveRoute
    {
        /*
         * UNIQUE DATABASE ROUTE ID.
         *
         * New user route:
         *     new UUID
         *
         * Automatic collision reroute:
         *     same UUID
         */
        QString routeId;

        QVector<QGeoCoordinate> path;

        /*
         * Index of next waypoint.
         */
        int waypointIndex = 1;

        double speedMetersPerSecond = 0.0;

        /*
         * Actual live ship position.
         *
         * This is the source of truth for movement and
         * database progress.
         */
        QGeoCoordinate currentPosition;

        /*
         * Collision avoidance state.
         */
        bool isRerouting = false;

        bool isWaiting = false;

        int rerouteCooldown = 0;

        /*
         * Last database progress save.
         */
        qint64 lastDbSaveTime = 0;
    };


    static constexpr int H3_BASE_RESOLUTION = 7;


    // ============================================================
    // REFINEMENT ZONES
    // ============================================================

    struct RefinementZone
    {
        double minLat;
        double minLon;
        double maxLat;
        double maxLon;
        int maxResolution;
    };

    int m_maxResolution = 8;

    QVector<RefinementZone>
        m_refinementZones;


    // ============================================================
    // TEST BOUNDING BOX
    // ============================================================

    static constexpr bool H3_TEST_BBOX_ONLY = true;

    static constexpr double H3_TEST_MIN_LAT = 30.0;

    static constexpr double H3_TEST_MAX_LAT = 48.0;

    static constexpr double H3_TEST_MIN_LON = -6.0;

    static constexpr double H3_TEST_MAX_LON = 42.0;


    // ============================================================
    // H3 GRAPH
    // ============================================================

    QVector<SeaCell> m_cells;

    QHash<H3Index, int>
        m_h3Index;

    QHash<int, QVector<int>>
        m_neighbors;

    QString m_gridCachePath;

    QString m_gridSourcePath;


    // ============================================================
    // H3 GRAPH INITIALIZATION
    // ============================================================

    void initializeGlobalGrid(
        const QString &shpPath);

    void buildGlobalGrid();

    QVector<H3Index> initialH3CellsForWorld(
        int resolution) const;

    QVector<H3Index> initialH3CellsForBBox(
        double minLatitude,
        double minLongitude,
        double maxLatitude,
        double maxLongitude,
        int resolution) const;

    bool refineH3Cell(
        int cellIndex);

    bool classifyH3Cell(
        int cellIndex);

    bool isH3CellWater(
        int cellIndex) const;

    QVector<H3Index> h3Children(
        H3Index parent,
        int childResolution) const;

    QGeoCoordinate h3CellCenter(
        H3Index cell) const;

    bool h3CellContains(
        H3Index cell,
        const QGeoCoordinate &coordinate) const;

    QVector<H3Index> h3Neighbors(
        H3Index cell) const;

    void rebuildAdjacency();

    void addNeighbor(
        int first,
        int second);

    void addCrossResolutionNeighbors(
        int cellIndex);

    int findLeafContaining(
        const QGeoCoordinate &coordinate) const;

    bool cellIsTraversable(
        int cellIndex) const;

    double distanceBetweenCells(
        int first,
        int second) const;

    double heuristic(
        int first,
        int goal) const;

    QVector<int> runAStar(
        int startCell,
        int goalCell,
        const QSet<int> &blockedCells);

    bool prepareEndpoints(
        const QGeoCoordinate &start,
        const QGeoCoordinate &goal);

    bool ensureEndpointCell(
        const QGeoCoordinate &coordinate) const;

    bool refineCellsUsedByPath(
        const QVector<int> &cellPath);

    QVector<QGeoCoordinate>
    convertCellPathToCoordinates(
        const QVector<int> &cellPath,
        const QGeoCoordinate &start,
        const QGeoCoordinate &goal) const;


    // ============================================================
    // CACHE
    // ============================================================

    QString defaultGridCachePath(
        const QString &shpPath) const;

    QString gridSourceSignature(
        const QString &sourcePath) const;

    bool loadGridCache(
        const QString &cachePath,
        const QString &sourcePath);

    bool saveGridCache(
        const QString &cachePath,
        const QString &sourcePath);


    // ============================================================
    // PARALLEL GRID CLASSIFICATION
    // ============================================================

    struct GridClassification
    {
        H3Index h3 = H3_NULL;

        CellState state =
            CellState::Unknown;
    };

    static bool isWaterOnLayer(
        OGRLayer *layer,
        double latitude,
        double longitude);

    static CellState classifyH3IndexOnLayer(
        OGRLayer *layer,
        H3Index cell);


    // ============================================================
    // EXISTING WATER FUNCTIONS
    // ============================================================

    bool isLoaded() const;

    static bool validCoordinates(
        double latitude,
        double longitude);

    QGeoCoordinate findNearestWater(
        double latitude,
        double longitude,
        double maxSearchKm = 100.0);

    QGeoCoordinate findNearestWaterInternal(
        const QGeoCoordinate &origin,
        double maxSearchKm);

    bool isSegmentWater(
        const QGeoCoordinate &start,
        const QGeoCoordinate &end) const;

    bool validatePath(
        const QVector<QGeoCoordinate> &path) const;

    static void radiusToDegrees(
        double latitude,
        double radiusKm,
        double &latitudeDegrees,
        double &longitudeDegrees);

    bool closestPointOnGeometry(
        const OGRGeometry *geometry,
        const OGRPoint &source,
        OGRPoint &closestPoint);

    bool closestPointOnLineString(
        const OGRGeometry *geometry,
        const OGRPoint &source,
        OGRPoint &closestPoint);

    static void closestPointOnSegment(
        double px,
        double py,
        double x1,
        double y1,
        double x2,
        double y2,
        double &resultX,
        double &resultY);


    // ============================================================
    // MOVEMENT
    // ============================================================

    QTimer m_navigationTimer;

    /*
     * One currently active route per ship.
     */
    QHash<QString, ActiveRoute>
        m_activeRoutes;

    /*
     * Each user route request gets an increasing generation.
     *
     * If an old A* calculation finishes after a newer request,
     * its result is discarded.
     */
    QHash<QString, quint64>
        m_routeRequestGeneration;


    // ============================================================
    // GDAL
    // ============================================================

    GDALDataset *m_dataset = nullptr;

    OGRLayer *m_waterLayer = nullptr;


    // ============================================================
    // NAVIGATION WORKERS
    // ============================================================

    void finishNavigation(
        const QString &shipId,
        quint64 requestGeneration,
        bool isReroute,
        double speedMetersPerSecond,
        const QVariantList &variantPath);

    QThreadPool m_navigationPool;

    QThreadPool m_gridPool;

    mutable std::recursive_mutex
        m_navigationMutex;

    mutable std::recursive_mutex
        m_routesMutex;


    QVariantList findPathInternal(
        double startLatitude,
        double startLongitude,
        double targetLatitude,
        double targetLongitude,
        const QSet<int> &blockedCells);


private slots:

    void updateNavigation();


public slots:

    void restoreNavigation(
        const QString& shipId,
        const QString& routeId,
        double currentLat,
        double currentLon,
        int shipSpeed,
        const QVariantList& path);

    QVariantList getShipRouteHistory(
        const QString& shipId);


signals:

    void pathReady(
        const QString &shipId,
        const QVariantList &path);

    void previewPathReady(
        const QString &shipId,
        const QVariantList &path);

    void shipPositionChanged(
        const QString &shipId,
        double latitude,
        double longitude,
        int course);

    void navigationFinished(
        const QString &shipId);

    void navigationFailed(
        const QString &shipId,
        const QString &reason);


    void waterPolygonsLoaded(
        bool success
        );

    void waterCheckFinished(
        double latitude,
        double longitude,
        bool water
        );

    void positionValidationFinished(
        double requestedLatitude,
        double requestedLongitude,
        double resultLatitude,
        double resultLongitude,
        bool valid
        );


    // ============================================================
    // ROUTE DATABASE SIGNALS
    // ============================================================

    /*
     * NEW user-created route.
     */
    void routeStartedNeedsSave(
        const QString& shipId,
        const QString& routeId,
        double lat,
        double lon,
        const QVariantList& path
        );

    /*
     * Update exact route.
     */
    void routeProgressNeedsSave(
        const QString& shipId,
        const QString& routeId,
        double lat,
        double lon,
        int course
        );

    /*
     * Mark exact route finished.
     */
signals:
    void routeFinishedNeedsClear(const QString& shipId, const QString& routeId, double finalLat, double finalLon, int stoppedWaypointIndex, int course);

    /*
     * Delete exact active route because user cancelled it.
     */
    void routeCancelledNeedsDelete(
        const QString& shipId,
        const QString& routeId
        );
};

#endif // NAVIGATION_H
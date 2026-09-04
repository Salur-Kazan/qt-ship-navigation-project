#include "navigation.h"
#include "database.h"

#include <QDebug>
#include <QtMath>

#include <QDataStream>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>

#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

#include <QRunnable>
#include <QMetaObject>

#include <functional>
#include <mutex>
#include <utility>

#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <h3/h3api.h>


namespace
{

struct OpenNode
{
    int index = -1;
    double fCost = 0.0;

    bool operator>(
        const OpenNode &other) const
    {
        return fCost > other.fCost;
    }
};

class NavigationTask final : public QRunnable
{
public:

    explicit NavigationTask(
        std::function<void()> function
        )
        : m_function(std::move(function))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (m_function)
        {
            m_function();
        }
    }

private:

    std::function<void()> m_function;
};

}


// ============================================================
// CONSTRUCTOR
// ============================================================

Navigation::Navigation(QObject *parent)
    : QObject(parent)
{
    GDALAllRegister();

    /*
     * --------------------------------------------------------
     * Normal navigation pool.
     *
     * Keep this at 1 for now because your navigation/GDAL
     * state is shared.
     * --------------------------------------------------------
     */
    m_navigationPool.setMaxThreadCount(1);

    m_navigationPool.setExpiryTimeout(-1);

    /*
     * --------------------------------------------------------
     * Grid preprocessing pool.
     *
     * Your CPU has 16 physical cores.
     *
     * QThread::idealThreadCount() may report the number of
     * processors exposed to Linux/Qt. We cap it at 16 so we
     * don't create 22 simultaneous GDAL workers just because
     * the CPU reports 22 hardware threads.
     * --------------------------------------------------------
     */
    const int idealThreads =
        QThread::idealThreadCount();

    const int gridThreadCount =
        std::max(
            1,
            std::min(
                8,
                idealThreads > 0 ? idealThreads : 1));

    m_gridPool.setMaxThreadCount(
        gridThreadCount);

    m_gridPool.setExpiryTimeout(-1);

    qDebug()
        << "Navigation:"
        << "grid worker threads:"
        << m_gridPool.maxThreadCount();

    m_navigationTimer.setInterval(100);

    connect(
        &m_navigationTimer,
        &QTimer::timeout,
        this,
        &Navigation::updateNavigation);
}


// ============================================================
// DESTRUCTOR
// ============================================================

Navigation::~Navigation()
{
    /*
     * The grid pool must finish first.
     *
     * Grid workers open their own GDAL datasets, so we must
     * make sure every worker has closed its dataset before
     * destroying Navigation.
     */
    m_gridPool.waitForDone();

    /*
     * Then wait for normal navigation workers.
     */
    m_navigationPool.waitForDone();

    if (m_dataset)
    {
        GDALClose(m_dataset);

        m_dataset = nullptr;
        m_waterLayer = nullptr;
    }
}


// ============================================================
// LOAD WATER POLYGONS
// ============================================================

bool Navigation::loadWaterPolygons(
    const QString &shpPath)
{
    /*
     * Close an old dataset first.
     */
    if (m_dataset)
    {
        GDALClose(m_dataset);

        m_dataset = nullptr;
        m_waterLayer = nullptr;
    }

    const QByteArray pathBytes =
        shpPath.toUtf8();

    m_dataset =
        static_cast<GDALDataset *>(
            GDALOpenEx(
                pathBytes.constData(),
                GDAL_OF_VECTOR |
                    GDAL_OF_READONLY,
                nullptr,
                nullptr,
                nullptr));

    if (!m_dataset)
    {
        qWarning()
        << "Navigation:"
        << "could not open water Shapefile:"
        << shpPath;

        return false;
    }

    if (m_dataset->GetLayerCount() <= 0)
    {
        qWarning()
        << "Navigation:"
        << "Shapefile contains no layers.";

        GDALClose(m_dataset);

        m_dataset = nullptr;

        return false;
    }

    m_waterLayer =
        m_dataset->GetLayer(0);

    if (!m_waterLayer)
    {
        qWarning()
        << "Navigation:"
        << "could not access water layer.";

        GDALClose(m_dataset);

        m_dataset = nullptr;

        return false;
    }

    qDebug()
        << "Navigation:"
        << "water polygons loaded.";

    qDebug()
        << "Layer:"
        << m_waterLayer->GetName();

    qDebug()
        << "Features:"
        << static_cast<long long>(
               m_waterLayer->GetFeatureCount());

    const OGRSpatialReference *srs =
        m_waterLayer->GetSpatialRef();

    if (srs)
    {
        const char *authority =
            srs->GetAuthorityCode(
                nullptr);

        if (authority)
        {
            qDebug()
            << "Water SRS EPSG:"
            << authority;
        }
        else
        {
            qDebug()
            << "Water SRS:"
            << "authority code unavailable";
        }
    }

    /*
     * Initialize or load the persistent adaptive grid.
     *
     * This does NOT build a fine grid for the entire world.
     */
    initializeGlobalGrid(
        shpPath);

    return true;
}


// ============================================================
// IS LOADED
// ============================================================

bool Navigation::isLoaded() const
{
    return
        m_dataset != nullptr &&
        m_waterLayer != nullptr;
}


// ============================================================
// VALIDATE COORDINATES
// ============================================================

bool Navigation::validCoordinates(
    double latitude,
    double longitude)
{
    if (!std::isfinite(latitude) ||
        !std::isfinite(longitude))
    {
        return false;
    }

    if (latitude < -90.0 ||
        latitude > 90.0)
    {
        return false;
    }

    if (longitude < -180.0 ||
        longitude > 180.0)
    {
        return false;
    }

    return true;
}


// ============================================================
// IS WATER
// ============================================================

bool Navigation::isWater(
    double latitude,
    double longitude) const
{
    std::lock_guard<std::recursive_mutex> lock(
        m_navigationMutex);

    if (!validCoordinates(
            latitude,
            longitude))
    {
        return false;
    }

    if (!isLoaded())
    {
        qWarning()
        << "Navigation:"
        << "water polygons are not loaded.";

        return false;
    }

    /*
     * OGR:
     *
     * X = longitude
     * Y = latitude
     */

    OGRPoint point(
        longitude,
        latitude);

    /*
     * Only search polygons around this point.
     */
    m_waterLayer->SetSpatialFilter(
        &point);

    m_waterLayer->ResetReading();

    bool water = false;

    OGRFeature *feature = nullptr;

    while (
        (feature =
         m_waterLayer->GetNextFeature())
        != nullptr)
    {
        const OGRGeometry *geometry =
            feature->GetGeometryRef();

        if (geometry)
        {
            if (geometry->Intersects(
                    &point))
            {
                water = true;

                OGRFeature::DestroyFeature(
                    feature);

                break;
            }
        }

        OGRFeature::DestroyFeature(
            feature);
    }

    m_waterLayer->SetSpatialFilter(
        nullptr);

    m_waterLayer->ResetReading();

    return water;
}


// ============================================================
// VALIDATE POSITION
// ============================================================

QGeoCoordinate Navigation::validatePosition(
    double latitude,
    double longitude)
{
    std::lock_guard<std::recursive_mutex> lock(
        m_navigationMutex);

    QGeoCoordinate original(
        latitude,
        longitude);

    if (!original.isValid())
    {
        return {};
    }

    if (isWater(
            latitude,
            longitude))
    {
        return original;
    }

    qDebug()
        << "Navigation:"
        << "ship is on land at"
        << latitude
        << longitude;

    return findNearestWater(
        latitude,
        longitude);
}

// ============================================================
// FIND NEAREST WATER
// ============================================================

QGeoCoordinate Navigation::findNearestWater(
    double latitude,
    double longitude,
    double maxSearchKm)
{
    QGeoCoordinate origin(
        latitude,
        longitude);

    if (!origin.isValid())
    {
        return {};
    }

    return findNearestWaterInternal(
        origin,
        maxSearchKm);
}


// ============================================================
// FIND NEAREST WATER INTERNAL
// ============================================================

QGeoCoordinate Navigation::findNearestWaterInternal(
    const QGeoCoordinate &origin,
    double maxSearchKm)
{
    if (!isLoaded())
        return {};

    if (isWater(
            origin.latitude(),
            origin.longitude()))
    {
        return origin;
    }

    double radiusKm = 1.0;

    while (radiusKm <= maxSearchKm)
    {
        double latDegrees = 0.0;
        double lonDegrees = 0.0;

        radiusToDegrees(
            origin.latitude(),
            radiusKm,
            latDegrees,
            lonDegrees);

        const double minLon =
            std::max(
                -180.0,
                origin.longitude() -
                    lonDegrees);

        const double maxLon =
            std::min(
                180.0,
                origin.longitude() +
                    lonDegrees);

        const double minLat =
            std::max(
                -90.0,
                origin.latitude() -
                    latDegrees);

        const double maxLat =
            std::min(
                90.0,
                origin.latitude() +
                    latDegrees);

        m_waterLayer->SetSpatialFilterRect(
            minLon,
            minLat,
            maxLon,
            maxLat);

        m_waterLayer->ResetReading();

        OGRFeature *feature = nullptr;

        bool found = false;

        QGeoCoordinate bestCoordinate;

        double bestDistance =
            std::numeric_limits<double>::max();

        OGRPoint source(
            origin.longitude(),
            origin.latitude());

        while (
            (feature =
             m_waterLayer->GetNextFeature())
            != nullptr)
        {
            const OGRGeometry *geometry =
                feature->GetGeometryRef();

            if (geometry)
            {
                if (geometry->Intersects(
                        &source))
                {
                    bestCoordinate =
                        origin;

                    bestDistance = 0.0;

                    found = true;

                    OGRFeature::DestroyFeature(
                        feature);

                    break;
                }

                OGRPoint closest;

                if (closestPointOnGeometry(
                        geometry,
                        source,
                        closest))
                {
                    QGeoCoordinate candidate(
                        closest.getY(),
                        closest.getX());

                    const double distance =
                        origin.distanceTo(
                            candidate);

                    if (distance <
                        bestDistance)
                    {
                        bestDistance =
                            distance;

                        bestCoordinate =
                            candidate;

                        found = true;
                    }
                }
            }

            OGRFeature::DestroyFeature(
                feature);
        }

        m_waterLayer->SetSpatialFilter(
            nullptr);

        m_waterLayer->ResetReading();

        if (found &&
            bestCoordinate.isValid())
        {
            qDebug()
            << "Navigation:"
            << "nearest water:"
            << bestCoordinate.latitude()
            << bestCoordinate.longitude()
            << "distance:"
            << bestDistance
            << "meters";

            return bestCoordinate;
        }

        radiusKm *= 2.0;
    }

    qWarning()
        << "Navigation:"
        << "no water found within"
        << maxSearchKm
        << "km.";

    return {};
}


// ============================================================
// RADIUS TO DEGREES
// ============================================================

void Navigation::radiusToDegrees(
    double latitude,
    double radiusKm,
    double &latitudeDegrees,
    double &longitudeDegrees)
{
    constexpr double earthRadiusKm =
        6371.0;

    constexpr double pi =
        3.14159265358979323846;

    latitudeDegrees =
        (radiusKm /
         earthRadiusKm) *
        (180.0 / pi);

    const double latitudeRadians =
        latitude *
        pi / 180.0;

    const double cosLatitude =
        std::cos(latitudeRadians);

    if (std::abs(cosLatitude) < 1e-10)
    {
        longitudeDegrees =
            180.0;

        return;
    }

    longitudeDegrees =
        (radiusKm /
         (earthRadiusKm *
          cosLatitude)) *
        (180.0 / pi);
}


// ============================================================
// CLOSEST POINT ON SEGMENT
// ============================================================

void Navigation::closestPointOnSegment(
    double px,
    double py,
    double x1,
    double y1,
    double x2,
    double y2,
    double &resultX,
    double &resultY)
{
    const double dx =
        x2 - x1;

    const double dy =
        y2 - y1;

    const double lengthSquared =
        dx * dx +
        dy * dy;

    if (lengthSquared <= 0.0)
    {
        resultX = x1;
        resultY = y1;

        return;
    }

    double t =
        ((px - x1) * dx +
         (py - y1) * dy)
        /
        lengthSquared;

    t = std::clamp(
        t,
        0.0,
        1.0);

    resultX =
        x1 + t * dx;

    resultY =
        y1 + t * dy;
}


// ============================================================
// CLOSEST POINT ON LINE STRING
// ============================================================

bool Navigation::closestPointOnLineString(
    const OGRGeometry *geometry,
    const OGRPoint &source,
    OGRPoint &closestPoint)
{
    if (!geometry)
        return false;

    const OGRLineString *line =
        geometry->toLineString();

    if (!line)
        return false;

    const int pointCount =
        line->getNumPoints();

    if (pointCount <= 0)
        return false;

    const double px =
        source.getX();

    const double py =
        source.getY();

    double bestDistance =
        std::numeric_limits<double>::max();

    double bestX = 0.0;
    double bestY = 0.0;

    if (pointCount == 1)
    {
        bestX =
            line->getX(0);

        bestY =
            line->getY(0);

        closestPoint.setX(bestX);
        closestPoint.setY(bestY);

        return true;
    }

    for (int i = 0;
         i < pointCount - 1;
         ++i)
    {
        const double x1 =
            line->getX(i);

        const double y1 =
            line->getY(i);

        const double x2 =
            line->getX(i + 1);

        const double y2 =
            line->getY(i + 1);

        double candidateX = 0.0;
        double candidateY = 0.0;

        closestPointOnSegment(
            px,
            py,
            x1,
            y1,
            x2,
            y2,
            candidateX,
            candidateY);

        const double distance =
            std::hypot(
                px - candidateX,
                py - candidateY);

        if (distance <
            bestDistance)
        {
            bestDistance =
                distance;

            bestX =
                candidateX;

            bestY =
                candidateY;
        }
    }

    closestPoint.setX(bestX);
    closestPoint.setY(bestY);

    return true;
}


// ============================================================
// CLOSEST POINT ON GEOMETRY
// ============================================================

bool Navigation::closestPointOnGeometry(
    const OGRGeometry *geometry,
    const OGRPoint &source,
    OGRPoint &closestPoint)
{
    if (!geometry ||
        geometry->IsEmpty())
    {
        return false;
    }

    const OGRwkbGeometryType type =
        wkbFlatten(
            geometry->getGeometryType());

    if (type == wkbPoint)
    {
        const OGRPoint *point =
            geometry->toPoint();

        if (!point)
            return false;

        closestPoint =
            *point;

        return true;
    }

    if (type == wkbLineString ||
        type == wkbLinearRing)
    {
        return closestPointOnLineString(
            geometry,
            source,
            closestPoint);
    }

    if (type == wkbPolygon)
    {
        const OGRPolygon *polygon =
            geometry->toPolygon();

        if (!polygon)
            return false;

        OGRPoint bestPoint;

        double bestDistance =
            std::numeric_limits<double>::max();

        const OGRLinearRing *outer =
            polygon->getExteriorRing();

        if (outer)
        {
            OGRPoint candidate;

            if (closestPointOnLineString(
                    outer,
                    source,
                    candidate))
            {
                const double distance =
                    source.Distance(
                        &candidate);

                if (distance <
                    bestDistance)
                {
                    bestDistance =
                        distance;

                    bestPoint =
                        candidate;
                }
            }
        }

        const int ringCount =
            polygon->getNumInteriorRings();

        for (int i = 0;
             i < ringCount;
             ++i)
        {
            const OGRLinearRing *ring =
                polygon->getInteriorRing(i);

            if (!ring)
                continue;

            OGRPoint candidate;

            if (closestPointOnLineString(
                    ring,
                    source,
                    candidate))
            {
                const double distance =
                    source.Distance(
                        &candidate);

                if (distance <
                    bestDistance)
                {
                    bestDistance =
                        distance;

                    bestPoint =
                        candidate;
                }
            }
        }

        if (bestDistance ==
            std::numeric_limits<double>::max())
        {
            return false;
        }

        closestPoint =
            bestPoint;

        return true;
    }

    if (type == wkbMultiPolygon ||
        type == wkbGeometryCollection ||
        type == wkbMultiSurface ||
        type == wkbMultiCurve)
    {
        const OGRGeometryCollection *collection =
            geometry->toGeometryCollection();

        if (!collection)
            return false;

        OGRPoint bestPoint;

        double bestDistance =
            std::numeric_limits<double>::max();

        const int count =
            collection->getNumGeometries();

        for (int i = 0;
             i < count;
             ++i)
        {
            const OGRGeometry *child =
                collection->getGeometryRef(i);

            if (!child)
                continue;

            OGRPoint candidate;

            if (closestPointOnGeometry(
                    child,
                    source,
                    candidate))
            {
                const double distance =
                    source.Distance(
                        &candidate);

                if (distance <
                    bestDistance)
                {
                    bestDistance =
                        distance;

                    bestPoint =
                        candidate;
                }
            }
        }

        if (bestDistance ==
            std::numeric_limits<double>::max())
        {
            return false;
        }

        closestPoint =
            bestPoint;

        return true;
    }

    return false;
}


// ============================================================
// H3 DEBUG VISUALIZATION
// ============================================================

QVariantList Navigation::h3DebugCells() const
{
    QVariantList result;

    double minLat = 90.0;
    double maxLat = -90.0;
    double minLon = 180.0;
    double maxLon = -180.0;

    for (const SeaCell &cell : m_cells)
    {
        if (cell.h3 == H3_NULL)
            continue;

        if (cell.state == CellState::Land)
            continue;

        if (cell.resolution < 15 &&
            cell.state == CellState::Mixed)
        {
            continue;
        }

        CellBoundary boundary{};

        if (cellToBoundary(
                cell.h3,
                &boundary) != E_SUCCESS)
        {
            continue;
        }

        QVariantList path;
        path.reserve(boundary.numVerts);

        for (int vertex = 0;
             vertex < boundary.numVerts;
             ++vertex)
        {
            const double latitude =
                radsToDegs(boundary.verts[vertex].lat);

            const double longitude =
                radsToDegs(boundary.verts[vertex].lng);

            minLat = std::min(minLat, latitude);
            maxLat = std::max(maxLat, latitude);
            minLon = std::min(minLon, longitude);
            maxLon = std::max(maxLon, longitude);

            path.append(
                QVariant::fromValue(
                    QGeoCoordinate(
                        latitude,
                        longitude)));
        }

        QVariantMap polygon;
        polygon["path"] = path;
        polygon["resolution"] = cell.resolution;
        polygon["latitude"] = cell.center.latitude();
        polygon["longitude"] = cell.center.longitude();

        result.append(polygon);
    }

    qDebug()
        << "Navigation: H3 debug polygons:"
        << result.size();

    qDebug()
        << "H3 debug bounding box:"
        << "lat" << minLat << "to" << maxLat
        << "lon" << minLon << "to" << maxLon;

    return result;
}

// ============================================================
// H3 CACHE PATH
// ============================================================

namespace
{
constexpr quint32 H3_GRID_MAGIC = 0x48334731; // "H3G1"
constexpr quint32 H3_GRID_CACHE_VERSION = 3;

static LatLng toH3LatLng(double latitude, double longitude)
{
    LatLng p{};
    p.lat = degsToRads(latitude);
    p.lng = degsToRads(longitude);
    return p;
}
}

QString Navigation::defaultGridCachePath(
    const QString &shpPath) const
{
    const QString base =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

    QDir dir(base);

    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            qWarning()
            << "Navigation: could not create cache directory:"
            << base;
        }
    }

    const QString absolutePath =
        QFileInfo(shpPath).absoluteFilePath();

    const QByteArray hash =
        QCryptographicHash::hash(
            absolutePath.toUtf8(),
            QCryptographicHash::Sha256)
            .toHex()
            .left(16);

    const QString cachePath =
        dir.filePath(
            QString("navigation_h3_%1.dat")
                .arg(QString::fromLatin1(hash)));

    qDebug()
        << "Navigation: H3 cache path:"
        << cachePath;

    return cachePath;
}

QString Navigation::gridSourceSignature(const QString &sourcePath) const
{
    const QFileInfo shpInfo(sourcePath);

    const QString base =
        shpInfo.absolutePath() +
        "/" +
        shpInfo.completeBaseName();

    const QStringList files = {
        base + ".shp",
        base + ".shx",
        base + ".dbf",
        base + ".prj",
        base + ".cpg"
    };

    QString signature;

    for (const QString &filePath : files)
    {
        const QFileInfo info(filePath);
        signature += info.absoluteFilePath() + "|";
        if (!info.exists()) {
            signature += "MISSING;";
            continue;
        }
        signature += QString::number(info.size()) + "|";
    }

    // ONLY the base rules. No m_maxResolution, no refinement zones!
    signature += QString("_gridRules_%1_%2_%3_%4_baseRes_%5_bboxMode_%6")
                     .arg(H3_TEST_MIN_LAT)
                     .arg(H3_TEST_MAX_LAT)
                     .arg(H3_TEST_MIN_LON)
                     .arg(H3_TEST_MAX_LON)
                     .arg(H3_BASE_RESOLUTION)
                     .arg(H3_TEST_BBOX_ONLY);

    return QString(QCryptographicHash::hash(signature.toUtf8(), QCryptographicHash::Md5).toHex());
}

void Navigation::initializeGlobalGrid(const QString &shpPath)
{
    m_gridSourcePath = QFileInfo(shpPath).absoluteFilePath();
    m_gridCachePath = defaultGridCachePath(m_gridSourcePath);

    // 1. Try to load the existing map
    bool loaded = loadGridCache(m_gridCachePath, m_gridSourcePath);
    int cellsBefore = m_cells.size();

    if (loaded) {
        qDebug() << "Navigation: loaded saved H3 graph.";
    } else {
        qDebug() << "Navigation: no valid H3 cache found. building adaptive H3 graph...";
    }

    // 2. ALWAYS run the builder.
    // If the map is completely done, it will realize it in 1 millisecond and do nothing.
    // If you added a new Refinement Zone in your code, it will generate ONLY that zone!
    buildGlobalGrid();

    // 3. Rebuild the connections between cells (needed for pathfinding)
    rebuildAdjacency();

    // 4. If we generated brand new cells (because of a new refinement zone), save the file!
    if (!loaded || m_cells.size() > cellsBefore)
    {
        qDebug() << "Navigation: Grid updated with new refinement zones. Saving to cache...";
        saveGridCache(m_gridCachePath, m_gridSourcePath);
    }
}

QVector<H3Index> Navigation::initialH3CellsForWorld(int resolution) const
{
    const int count = res0CellCount();
    if (count <= 0)
        return {};

    QVector<H3Index> res0(count);

    if (getRes0Cells(res0.data()) != E_SUCCESS)
        return {};

    QVector<H3Index> result;

    int64_t estimated = 0;
    for (H3Index base : res0)
    {
        int64_t childCount = 0;
        if (cellToChildrenSize(base, resolution, &childCount) == E_SUCCESS)
            estimated += childCount;
    }

    if (estimated > 0 && estimated < 1000000)
        result.reserve(static_cast<int>(estimated));

    for (H3Index base : res0)
    {
        int64_t childCount = 0;
        if (cellToChildrenSize(base, resolution, &childCount) != E_SUCCESS ||
            childCount <= 0)
            continue;

        QVector<H3Index> children(static_cast<int>(childCount));
        if (cellToChildren(base, resolution, children.data()) != E_SUCCESS)
            continue;

        result += children;
    }

    return result;
}

QVector<H3Index> Navigation::initialH3CellsForBBox(
    double minLatitude,
    double minLongitude,
    double maxLatitude,
    double maxLongitude,
    int resolution) const
{
    if (minLatitude >= maxLatitude || minLongitude >= maxLongitude)
        return {};

    LatLng verts[4] = {
        toH3LatLng(minLatitude, minLongitude),
        toH3LatLng(minLatitude, maxLongitude),
        toH3LatLng(maxLatitude, maxLongitude),
        toH3LatLng(maxLatitude, minLongitude)
    };

    GeoLoop loop{};
    loop.numVerts = 4;
    loop.verts = verts;

    GeoPolygon polygon{};
    polygon.geoloop = loop;
    polygon.numHoles = 0;
    polygon.holes = nullptr;

    int64_t maxCount = 0;
    if (maxPolygonToCellsSize(&polygon, resolution, 0, &maxCount) != E_SUCCESS ||
        maxCount <= 0)
        return {};

    QVector<H3Index> cells(static_cast<int>(maxCount));

    if (polygonToCells(&polygon, resolution, 0, cells.data()) != E_SUCCESS)
        return {};

    QVector<H3Index> result;
    result.reserve(cells.size());

    for (H3Index cell : cells)
    {
        if (cell != H3_NULL)
            result.append(cell);
    }

    qDebug()
        << "Navigation: H3 bbox seed cells:"
        << result.size()
        << "max buffer:" << cells.size();

    return result;
}

QGeoCoordinate Navigation::h3CellCenter(H3Index cell) const
{
    LatLng center{};
    if (cellToLatLng(cell, &center) != E_SUCCESS)
        return {};

    return QGeoCoordinate(
        radsToDegs(center.lat),
        radsToDegs(center.lng));
}

bool Navigation::h3CellContains(
    H3Index cell,
    const QGeoCoordinate &coordinate) const
{
    if (!coordinate.isValid())
        return false;

    LatLng p = toH3LatLng(
        coordinate.latitude(),
        coordinate.longitude());

    H3Index found = H3_NULL;
    if (latLngToCell(
            &p,
            getResolution(cell),
            &found) != E_SUCCESS)
    {
        return false;
    }

    return found == cell;
}

QVector<H3Index> Navigation::h3Children(
    H3Index parent,
    int childResolution) const
{
    int64_t childCount = 0;
    if (cellToChildrenSize(
            parent,
            childResolution,
            &childCount) != E_SUCCESS ||
        childCount <= 0)
    {
        return {};
    }

    QVector<H3Index> children(
        static_cast<int>(childCount));

    if (cellToChildren(
            parent,
            childResolution,
            children.data()) != E_SUCCESS)
    {
        return {};
    }

    return children;
}

bool Navigation::isH3CellWater(int cellIndex) const
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return false;

    const H3Index cell = m_cells[cellIndex].h3;

    LatLng center{};
    if (cellToLatLng(cell, &center) != E_SUCCESS)
        return false;

    if (!isWater(
            radsToDegs(center.lat),
            radsToDegs(center.lng)))
    {
    }

    CellBoundary boundary{};
    if (cellToBoundary(cell, &boundary) != E_SUCCESS)
        return false;

    int waterPoints = 0;
    const int total = boundary.numVerts + 1;

    if (isWater(
            radsToDegs(center.lat),
            radsToDegs(center.lng)))
    {
        ++waterPoints;
    }

    for (int i = 0; i < boundary.numVerts; ++i)
    {
        if (isWater(
                radsToDegs(boundary.verts[i].lat),
                radsToDegs(boundary.verts[i].lng)))
        {
            ++waterPoints;
        }
    }

    return waterPoints == total;
}

bool Navigation::classifyH3Cell(
    int cellIndex)
{
    if (cellIndex < 0 ||
        cellIndex >= m_cells.size())
    {
        return false;
    }

    SeaCell &cell =
        m_cells[cellIndex];

    if (cell.state != CellState::Unknown)
        return true;

    if (!m_waterLayer)
    {
        cell.state =
            CellState::Land;

        return false;
    }

    cell.state =
        classifyH3IndexOnLayer(
            m_waterLayer,
            cell.h3);

    return true;
}

bool Navigation::refineH3Cell(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return false;

    const SeaCell parent = m_cells[cellIndex];

    if (parent.resolution >= 15)
        return false;

    const int childResolution = parent.resolution + 1;

    int64_t childCount = 0;
    if (cellToChildrenSize(parent.h3, childResolution, &childCount) != E_SUCCESS || childCount <= 0)
        return false;

    // Zero-alloc stack array for common 7-child hex cases
    H3Index children[7] = {0};
    std::vector<H3Index> fallback;
    H3Index* out = children;

    if (childCount > 7) {
        fallback.resize(childCount);
        out = fallback.data();
    }

    if (cellToChildren(parent.h3, childResolution, out) != E_SUCCESS)
        return false;

    m_cells[cellIndex].state = CellState::Mixed;

    for (int64_t i = 0; i < childCount; ++i)
    {
        H3Index child = out[i];
        if (child == H3_NULL || m_h3Index.contains(child))
            continue;

        SeaCell seaCell;
        seaCell.h3 = child;
        seaCell.resolution = childResolution;
        seaCell.state = CellState::Unknown;
        seaCell.center = h3CellCenter(child);

        const int newIndex = m_cells.size();
        m_cells.append(seaCell);
        m_h3Index.insert(child, newIndex);
    }

    return true;
}

void Navigation::buildGlobalGrid()
{
    // 1. Calculate the absolute highest resolution required globally or locally
    int absoluteMaxRes = m_maxResolution;
    for (const auto &zone : m_refinementZones) {
        absoluteMaxRes = std::max(absoluteMaxRes, zone.maxResolution);
    }

    // 2. Only seed the initial world/bbox cells if the grid is entirely empty
    if (m_cells.isEmpty())
    {
        m_h3Index.clear();
        m_neighbors.clear();

        // ---------------------------------------------------------
        // MODIFIED: Use BBox for Mediterranean/Black Sea if enabled
        // ---------------------------------------------------------
        QVector<H3Index> roots;
        if (H3_TEST_BBOX_ONLY) {
            roots = initialH3CellsForBBox(
                H3_TEST_MIN_LAT, H3_TEST_MIN_LON,
                H3_TEST_MAX_LAT, H3_TEST_MAX_LON,
                H3_BASE_RESOLUTION
                );
        } else {
            roots = initialH3CellsForWorld(H3_BASE_RESOLUTION);
        }
        // ---------------------------------------------------------

        m_cells.reserve(roots.size());

        for (H3Index cell : roots)
        {
            SeaCell seaCell;
            seaCell.h3 = cell;
            seaCell.resolution = H3_BASE_RESOLUTION;
            seaCell.center = h3CellCenter(cell);
            seaCell.state = CellState::Unknown;

            const int index = m_cells.size();
            m_cells.append(seaCell);
            m_h3Index.insert(cell, index);
        }
    }

    // 3. Auto-detect where to resume from
    int startResolution = absoluteMaxRes;
    for (const SeaCell &cell : m_cells) {
        if (cell.state == CellState::Unknown || cell.state == CellState::Mixed) {
            startResolution = std::min(startResolution, cell.resolution);
        }
    }

    if (startResolution > absoluteMaxRes) {
        qDebug() << "Navigation: Grid is already fully refined to targets.";
        return;
    }

    // 4. Main loop (Checkpoint-safe)
    for (int resolution = startResolution; resolution <= absoluteMaxRes; ++resolution)
    {
        // ============================================================
        // A. GATHER AND CLASSIFY UNKNOWN CELLS
        // ============================================================
        QVector<int> cellsToClassify;
        for (int i = 0; i < m_cells.size(); ++i) {
            if (m_cells[i].resolution == resolution && m_cells[i].state == CellState::Unknown) {
                cellsToClassify.append(i);
            }
        }

        if (!cellsToClassify.isEmpty())
        {
            const int cellCount = cellsToClassify.size();
            QVector<GridClassification> classifications(cellCount);

            const int workerCount = std::max(1, std::min(m_gridPool.maxThreadCount(), cellCount));
            constexpr int JOBS_PER_WORKER = 4;
            const int totalJobs = std::max(workerCount, workerCount * JOBS_PER_WORKER);
            const int chunkSize = std::max(1, (cellCount + totalJobs - 1) / totalJobs);

            for (int begin = 0; begin < cellCount; begin += chunkSize)
            {
                const int end = std::min(begin + chunkSize, cellCount);
                const QString sourcePath = m_gridSourcePath;

                m_gridPool.start(new NavigationTask([this, sourcePath, begin, end, &cellsToClassify, &classifications]()
                                                    {
                                                        GDALDataset *dataset = static_cast<GDALDataset *>(GDALOpenEx(
                                                            sourcePath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr));

                                                        OGRLayer *layer = dataset ? dataset->GetLayer(0) : nullptr;

                                                        for (int i = begin; i < end; ++i)
                                                        {
                                                            const int cellIndex = cellsToClassify[i];
                                                            classifications[i].h3 = m_cells[cellIndex].h3;
                                                            classifications[i].state = layer ? classifyH3IndexOnLayer(layer, classifications[i].h3) : CellState::Land;
                                                        }

                                                        if (dataset) GDALClose(dataset);
                                                    }));
            }

            m_gridPool.waitForDone();

            for (const GridClassification &c : classifications)
            {
                const auto it = m_h3Index.constFind(c.h3);
                if (it != m_h3Index.constEnd()) {
                    m_cells[it.value()].state = c.state;
                }
            }
        }

        // ============================================================
        // B. GATHER AND REFINE MIXED CELLS
        // ============================================================
        QVector<int> cellsToRefine;
        for (int i = 0; i < m_cells.size(); ++i)
        {
            if (m_cells[i].resolution == resolution && m_cells[i].state == CellState::Mixed)
            {
                int allowedMax = m_maxResolution;

                // Check Localized Deep Refinement Zones
                for (const auto &zone : m_refinementZones) {
                    const double lat = m_cells[i].center.latitude();
                    const double lon = m_cells[i].center.longitude();

                    if (lat >= zone.minLat && lat <= zone.maxLat &&
                        lon >= zone.minLon && lon <= zone.maxLon)
                    {
                        allowedMax = std::max(allowedMax, zone.maxResolution);
                    }
                }

                // If it can still be refined, queue it up.
                // If it hit its max resolution limit, it STAYS Mixed!
                // Pathfinding treats Mixed as land, so this creates a safe boundary, but lets us resume later.
                if (resolution < allowedMax) {
                    cellsToRefine.append(i);
                }
            }
        }

        const int refineCount = cellsToRefine.size();

        if (refineCount > 0)
        {
            QVector<QVector<H3Index>> refineResults(refineCount);

            const int workerCount = std::max(1, std::min(m_gridPool.maxThreadCount(), refineCount));
            constexpr int JOBS_PER_WORKER = 4;
            const int totalJobs = std::max(workerCount, workerCount * JOBS_PER_WORKER);
            const int chunkSize = std::max(1, (refineCount + totalJobs - 1) / totalJobs);

            for (int begin = 0; begin < refineCount; begin += chunkSize)
            {
                const int end = std::min(begin + chunkSize, refineCount);

                m_gridPool.start(new NavigationTask([this, begin, end, &cellsToRefine, &refineResults]()
                                                    {
                                                        for (int i = begin; i < end; ++i)
                                                        {
                                                            const int parentIndex = cellsToRefine[i];
                                                            const SeaCell parent = m_cells[parentIndex];
                                                            refineResults[i] = h3Children(parent.h3, parent.resolution + 1);
                                                        }
                                                    }));
            }

            m_gridPool.waitForDone();

            for (int i = 0; i < refineCount; ++i)
            {
                const QVector<H3Index> &children = refineResults[i];
                if (children.isEmpty()) continue;

                const int childRes = m_cells[cellsToRefine[i]].resolution + 1;

                for (H3Index childH3 : children)
                {
                    if (childH3 != H3_NULL && !m_h3Index.contains(childH3))
                    {
                        SeaCell childCell;
                        childCell.h3 = childH3;
                        childCell.resolution = childRes;
                        childCell.state = CellState::Unknown;
                        childCell.center = h3CellCenter(childH3);

                        const int newIndex = m_cells.size();
                        m_cells.append(childCell);
                        m_h3Index.insert(childH3, newIndex);
                    }
                }
            }
        }
    }
}

QVector<H3Index> Navigation::h3Neighbors(H3Index cell) const
{
    // H3 grid disk of radius 1 handles 7 items (node + 6 neighbors, or node + 5 for pentagons)
    H3Index out[7] = {0};
    if (gridDisk(cell, 1, out) != E_SUCCESS)
        return {};

    QVector<H3Index> neighbors;
    neighbors.reserve(7);

    for (int i = 0; i < 7; ++i)
    {
        H3Index candidate = out[i];
        if (candidate != H3_NULL && candidate != cell)
            neighbors.append(candidate);
    }

    return neighbors;
}

void Navigation::addNeighbor(int first, int second)
{
    if (first < 0 || second < 0 || first == second)
        return;

    if (!m_neighbors[first].contains(second))
        m_neighbors[first].append(second);

    if (!m_neighbors[second].contains(first))
        m_neighbors[second].append(first);
}

void Navigation::addCrossResolutionNeighbors(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return;

    const SeaCell &cell = m_cells[cellIndex];

    if (cell.state == CellState::Land)
        return;

    if (cell.resolution <= H3_BASE_RESOLUTION)
        return;

    const int parentResolution = cell.resolution - 1;

    H3Index parent = H3_NULL;
    if (cellToParent(cell.h3, parentResolution, &parent) != E_SUCCESS)
        return;

    // Use zero-alloc stack approach rather than h3Neighbors to avoid dynamic heap overhead.
    H3Index out[7] = {0};
    if (gridDisk(parent, 1, out) == E_SUCCESS)
    {
        for (int k = 0; k < 7; ++k)
        {
            H3Index candidate = out[k];
            if (candidate != H3_NULL && candidate != parent)
            {
                const auto it = m_h3Index.constFind(candidate);
                if (it != m_h3Index.constEnd())
                {
                    const int otherIndex = it.value();
                    const SeaCell &other = m_cells[otherIndex];

                    if (other.state != CellState::Land && other.resolution == parentResolution)
                        addNeighbor(cellIndex, otherIndex);
                }
            }
        }
    }

    const auto exactParent = m_h3Index.constFind(parent);
    if (exactParent != m_h3Index.constEnd())
    {
        const int parentIndex = exactParent.value();
        if (m_cells[parentIndex].state != CellState::Land)
            addNeighbor(cellIndex, parentIndex);
    }
}

void Navigation::rebuildAdjacency()
{
    m_neighbors.clear();

    int leafCount = 0;

    for (int i = 0; i < m_cells.size(); ++i)
    {
        const SeaCell &cell = m_cells[i];

        bool hasChild = false;

        if (cell.resolution < 15)
        {
            int64_t childCount = 0;
            if (cellToChildrenSize(cell.h3, cell.resolution + 1, &childCount) == E_SUCCESS && childCount > 0)
            {
                // Memory optimized loop, skips QVector creation
                H3Index children[7] = {0};
                std::vector<H3Index> fallback;
                H3Index* out = children;

                if (childCount > 7) {
                    fallback.resize(childCount);
                    out = fallback.data();
                }

                if (cellToChildren(cell.h3, cell.resolution + 1, out) == E_SUCCESS) {
                    for (int64_t c = 0; c < childCount; ++c) {
                        if (out[c] != H3_NULL && m_h3Index.contains(out[c])) {
                            hasChild = true;
                            break;
                        }
                    }
                }
            }
        }

        if (hasChild || cell.state == CellState::Land)
            continue;

        ++leafCount;

        // Perform zero allocation stack arrays for graph building
        H3Index diskOut[7] = {0};
        if (gridDisk(cell.h3, 1, diskOut) == E_SUCCESS)
        {
            for (int k = 0; k < 7; ++k)
            {
                H3Index neighbor = diskOut[k];
                if (neighbor != H3_NULL && neighbor != cell.h3)
                {
                    const auto it = m_h3Index.constFind(neighbor);
                    if (it != m_h3Index.constEnd())
                    {
                        const int neighborIndex = it.value();
                        const SeaCell &other = m_cells[neighborIndex];

                        if (other.state != CellState::Land && other.resolution == cell.resolution)
                            addNeighbor(i, neighborIndex);
                    }
                }
            }
        }

        addCrossResolutionNeighbors(i);
    }

    qDebug()
        << "Navigation: H3 leaf cells:" << leafCount
        << "neighbor nodes:" << m_neighbors.size();
}

int Navigation::findLeafContaining(
    const QGeoCoordinate &coordinate) const
{
    if (!coordinate.isValid())
        return -1;

    for (int resolution = 15;
         resolution >= H3_BASE_RESOLUTION;
         --resolution)
    {
        LatLng p = toH3LatLng(
            coordinate.latitude(),
            coordinate.longitude());

        H3Index cell = H3_NULL;
        if (latLngToCell(&p, resolution, &cell) != E_SUCCESS)
            continue;

        const auto it = m_h3Index.constFind(cell);
        if (it == m_h3Index.constEnd())
            continue;

        const int index = it.value();

        bool hasChild = false;
        if (resolution < 15)
        {
            int64_t childCount = 0;
            if (cellToChildrenSize(cell, resolution + 1, &childCount) == E_SUCCESS && childCount > 0)
            {
                H3Index children[7] = {0};
                std::vector<H3Index> fallback;
                H3Index* out = children;

                if (childCount > 7) {
                    fallback.resize(childCount);
                    out = fallback.data();
                }

                if (cellToChildren(cell, resolution + 1, out) == E_SUCCESS) {
                    for (int64_t c = 0; c < childCount; ++c) {
                        if (out[c] != H3_NULL && m_h3Index.contains(out[c])) {
                            hasChild = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!hasChild && m_cells[index].state != CellState::Land)
            return index;
    }

    return -1;
}

bool Navigation::ensureEndpointCell(
    const QGeoCoordinate &coordinate) const
{
    const int index = findLeafContaining(coordinate);
    if (index < 0)
        return false;

    const SeaCell &cell = m_cells[index];

    if (cell.state == CellState::PureWater)
        return true;

    return isWater(
        coordinate.latitude(),
        coordinate.longitude());
}

bool Navigation::prepareEndpoints(
    const QGeoCoordinate &start,
    const QGeoCoordinate &goal)
{
    return ensureEndpointCell(start) &&
           ensureEndpointCell(goal);
}

bool Navigation::cellIsTraversable(
    int cellIndex) const
{
    if (cellIndex < 0 ||
        cellIndex >= m_cells.size())
    {
        return false;
    }

    const SeaCell &cell =
        m_cells[cellIndex];

    if (cell.state != CellState::PureWater)
        return false;

    return m_neighbors.contains(
        cellIndex);
}

double Navigation::distanceBetweenCells(int first, int second) const
{
    if (first < 0 || second < 0 ||
        first >= m_cells.size() || second >= m_cells.size())
        return std::numeric_limits<double>::max();

    return m_cells[first].center.distanceTo(m_cells[second].center);
}

double Navigation::heuristic(int first, int goal) const
{
    return distanceBetweenCells(first, goal);
}

QVector<int> Navigation::runAStar(int startCell, int goalCell, const QSet<int> &blockedCells)
{
    QVector<int> emptyPath;
    if (!cellIsTraversable(startCell) || !cellIsTraversable(goalCell)) return emptyPath;

    const int numCells = m_cells.size();
    std::vector<double> gCost(numCells, std::numeric_limits<double>::max());
    std::vector<int> parent(numCells, -1);
    std::vector<bool> closed(numCells, false);
    std::priority_queue<OpenNode, std::vector<OpenNode>, std::greater<OpenNode>> openSet;

    gCost[startCell] = 0.0;
    openSet.push({startCell, heuristic(startCell, goalCell)});

    while (!openSet.empty()) {
        const OpenNode current = openSet.top();
        openSet.pop();

        const int currentIndex = current.index;
        if (closed[currentIndex]) continue;
        closed[currentIndex] = true;

        if (currentIndex == goalCell) {
            QVector<int> path;
            int node = goalCell;
            while (node >= 0) {
                path.append(node);
                if (node == startCell) break;
                node = parent[node];
            }
            if (path.isEmpty() || path.last() != startCell) return emptyPath;
            std::reverse(path.begin(), path.end());
            return path;
        }

        const QVector<int> neighbors = m_neighbors.value(currentIndex);
        for (int neighbor : neighbors) {
            if (closed[neighbor] || !cellIsTraversable(neighbor)) continue;

            // COLLISION AVOIDANCE: Skip if another ship is currently in this cell!
            if (blockedCells.contains(neighbor) && neighbor != goalCell) continue;

            const double moveCost = distanceBetweenCells(currentIndex, neighbor);
            const double tentativeG = gCost[currentIndex] + moveCost;

            if (tentativeG < gCost[neighbor]) {
                parent[neighbor] = currentIndex;
                gCost[neighbor] = tentativeG;
                openSet.push({neighbor, tentativeG + heuristic(neighbor, goalCell)});
            }
        }
    }
    return emptyPath;
}

bool Navigation::refineCellsUsedByPath(
    const QVector<int> &cellPath)
{
    Q_UNUSED(cellPath);
    return false;
}

// ============================================================
// CONVERT CELL PATH TO COORDINATES
// ============================================================

QVector<QGeoCoordinate> Navigation::convertCellPathToCoordinates(
    const QVector<int> &cellPath,
    const QGeoCoordinate &start,
    const QGeoCoordinate &goal) const
{
    QVector<QGeoCoordinate> result;

    if (cellPath.isEmpty())
        return result;

    // Always start at the exact current position
    result.append(start);

    // If start and goal are inside the exact same H3 cell,
    // just move in a straight line to the goal.
    if (cellPath.size() == 1)
    {
        if (start.distanceTo(goal) >= 1.0)
        {
            result.append(goal);
        }
        return result;
    }

    // Loop through the path, but SKIP the first cell (index 0)
    // and SKIP the last cell (index size - 1).
    for (int i = 1; i < cellPath.size() - 1; ++i)
    {
        const int index = cellPath[i];

        if (index < 0 || index >= m_cells.size())
            continue;

        const QGeoCoordinate center = m_cells[index].center;

        if (!center.isValid())
            continue;

        // Prevent duplicate waypoints if cells are stacked/tiny
        if (!result.isEmpty() &&
            result.last().distanceTo(center) < 1.0)
            continue;

        result.append(center);
    }

    // Always end at the exact target position (skipping the final cell center)
    if (result.isEmpty() || result.last().distanceTo(goal) >= 1.0)
    {
        result.append(goal);
    }

    return result;
}

QVariantList Navigation::findPath(
    double startLatitude,
    double startLongitude,
    double targetLatitude,
    double targetLongitude,
    double cellSizeMeters)
{
    Q_UNUSED(cellSizeMeters);
    // Public API calls internal API with no blocked cells
    return findPathInternal(startLatitude, startLongitude, targetLatitude, targetLongitude, QSet<int>());
}

QVariantList Navigation::findPathInternal(
    double startLatitude,
    double startLongitude,
    double targetLatitude,
    double targetLongitude,
    const QSet<int> &blockedCells)
{
    std::lock_guard<std::recursive_mutex> lock(m_navigationMutex);

    QVariantList result;

    QGeoCoordinate start(startLatitude, startLongitude);
    if (!start.isValid()) return result;

    int startCell = findLeafContaining(start);
    if (startCell < 0 || !cellIsTraversable(startCell)) {
        // Fallback to nearest traversable cell if drifting slightly off-grid
        int best = -1;
        double minDist = std::numeric_limits<double>::max();
        for (int i = 0; i < m_cells.size(); ++i) {
            if (cellIsTraversable(i)) {
                double d = start.distanceTo(m_cells[i].center);
                if (d < minDist) {
                    minDist = d;
                    best = i;
                }
            }
        }
        startCell = best;
    }

    if (startCell < 0) return result;

    QGeoCoordinate goal(targetLatitude, targetLongitude);
    if (!goal.isValid()) return result;

    int goalCell = findLeafContaining(goal);
    if (goalCell < 0 || !cellIsTraversable(goalCell)) {
        goal = validatePosition(targetLatitude, targetLongitude);
        if (!goal.isValid()) return result;
        goalCell = findLeafContaining(goal);
    }

    if (goalCell < 0) return result;

    QVector<int> cellPath;
    if (startCell == goalCell) {
        cellPath.append(startCell);
    } else {
        // 1. Try finding a route with collision avoidance enabled
        cellPath = runAStar(startCell, goalCell, blockedCells);

        // 2. FAILSAFE: Only allow fallback if we are NOT dodging another ship.
        if (cellPath.isEmpty() && blockedCells.isEmpty()) {
            cellPath = runAStar(startCell, goalCell, QSet<int>());
        }
    }

    if (cellPath.isEmpty()) return result;

    const QVector<QGeoCoordinate> path = convertCellPathToCoordinates(cellPath, start, goal);
    for (const QGeoCoordinate &coordinate : path) {
        if (coordinate.isValid()) {
            QVariantMap point;
            point["latitude"] = coordinate.latitude();
            point["longitude"] = coordinate.longitude();
            result.append(point);
        }
    }

    return result;
}

bool Navigation::saveGridCache(
    const QString &cachePath,
    const QString &sourcePath)
{
    if (cachePath.isEmpty() ||
        sourcePath.isEmpty())
    {
        qWarning()
        << "Navigation: cache save aborted:"
        << "empty path.";

        return false;
    }

    const QFileInfo cacheInfo(cachePath);

    QDir cacheDir =
        cacheInfo.absoluteDir();

    if (!cacheDir.exists())
    {
        if (!cacheDir.mkpath("."))
        {
            qWarning()
            << "Navigation: could not create cache directory:"
            << cacheDir.absolutePath();

            return false;
        }
    }

    qDebug()
        << "Navigation: saving H3 cache:"
        << cachePath;

    QSaveFile file(cachePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning()
        << "Navigation: could not open cache for writing:"
        << cachePath
        << "error:"
        << file.errorString();

        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    const QString absoluteSourcePath =
        QFileInfo(sourcePath).absoluteFilePath();

    const QString sourceSignature =
        gridSourceSignature(
            absoluteSourcePath);

    stream
        << H3_GRID_MAGIC
        << H3_GRID_CACHE_VERSION
        << absoluteSourcePath
        << sourceSignature
        << quint32(m_cells.size());

    for (const SeaCell &cell : m_cells)
    {
        stream
            << quint64(cell.h3)
            << qint32(cell.resolution)
            << quint8(cell.state);
    }

    if (stream.status() != QDataStream::Ok)
    {
        qWarning()
        << "Navigation: QDataStream error while saving H3 cache.";

        return false;
    }

    if (!file.commit())
    {
        qWarning()
        << "Navigation: QSaveFile commit failed:"
        << file.errorString();

        return false;
    }

    const QFileInfo savedFile(cachePath);

    if (!savedFile.exists())
    {
        qWarning()
        << "Navigation: cache commit reported success,"
        << "but cache file does not exist:"
        << cachePath;

        return false;
    }

    qDebug()
        << "Navigation: H3 cache saved successfully."
        << "cells:"
        << m_cells.size()
        << "bytes:"
        << savedFile.size();

    return true;
}

bool Navigation::loadGridCache(
    const QString &cachePath,
    const QString &sourcePath)
{
    qDebug()
    << "Navigation: checking H3 cache:"
    << cachePath;

    QFile file(cachePath);

    if (!file.exists())
    {
        qDebug()
        << "Navigation: cache file does not exist.";

        return false;
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning()
        << "Navigation: could not open H3 cache:"
        << file.errorString();

        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint32 version = 0;

    QString savedSourcePath;
    QString savedSourceSignature;

    quint32 cellCount = 0;

    stream
        >> magic
        >> version
        >> savedSourcePath
        >> savedSourceSignature
        >> cellCount;

    if (stream.status() != QDataStream::Ok)
    {
        qWarning()
        << "Navigation: H3 cache header could not be read.";

        return false;
    }

    if (magic != H3_GRID_MAGIC)
    {
        qWarning()
        << "Navigation: invalid H3 cache magic.";

        return false;
    }

    if (version != H3_GRID_CACHE_VERSION)
    {
        qWarning()
        << "Navigation: H3 cache version mismatch."
        << "saved:"
        << version
        << "current:"
        << H3_GRID_CACHE_VERSION;

        return false;
    }

    const QString absoluteSourcePath =
        QFileInfo(sourcePath).absoluteFilePath();

    if (savedSourcePath != absoluteSourcePath)
    {
        qWarning()
        << "Navigation: H3 cache source path mismatch."
        << "\nsaved:"
        << savedSourcePath
        << "\ncurrent:"
        << absoluteSourcePath;

        return false;
    }

    const QString currentSignature =
        gridSourceSignature(
            absoluteSourcePath);

    if (savedSourceSignature != currentSignature)
    {
        qWarning()
        << "Navigation: H3 cache source signature changed.";

        return false;
    }

    if (cellCount == 0 ||
        cellCount > 50000000)
    {
        qWarning()
        << "Navigation: invalid H3 cache cell count:"
        << cellCount;

        return false;
    }

    QVector<SeaCell> loaded;
    loaded.resize(
        static_cast<int>(cellCount));

    QHash<H3Index, int> loadedIndex;

    for (int i = 0;
         i < loaded.size();
         ++i)
    {
        quint64 h3Value = 0;
        qint32 resolution = 0;
        quint8 state = 0;

        stream
            >> h3Value
            >> resolution
            >> state;

        if (stream.status() != QDataStream::Ok)
        {
            qWarning()
            << "Navigation: H3 cache ended unexpectedly"
            << "at cell"
            << i;

            return false;
        }

        const H3Index h3 =
            static_cast<H3Index>(
                h3Value);

        if (h3 == H3_NULL)
        {
            qWarning()
            << "Navigation: cache contains H3_NULL"
            << "at cell"
            << i;

            return false;
        }

        if (resolution < H3_BASE_RESOLUTION ||
            resolution > 15)
        {
            qWarning()
            << "Navigation: invalid H3 resolution in cache:"
            << resolution;

            return false;
        }

        if (loadedIndex.contains(h3))
        {
            qWarning()
            << "Navigation: duplicate H3 cell in cache:"
            << Qt::hex
            << h3
            << Qt::dec;

            return false;
        }

        loaded[i].h3 = h3;
        loaded[i].resolution = resolution;
        loaded[i].state =
            static_cast<CellState>(state);

        loaded[i].center =
            h3CellCenter(h3);

        loadedIndex.insert(
            h3,
            i);
    }

    if (stream.status() != QDataStream::Ok)
    {
        qWarning()
        << "Navigation: H3 cache read failed.";

        return false;
    }

    m_cells =
        std::move(loaded);

    m_h3Index =
        std::move(loadedIndex);

    m_neighbors.clear();

    qDebug()
        << "Navigation: H3 cache loaded successfully."
        << "cells:"
        << m_cells.size();

    return true;
}

// ============================================================
// PREVIEW A* PATH
// ============================================================

void Navigation::previewPath(
    const QString &shipId,
    double startLatitude,
    double startLongitude,
    double targetLatitude,
    double targetLongitude)
{
    if (shipId.isEmpty())
        return;

    m_navigationPool.start(
        new NavigationTask(
            [this,
             shipId,
             startLatitude,
             startLongitude,
             targetLatitude,
             targetLongitude]()
            {
                QVariantList variantPath;

                {
                    std::lock_guard<std::recursive_mutex>
                        lock(m_navigationMutex);

                    qDebug()
                        << "Navigation:"
                        << "calculating preview route for"
                        << shipId;

                    // IMPORTANT:
                    //
                    // This is the exact same A* function
                    // used by startNavigation().
                    //
                    // It calculates a route but does not
                    // create an ActiveRoute.
                    variantPath =
                        findPath(
                            startLatitude,
                            startLongitude,
                            targetLatitude,
                            targetLongitude);
                }

                QMetaObject::invokeMethod(
                    this,

                    [this,
                     shipId,
                     variantPath]()
                    {
                        emit previewPathReady(
                            shipId,
                            variantPath);
                    },

                    Qt::QueuedConnection);
            }
            ));
}

// ============================================================
// START NAVIGATION
// ============================================================

bool Navigation::startNavigation(
    const QString& shipId,
    double startLatitude,
    double startLongitude,
    double targetLatitude,
    double targetLongitude,
    double speedKnots,
    bool isReroute)
{
    if (shipId.isEmpty())
    {
        return false;
    }

    const double speedMetersPerSecond =
        speedKnots * 0.514444;

    if (speedMetersPerSecond <= 0.0)
    {
        emit navigationFailed(
            shipId,
            "Ship speed is zero."
            );

        return false;
    }

    /*
     * --------------------------------------------------------
     * REQUEST GENERATION
     * --------------------------------------------------------
     *
     * New USER request:
     *
     *     generation++
     *
     * Automatic reroute:
     *
     *     same generation
     *
     * This prevents old asynchronous A* results from
     * overwriting newer user navigation.
     */

    quint64 requestGeneration = 0;

    {
        std::lock_guard<std::recursive_mutex>
            lock(m_routesMutex);

        if (isReroute)
        {
            requestGeneration =
                m_routeRequestGeneration.value(
                    shipId,
                    0
                    );
        }
        else
        {
            requestGeneration =
                ++m_routeRequestGeneration[
                    shipId
            ];
        }
    }


    /*
     * --------------------------------------------------------
     * SNAPSHOT OTHER SHIPS
     * --------------------------------------------------------
     */

    QSet<int> blockedCells;

    {
        std::lock_guard<std::recursive_mutex>
            routeLock(m_routesMutex);

        std::lock_guard<std::recursive_mutex>
            navigationLock(m_navigationMutex);

        for (auto it =
             m_activeRoutes.begin();
             it != m_activeRoutes.end();
             ++it)
        {
            if (it.key() == shipId)
            {
                continue;
            }

            const QGeoCoordinate position =
                it.value().currentPosition;

            if (!position.isValid())
            {
                continue;
            }

            const int cell =
                findLeafContaining(
                    position
                    );

            if (cell >= 0)
            {
                blockedCells.insert(
                    cell
                    );
            }

            /*
             * Block a couple of upcoming cells as well.
             */

            const int waypoint =
                it.value().waypointIndex;

            for (int i = 0;
                 i < 2 &&
                 (waypoint + i) <
                     it.value().path.size();
                 ++i)
            {
                const int waypointCell =
                    findLeafContaining(
                        it.value().path[
                            waypoint + i]
                        );

                if (waypointCell >= 0)
                {
                    blockedCells.insert(
                        waypointCell
                        );
                }
            }
        }
    }


    /*
     * --------------------------------------------------------
     * ASYNCHRONOUS PATH CALCULATION
     * --------------------------------------------------------
     */

    m_navigationPool.start(
        new NavigationTask(
            [
                this,
                shipId,
                requestGeneration,
                isReroute,
                startLatitude,
                startLongitude,
                targetLatitude,
                targetLongitude,
                speedMetersPerSecond,
                blockedCells
    ]()
            {
                QVariantList variantPath;

                {
                    std::lock_guard<std::recursive_mutex>
                        lock(m_navigationMutex);

                    variantPath =
                        findPathInternal(
                            startLatitude,
                            startLongitude,
                            targetLatitude,
                            targetLongitude,
                            blockedCells
                            );
                }


                /*
                 * Return to Navigation's thread.
                 */

                QMetaObject::invokeMethod(
                    this,

                    [
                        this,
                        shipId,
                        requestGeneration,
                        isReroute,
                        speedMetersPerSecond,
                        variantPath
                ]()
                    {
                        finishNavigation(
                            shipId,
                            requestGeneration,
                            isReroute,
                            speedMetersPerSecond,
                            variantPath
                            );
                    },

                    Qt::QueuedConnection
                    );
            }
            )
        );

    return true;
}

// ============================================================
// FINISH ASYNCHRONOUS NAVIGATION
// ============================================================

void Navigation::finishNavigation(
    const QString& shipId,
    quint64 requestGeneration,
    bool isReroute,
    double speedMetersPerSecond,
    const QVariantList& variantPath)
{
    std::lock_guard<std::recursive_mutex>
        lock(m_routesMutex);


    /*
     * --------------------------------------------------------
     * DISCARD OLD A* RESULTS
     * --------------------------------------------------------
     */

    if (m_routeRequestGeneration.value(
            shipId,
            0
            ) != requestGeneration)
    {
        qDebug()
        << "Navigation:"
        << "discarding obsolete route result"
        << "ship:"
        << shipId
        << "generation:"
        << requestGeneration;

        return;
    }


    /*
     * --------------------------------------------------------
     * EMPTY ROUTE
     * --------------------------------------------------------
     */

    if (variantPath.isEmpty())
    {
        if (isReroute &&
            m_activeRoutes.contains(shipId))
        {
            ActiveRoute& oldRoute =
                m_activeRoutes[shipId];

            oldRoute.isRerouting =
                false;

            oldRoute.isWaiting =
                true;

            oldRoute.rerouteCooldown =
                20;
        }

        emit navigationFailed(
            shipId,
            "No water path could be found."
            );

        return;
    }


    /*
     * --------------------------------------------------------
     * BUILD NEW PATH
     * --------------------------------------------------------
     */

    QVector<QGeoCoordinate> newPath;

    newPath.reserve(
        variantPath.size()
        );

    for (const QVariant& value :
         variantPath)
    {
        const QVariantMap point =
            value.toMap();

        const QGeoCoordinate coordinate(
            point.value(
                     "latitude")
                .toDouble(),

            point.value(
                     "longitude")
                .toDouble()
            );

        if (coordinate.isValid())
        {
            newPath.append(
                coordinate
                );
        }
    }


    if (newPath.size() < 2)
    {
        if (isReroute &&
            m_activeRoutes.contains(shipId))
        {
            m_activeRoutes[shipId]
                .isRerouting =
                false;
        }

        emit navigationFailed(
            shipId,
            "Path contains too few points."
            );

        return;
    }


    /*
     * --------------------------------------------------------
     * BUILD ROUTE
     * --------------------------------------------------------
     */

    ActiveRoute route;

    route.speedMetersPerSecond =
        speedMetersPerSecond;

    route.path =
        newPath;

    route.waypointIndex =
        1;

    route.isRerouting =
        false;

    route.isWaiting =
        false;

    route.rerouteCooldown =
        0;

    route.lastDbSaveTime =
        QDateTime::currentMSecsSinceEpoch();


    /*
     * --------------------------------------------------------
     * DETERMINE LIVE POSITION
     * --------------------------------------------------------
     */

    const bool oldRouteExists =
        m_activeRoutes.contains(
            shipId
            );

    if (oldRouteExists)
    {
        const ActiveRoute& oldRoute =
            m_activeRoutes[
                shipId
        ];

        /*
         * NEVER use the original UI start coordinate
         * when replacing/rerouting an already moving ship.
         */

        route.currentPosition =
            oldRoute.currentPosition;

        if (!route.currentPosition.isValid())
        {
            route.currentPosition =
                route.path.first();
        }


        /*
         * ----------------------------------------------------
         * AUTOMATIC REROUTE
         * ----------------------------------------------------
         *
         * Preserve routeId.
         */

        if (isReroute)
        {
            route.routeId =
                oldRoute.routeId;

            route.rerouteCooldown =
                oldRoute.rerouteCooldown;
        }
        else
        {
            /*
             * ------------------------------------------------
             * USER STARTED A COMPLETELY NEW ROUTE
             * ------------------------------------------------
             */

            route.routeId =
                QUuid::createUuid()
                    .toString(
                        QUuid::WithoutBraces
                        );
        }
    }
    else
    {
        /*
         * First route for this ship.
         */

        route.currentPosition =
            route.path.first();

        route.routeId =
            QUuid::createUuid()
                .toString(
                    QUuid::WithoutBraces
                    );
    }


    /*
     * Make the first waypoint exactly where the ship
     * actually is now.
     */

    route.path[0] =
        route.currentPosition;


    /*
     * --------------------------------------------------------
     * REPLACE IN-MEMORY ACTIVE ROUTE
     * --------------------------------------------------------
     */

    m_activeRoutes[
        shipId
    ] = route;


    if (!m_navigationTimer.isActive())
    {
        m_navigationTimer.start();
    }


    qDebug()
        << "Navigation:"
        << (isReroute
                ? "automatic reroute"
                : "new route")
        << "ship:"
        << shipId
        << "routeId:"
        << route.routeId;


    /*
     * --------------------------------------------------------
     * BUILD ACTUAL STORED PATH
     * --------------------------------------------------------
     *
     * Do this from route.path rather than variantPath so
     * the database/UI both use the corrected live starting
     * coordinate.
     */

    QVariantList actualPath;

    actualPath.reserve(
        route.path.size()
        );

    for (const QGeoCoordinate& coordinate :
         route.path)
    {
        QVariantMap point;

        point["latitude"] =
            coordinate.latitude();

        point["longitude"] =
            coordinate.longitude();

        actualPath.append(
            point
            );
    }


    /*
     * --------------------------------------------------------
     * DATABASE
     * --------------------------------------------------------
     *
     * USER ROUTE:
     *
     *     INSERT new document.
     *
     * REROUTE:
     *
     *     DO NOT create another database route.
     */

    if (!isReroute)
    {
        emit routeStartedNeedsSave(
            shipId,
            route.routeId,
            route.currentPosition.latitude(),
            route.currentPosition.longitude(),
            actualPath
            );
    }


    /*
     * UI receives corrected path.
     */

    emit pathReady(
        shipId,
        actualPath
        );
}

// ============================================================
// UPDATE NAVIGATION
// ============================================================

    void Navigation::updateNavigation()
{
    std::lock_guard<std::recursive_mutex>
        lock(m_routesMutex);

    if (m_activeRoutes.isEmpty())
    {
        m_navigationTimer.stop();
        return;
    }

    constexpr double deltaSeconds = 0.1;
    constexpr double COLLISION_RADIUS = 300.0;


    // ========================================================
    // 1. COLLISION DETECTION
    // ========================================================

    QList<QString> needsReroute;

    for (auto it =
         m_activeRoutes.begin();
         it != m_activeRoutes.end();
         ++it)
    {
        ActiveRoute& route =
            it.value();

        if (route.rerouteCooldown > 0)
        {
            --route.rerouteCooldown;
        }

        if (route.isRerouting)
        {
            continue;
        }

        /*
         * Waiting route.
         */

        if (route.isWaiting)
        {
            if (route.rerouteCooldown <= 0)
            {
                route.isWaiting = false;

                if (!needsReroute.contains(it.key()))
                {
                    needsReroute.append(it.key());
                }
            }

            continue;
        }

        if (route.rerouteCooldown > 0)
        {
            continue;
        }

        const QGeoCoordinate p1 =
            route.currentPosition;

        if (!p1.isValid())
        {
            continue;
        }

        bool collisionDetected = false;

        for (auto other =
             m_activeRoutes.begin();
             other != m_activeRoutes.end();
             ++other)
        {
            if (it.key() == other.key())
            {
                continue;
            }

            const QGeoCoordinate p2 =
                other.value().currentPosition;

            if (!p2.isValid())
            {
                continue;
            }

            /*
             * Deterministic yielding.
             *
             * Only the ship with the greater ID reroutes.
             */

            if (p1.distanceTo(p2) < COLLISION_RADIUS &&
                it.key() > other.key())
            {
                collisionDetected = true;
                break;
            }
        }

        if (collisionDetected)
        {
            if (!needsReroute.contains(it.key()))
            {
                needsReroute.append(it.key());

                route.rerouteCooldown = 50;
            }
        }
    }


    // ========================================================
    // 2. START AUTOMATIC REROUTES
    // ========================================================

    for (const QString& shipId :
         needsReroute)
    {
        auto routeIt =
            m_activeRoutes.find(shipId);

        if (routeIt ==
            m_activeRoutes.end())
        {
            continue;
        }

        ActiveRoute& route =
            routeIt.value();

        if (route.path.isEmpty())
        {
            continue;
        }

        route.isRerouting = true;

        const QGeoCoordinate current =
            route.currentPosition;

        const QGeoCoordinate target =
            route.path.last();

        int course =
            qRound(
                current.azimuthTo(target)
                );

        if (course < 0)
        {
            course += 360;
        }

        course %= 360;

        const double speedKnots =
            route.speedMetersPerSecond / 0.514444;

        /*
         * Automatic reroute.
         *
         * isReroute = true means:
         *
         * - do NOT create another trip
         * - preserve the existing route ID
         */

        startNavigation(
            shipId,
            current.latitude(),
            current.longitude(),
            target.latitude(),
            target.longitude(),
            speedKnots,
            true
            );
    }


    // ========================================================
    // 3. MOVE ALL ACTIVE SHIPS
    // ========================================================

    for (auto it =
         m_activeRoutes.begin();
         it != m_activeRoutes.end();)
    {
        const QString shipId =
            it.key();

        ActiveRoute& route =
            it.value();


        // ----------------------------------------------------
        // Do not move while rerouting/waiting
        // ----------------------------------------------------

        if (route.isRerouting ||
            route.isWaiting)
        {
            ++it;
            continue;
        }


        // ----------------------------------------------------
        // Validate route
        // ----------------------------------------------------

        if (route.path.isEmpty() ||
            route.waypointIndex < 1)
        {
            const QString routeId =
                route.routeId;

            qWarning()
                << "Navigation:"
                << "invalid route:"
                << shipId
                << routeId;

            emit routeCancelledNeedsDelete(
                shipId,
                routeId
                );

            emit navigationFailed(
                shipId,
                "Invalid active route."
                );

            it =
                m_activeRoutes.erase(it);

            continue;
        }


        // ----------------------------------------------------
        // Safety: if waypoint index is already past route
        // ----------------------------------------------------

        if (route.waypointIndex >=
            route.path.size())
        {
            const QString routeId =
                route.routeId;

            const QGeoCoordinate finalPosition =
                route.currentPosition;

            qDebug()
                << "Navigation:"
                << "ARRIVED"
                << "ship:"
                << shipId
                << "route:"
                << routeId
                << "final:"
                << finalPosition.latitude()
                << finalPosition.longitude();

            /*
     * ----------------------------------------------------------
     * Calculate the final course.
     *
     * At this point the ship has reached the final waypoint,
     * so use the previous waypoint -> final position.
     * ----------------------------------------------------------
     */

            int course = 0;

            if (route.waypointIndex > 0 &&
                route.waypointIndex <= route.path.size())
            {
                const QGeoCoordinate previous =
                    route.path[route.waypointIndex - 1];

                course =
                    qRound(
                        previous.azimuthTo(
                            finalPosition
                            )
                        );

                if (course < 0)
                {
                    course += 360;
                }

                course %= 360;
            }

            qDebug()
                << "Navigation:"
                << "final course:"
                << course;

            /*
     * ----------------------------------------------------------
     * Save final position + final course.
     * ----------------------------------------------------------
     */

            emit routeFinishedNeedsClear(
                shipId,
                routeId,
                finalPosition.latitude(),
                finalPosition.longitude(),
                route.waypointIndex,
                course
                );

            emit navigationFinished(
                shipId
                );

            it =
                m_activeRoutes.erase(it);

            continue;
        }


        // ----------------------------------------------------
        // Current live position
        // ----------------------------------------------------

        const QGeoCoordinate current =
            route.currentPosition;

        const QGeoCoordinate target =
            route.path[
                route.waypointIndex
        ];
        int course =
            qRound(current.azimuthTo(target));

        if (course < 0)
        {
            course += 360;
        }

        course %= 360;

        if (!current.isValid() ||
            !target.isValid())
        {
            const QString routeId =
                route.routeId;

            qWarning()
                << "Navigation:"
                << "invalid position"
                << shipId
                << current
                << target;

            emit navigationFailed(
                shipId,
                "Invalid coordinate in route."
                );

            emit routeCancelledNeedsDelete(
                shipId,
                routeId
                );

            it =
                m_activeRoutes.erase(it);

            continue;
        }


        // ----------------------------------------------------
        // Distance to next waypoint
        // ----------------------------------------------------

        const double distance =
            current.distanceTo(target);

        const double movementDistance =
            route.speedMetersPerSecond *
            deltaSeconds;


        // ====================================================
        // 4. WAYPOINT REACHED
        // ====================================================

        if (distance <= 0.001 ||
            distance <= movementDistance)
        {
            /*
             * Snap exactly onto the waypoint.
             */

            route.currentPosition =
                target;

            ++route.waypointIndex;

            const QGeoCoordinate newPosition =
                route.currentPosition;

            int calculatedCourse = qRound(current.azimuthTo(target));
            if (calculatedCourse == 360) calculatedCourse = 0; // Normalize to 0-359

            /*
             * Always inform QML about the exact new position.
             */

            emit shipPositionChanged(
                shipId,
                newPosition.latitude(),
                newPosition.longitude(),
                calculatedCourse
                );


            // =================================================
            // FINAL DESTINATION
            // =================================================

            if (route.waypointIndex >=
                route.path.size())
            {
                const QString routeId =
                    route.routeId;

                const QGeoCoordinate finalPosition =
                    route.currentPosition;

                qDebug()
                    << "Navigation:"
                    << "ARRIVED"
                    << "ship:"
                    << shipId
                    << "route:"
                    << routeId
                    << "final:"
                    << finalPosition.latitude()
                    << finalPosition.longitude();


                /*
                 * IMPORTANT:
                 *
                 * Persist the FINAL position.
                 *
                 * main.cpp sends this to MarkFinished.
                 *
                 * database::markRouteFinished() must write the
                 * same coordinate into ships.latitude/longitude.
                 */

                emit routeFinishedNeedsClear(
                    shipId,
                    routeId,
                    finalPosition.latitude(),
                    finalPosition.longitude(),
                    route.waypointIndex,
                    course
                    );


                /*
                 * Notify UI.
                 */

                emit navigationFinished(
                    shipId
                    );


                /*
                 * Remove only this route.
                 */

                it =
                    m_activeRoutes.erase(it);

                continue;
            }


            // =================================================
            // INTERMEDIATE WAYPOINT
            // =================================================

            const qint64 now =
                QDateTime::currentMSecsSinceEpoch();

            if (now -
                    route.lastDbSaveTime >=
                1000)
            {
                route.lastDbSaveTime =
                    now;

                emit routeProgressNeedsSave(
                    shipId,
                    route.routeId,
                    newPosition.latitude(),
                    newPosition.longitude(),
                    course
                    );
            }

            ++it;

            continue;
        }


        // ====================================================
        // 5. PARTIAL MOVEMENT
        // ====================================================

        const double fraction =
            movementDistance / distance;

        const double safeFraction =
            std::clamp(
                fraction,
                0.0,
                1.0
                );


        // ----------------------------------------------------
        // Latitude
        // ----------------------------------------------------

        const double newLatitude =
            current.latitude()
            +
            (
                target.latitude()
                -
                current.latitude()
                )
                *
                safeFraction;


        // ----------------------------------------------------
        // Longitude
        // ----------------------------------------------------

        double lonDiff =
            target.longitude()
            -
            current.longitude();

        if (lonDiff > 180.0)
        {
            lonDiff -= 360.0;
        }
        else if (lonDiff < -180.0)
        {
            lonDiff += 360.0;
        }

        double newLongitude =
            current.longitude()
            +
            lonDiff *
                safeFraction;


        if (newLongitude > 180.0)
        {
            newLongitude -= 360.0;
        }
        else if (newLongitude < -180.0)
        {
            newLongitude += 360.0;
        }


        const QGeoCoordinate newPosition(
            newLatitude,
            newLongitude
            );

        int calculatedCourse = qRound(current.azimuthTo(newPosition));
        if (calculatedCourse == 360) calculatedCourse = 0;

        if (!newPosition.isValid())
        {
            const QString routeId =
                route.routeId;

            qWarning()
                << "Navigation:"
                << "calculated invalid position:"
                << shipId
                << newPosition;

            emit navigationFailed(
                shipId,
                "Calculated invalid ship position."
                );

            emit routeCancelledNeedsDelete(
                shipId,
                routeId
                );

            it =
                m_activeRoutes.erase(it);

            continue;
        }


        // ====================================================
        // 6. UPDATE LIVE STATE
        // ====================================================

        route.currentPosition =
            newPosition;


        /*
         * The previous waypoint becomes the actual point from
         * which the ship is currently travelling.
         *
         * Do NOT replace the whole path.
         */

        if (route.waypointIndex > 0 &&
            route.waypointIndex <
                route.path.size())
        {
            route.path[
                route.waypointIndex - 1
            ] =
                newPosition;
        }


        // ====================================================
        // 7. SEND POSITION TO UI
        // ====================================================

        emit shipPositionChanged(
            shipId,
            newPosition.latitude(),
            newPosition.longitude(),
            calculatedCourse
            );


        // ====================================================
        // 8. PERIODIC DATABASE PROGRESS
        // ====================================================

        const qint64 now =
            QDateTime::currentMSecsSinceEpoch();

        if (now -
                route.lastDbSaveTime >=
            1000)
        {
            route.lastDbSaveTime =
                now;

            emit routeProgressNeedsSave(
                shipId,
                route.routeId,
                newPosition.latitude(),
                newPosition.longitude(),
                course
                );
        }


        ++it;
    }
}
// ============================================================
// STOP / FINISH NAVIGATION
// ============================================================

// ============================================================
// STOP / FINISH NAVIGATION
// ============================================================

void Navigation::stopNavigation(
    const QString& shipId)
{
    std::lock_guard<std::recursive_mutex>
        lock(m_routesMutex);

    auto it =
        m_activeRoutes.find(
            shipId
            );

    if (it ==
        m_activeRoutes.end())
    {
        qDebug()
        << "Navigation:"
        << "stop requested but no active route:"
        << shipId;

        return;
    }

    ActiveRoute& route =
        it.value();

    const QString routeId =
        route.routeId;

    /*
     * currentPosition is the actual live position
     * of the ship at the moment Stop is pressed.
     */
    const QGeoCoordinate finalPosition =
        route.currentPosition;

    /*
     * --------------------------------------------------------
     * VALIDATE FINAL POSITION
     * --------------------------------------------------------
     */

    if (!finalPosition.isValid())
    {
        qWarning()
        << "Navigation:"
        << "cannot finish stopped route because"
        << "current position is invalid:"
        << shipId
        << routeId;

        emit routeCancelledNeedsDelete(
            shipId,
            routeId
            );

        m_activeRoutes.erase(
            it
            );

        if (m_activeRoutes.isEmpty())
        {
            m_navigationTimer.stop();
        }

        return;
    }

    /*
     * --------------------------------------------------------
     * CALCULATE CURRENT COURSE
     * --------------------------------------------------------
     *
     * The ship is currently travelling toward the next
     * waypoint. Use the current position -> next waypoint
     * to calculate its heading.
     */

    int course = 0;

    if (route.waypointIndex >= 0 &&
        route.waypointIndex < route.path.size())
    {
        const QGeoCoordinate nextWaypoint =
            route.path[route.waypointIndex];

        if (nextWaypoint.isValid())
        {
            course =
                qRound(
                    finalPosition.azimuthTo(
                        nextWaypoint
                        )
                    );

            if (course < 0)
            {
                course += 360;
            }

            course %= 360;
        }
    }
    else if (route.waypointIndex > 0 &&
             route.waypointIndex <= route.path.size())
    {
        /*
         * Fallback:
         *
         * If there is no next waypoint, use the previous
         * waypoint -> current position.
         */
        const QGeoCoordinate previousWaypoint =
            route.path[
                route.waypointIndex - 1
        ];

        if (previousWaypoint.isValid())
        {
            course =
                qRound(
                    previousWaypoint.azimuthTo(
                        finalPosition
                        )
                    );

            if (course < 0)
            {
                course += 360;
            }

            course %= 360;
        }
    }

    qDebug()
        << "Navigation:"
        << "STOP / FINISH"
        << "ship:"
        << shipId
        << "routeId:"
        << routeId
        << "final position:"
        << finalPosition.latitude()
        << finalPosition.longitude()
        << "course:"
        << course;

    /*
     * --------------------------------------------------------
     * FINISH THE DATABASE ROUTE
     * --------------------------------------------------------
     */

    emit routeFinishedNeedsClear(
        shipId,
        routeId,
        finalPosition.latitude(),
        finalPosition.longitude(),
        route.waypointIndex,
        course
        );

    /*
     * --------------------------------------------------------
     * NOTIFY QML
     * --------------------------------------------------------
     */

    emit navigationFinished(
        shipId
        );

    /*
     * --------------------------------------------------------
     * REMOVE ACTIVE ROUTE
     * --------------------------------------------------------
     */

    m_activeRoutes.erase(
        it
        );

    /*
     * --------------------------------------------------------
     * STOP TIMER IF THIS WAS THE LAST ACTIVE SHIP
     * --------------------------------------------------------
     */

    if (m_activeRoutes.isEmpty())
    {
        m_navigationTimer.stop();
    }
}

void Navigation::validatePositionAsync(
    double latitude,
    double longitude)
{
    m_navigationPool.start(
        new NavigationTask(
            [this, latitude, longitude]()
            {
                QGeoCoordinate result;

                {
                    std::lock_guard<std::recursive_mutex>
                        lock(m_navigationMutex);

                    result =
                        validatePosition(
                            latitude,
                            longitude);
                }

                QMetaObject::invokeMethod(
                    this,

                    [this,
                     latitude,
                     longitude,
                     result]()
                    {
                        emit positionValidationFinished(
                            latitude,
                            longitude,
                            result.latitude(),
                            result.longitude(),
                            result.isValid());
                    },

                    Qt::QueuedConnection);
            }
            ));
}

void Navigation::loadWaterPolygonsAsync(
    const QString &shpPath)
{
    m_navigationPool.start(
        new NavigationTask(
            [this, shpPath]()
            {
                bool success = false;

                {
                    std::lock_guard<std::recursive_mutex>
                        lock(m_navigationMutex);

                    success =
                        loadWaterPolygons(
                            shpPath);
                }

                QMetaObject::invokeMethod(
                    this,

                    [this, success]()
                    {
                        emit waterPolygonsLoaded(
                            success);
                    },

                    Qt::QueuedConnection);
            }
            ));
}

bool Navigation::isWaterOnLayer(
    OGRLayer *layer,
    double latitude,
    double longitude)
{
    if (!layer)
        return false;

    if (!validCoordinates(
            latitude,
            longitude))
    {
        return false;
    }

    OGRPoint point(
        longitude,
        latitude);

    layer->SetSpatialFilter(
        &point);

    layer->ResetReading();

    bool water = false;

    OGRFeature *feature = nullptr;

    while (
        (feature =
         layer->GetNextFeature())
        != nullptr)
    {
        const OGRGeometry *geometry =
            feature->GetGeometryRef();

        if (geometry &&
            geometry->Intersects(&point))
        {
            water = true;

            OGRFeature::DestroyFeature(
                feature);

            break;
        }

        OGRFeature::DestroyFeature(
            feature);
    }

    layer->SetSpatialFilter(
        nullptr);

    layer->ResetReading();

    return water;
}

Navigation::CellState Navigation::classifyH3IndexOnLayer(
    OGRLayer *layer,
    H3Index h3)
{
    if (!layer || h3 == H3_NULL)
        return CellState::Land;

    LatLng center{};
    if (cellToLatLng(h3, &center) != E_SUCCESS)
    {
        return CellState::Land;
    }

    const double centerLat = radsToDegs(center.lat);
    // > 72.0 cuts off the sea over Northern Russia / Canada
    // < -60.0 cuts off Antarctica but leaves the Magellan Strait open
    if (centerLat > 72.0 || centerLat < -60.0)
    {
        return CellState::Land;
    }

    const double centerLon = radsToDegs(center.lng);

    CellBoundary boundary{};
    if (cellToBoundary(h3, &boundary) != E_SUCCESS)
    {
        return CellState::Land;
    }

    // ============================================================
    // PERFORMANCE OPTIMIZATION:
    // Calculate the bounding box of the hexagon cell.
    // ============================================================
    double minLat = centerLat, maxLat = centerLat;
    double minLon = centerLon, maxLon = centerLon;

    for (int i = 0; i < boundary.numVerts; ++i)
    {
        const double vLat = radsToDegs(boundary.verts[i].lat);
        const double vLon = radsToDegs(boundary.verts[i].lng);

        if (vLat < minLat) minLat = vLat;
        if (vLat > maxLat) maxLat = vLat;
        if (vLon < minLon) minLon = vLon;
        if (vLon > maxLon) maxLon = vLon;
    }

    // Ask GDAL to filter using the bounding box (Lightning fast)
    layer->SetSpatialFilterRect(minLon, minLat, maxLon, maxLat);
    layer->ResetReading();

    // Pull ONLY the polygons touching this cell into RAM
    std::vector<OGRGeometry*> localGeometries;
    OGRFeature *feature = nullptr;

    while ((feature = layer->GetNextFeature()) != nullptr)
    {
        OGRGeometry *geom = feature->GetGeometryRef();
        if (geom) {
            localGeometries.push_back(geom->clone());
        }
        OGRFeature::DestroyFeature(feature);
    }

    layer->SetSpatialFilter(nullptr); // Clear the filter immediately

    // If no water polygons even touch the cell's box, it is 100% land!
    if (localGeometries.empty())
    {
        return CellState::Land;
    }

    // ============================================================
    // FAST RAM CHECK:
    // Check our 7 points against the cached geometries in memory
    // ============================================================
    auto isWaterFast = [&localGeometries](double lat, double lon) {
        OGRPoint pt(lon, lat);
        for (OGRGeometry *geom : localGeometries) {
            if (geom->Intersects(&pt)) return true;
        }
        return false;
    };

    int waterCount = 0;
    const int totalCount = boundary.numVerts + 1;

    // Check Center
    if (isWaterFast(centerLat, centerLon)) {
        ++waterCount;
    }

    // Check Hexagon Vertices
    for (int i = 0; i < boundary.numVerts; ++i)
    {
        if (isWaterFast(radsToDegs(boundary.verts[i].lat), radsToDegs(boundary.verts[i].lng))) {
            ++waterCount;
        }
    }

    // Clean up our RAM geometries to prevent memory leaks
    for (OGRGeometry *geom : localGeometries) {
        OGRGeometryFactory::destroyGeometry(geom);
    }

    if (waterCount == 0) return CellState::Land;
    if (waterCount == totalCount) return CellState::PureWater;

    return CellState::Mixed;
}

//Globalresolutioncheckpointkindofthing y

void Navigation::setGlobalMaxResolution(int maxRes)
{
    m_maxResolution = maxRes;
}

void Navigation::addRefinementZone(
    double minLat, double minLon,
    double maxLat, double maxLon, int maxRes)
{
    m_refinementZones.append({minLat, minLon, maxLat, maxLon, maxRes});
}

void Navigation::refineGrid()
{
    qDebug() << "Navigation: starting grid refinement/resumption...";
    buildGlobalGrid();
    saveGridCache(m_gridCachePath, m_gridSourcePath);
    rebuildAdjacency();
}

QVariantList Navigation::h3DebugCellsNear(double centerLat, double centerLon, int maxCount) const
{
    QGeoCoordinate viewCenter(centerLat, centerLon);
    struct CellDistance {
        int index;
        double distance;
    };

    QVector<CellDistance> candidateCells;
    candidateCells.reserve(m_cells.size());

    // 1. Filter valid sea cells and calculate their distance to the map center
    for (int i = 0; i < m_cells.size(); ++i)
    {
        const SeaCell &cell = m_cells[i];
        if (cell.h3 == H3_NULL) continue;
        if (cell.state == CellState::Land) continue;
        if (cell.resolution < 15 && cell.state == CellState::Mixed) continue;

        if (!cell.center.isValid()) continue;

        double dist = viewCenter.distanceTo(cell.center);
        candidateCells.append({i, dist});
    }

    // 2. Sort by closest distance
    std::sort(candidateCells.begin(), candidateCells.end(), [](const CellDistance &a, const CellDistance &b) {
        return a.distance < b.distance;
    });

    // 3. Take only the top 'maxCount' cells
    int limit = std::min(maxCount, static_cast<int>(candidateCells.size()));
    QVariantList result;
    result.reserve(limit);

    for (int i = 0; i < limit; ++i)
    {
        const SeaCell &cell = m_cells[candidateCells[i].index];
        CellBoundary boundary{};
        if (cellToBoundary(cell.h3, &boundary) != E_SUCCESS)
            continue;

        QVariantList path;
        path.reserve(boundary.numVerts);

        for (int vertex = 0; vertex < boundary.numVerts; ++vertex)
        {
            path.append(QVariant::fromValue(QGeoCoordinate(
                radsToDegs(boundary.verts[vertex].lat),
                radsToDegs(boundary.verts[vertex].lng))));
        }

        QVariantMap polygon;
        polygon["path"] = path;
        polygon["resolution"] = cell.resolution;
        polygon["latitude"] = cell.center.latitude();
        polygon["longitude"] = cell.center.longitude();

        result.append(polygon);
    }

    qDebug() << "Navigation: Displaying closest" << result.size() << "H3 cells out of" << candidateCells.size();
    return result;
}

// ============================================================
// RESTORE NAVIGATION FROM DATABASE
// ============================================================

void Navigation::restoreNavigation(
    const QString& shipId,
    const QString& routeId,
    double currentLat,
    double currentLon,
    int shipSpeed,
    const QVariantList& path)
{
    std::lock_guard<std::recursive_mutex>
        lock(m_routesMutex);


    if (shipId.isEmpty())
    {
        return;
    }

    if (path.isEmpty())
    {
        return;
    }


    ActiveRoute route;


    /*
     * Restore exact database route ID.
     *
     * Old databases may not have one; generate one as a
     * fallback, although new routes always have one.
     */

    route.routeId =
        routeId.trimmed().isEmpty()
            ? QUuid::createUuid()
                  .toString(
                      QUuid::WithoutBraces
                      )
            : routeId.trimmed();


    route.speedMetersPerSecond =
        static_cast<double>(
            shipSpeed
            )
        *
        0.514444;


    route.currentPosition =
        QGeoCoordinate(
            currentLat,
            currentLon
            );


    route.isRerouting =
        false;

    route.isWaiting =
        false;

    route.rerouteCooldown =
        0;

    route.lastDbSaveTime =
        QDateTime::currentMSecsSinceEpoch();


    /*
     * Restore path.
     */

    for (const QVariant& value :
         path)
    {
        const QVariantMap point =
            value.toMap();

        const QGeoCoordinate coordinate(
            point.value(
                     "latitude")
                .toDouble(),

            point.value(
                     "longitude")
                .toDouble()
            );

        if (coordinate.isValid())
        {
            route.path.append(
                coordinate
                );
        }
    }


    if (route.path.size() < 2)
    {
        return;
    }


    /*
     * Find waypoint nearest to the persisted live position.
     */

    int bestIndex =
        0;

    double minDistance =
        std::numeric_limits<double>::max();


    for (int i = 0;
         i < route.path.size();
         ++i)
    {
        const double distance =
            route.currentPosition.distanceTo(
                route.path[i]
                );

        if (distance < minDistance)
        {
            minDistance =
                distance;

            bestIndex =
                i;
        }
    }


    /*
     * Next waypoint is after the closest point.
     *
     * Keep at least one segment available.
     */

    if (route.path.size() > 1)
    {
        route.waypointIndex = std::min(
            bestIndex + 1,
            static_cast<int>(route.path.size() - 1)
            );
    }
    else
    {
        route.waypointIndex = 0;
    }


    /*
     * Make the first path point exactly the persisted
     * current position.
     */

    route.path[0] =
        route.currentPosition;


    /*
     * This becomes the current generation.
     */

    ++m_routeRequestGeneration[
        shipId
    ];


    /*
     * Restore.
     */

    m_activeRoutes[
        shipId
    ] =
        route;


    if (!m_navigationTimer.isActive())
    {
        m_navigationTimer.start();
    }


    qDebug()
        << "Navigation restored:"
        << "ship:"
        << shipId
        << "routeId:"
        << route.routeId
        << "position:"
        << currentLat
        << currentLon;
}
QVariantList Navigation::getShipRouteHistory(const QString &shipId)
{
    database db;
    return db.getFinishedRoutesForShip(shipId);
}
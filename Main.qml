import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtLocation
import QtPositioning
import MapLibre 3.0

ApplicationWindow {
    id: window

    width: 1366
    height: 768

    minimumWidth: 200
    minimumHeight: 250

    visible: true
    title: qsTr("Radar")

    property bool lightMode:
        Application.styleHints.colorScheme === Qt.Light

    property color reallyDark: "#1f1f1f"
    property color dark: "#262626"
    property color reallyLight: "#e7e7e7"
    property color light: "#e0e0e0"

    // ============================================================
    // FIRST PANEL
    // ============================================================

    property string firstPanelShipId: ""

    property string selectedName: ""
    property string selectedFlag: ""
    property real selectedLatitude: 0
    property real selectedLongitude: 0
    property real selectedSpeed: 0
    property int selectedCourse: 0
    property string selectedClass: ""
    property string selectedType: ""
    property string selectedAffiliation: ""

    // ============================================================
    // SECOND PANEL
    // ============================================================

    property string secondPanelShipId: ""

    property string secondSelectedName: ""
    property string secondSelectedFlag: ""
    property real secondSelectedLatitude: 0
    property real secondSelectedLongitude: 0
    property int secondSelectedSpeed: 0
    property int secondSelectedCourse: 0
    property string secondSelectedClass: ""
    property string secondSelectedType: ""
    property string secondSelectedAffiliation: ""

    property bool firstPanelOpen: false
    property bool secondPanelOpen: false
    property bool terminalOpen: false

    // ============================================================
    // NAVIGATION
    // ============================================================

    property string navigationShipId: ""
    property bool selectingNavigationDestination: false

    property real pendingNavigationLatitude: NaN
    property real pendingNavigationLongitude: NaN

    property var navigatingShips: ({})

    // ============================================================
    // ACTIVE NAVIGATION ROUTES
    // ============================================================

    property var shipRoutes: ({})
    property bool showAllRoutes: false

    property var destinationPreviewLine: null
    property string destinationPreviewShipId: ""

    // ============================================================
    // CURRENT TRAVEL PATHS
    // ============================================================

    property var currentTravelPaths: ({})

    // ============================================================
    // HISTORICAL ROUTES
    // ============================================================

    property var firstPanelHistoryData: []
    property var secondPanelHistoryData: []

    property var activeHistoricalRoute: null
    property var activeHistoricalRoutes: []

    property string displayedRouteMode: ""
    property string displayedRouteShipId: ""

    // ============================================================
    // HISTORICAL PLAYBACK
    // ============================================================

    property bool historicalPlaybackRunning: false
    property int historicalPlaybackIndex: -1
    property var historicalPlaybackRoutes: []

    // ============================================================
    // GHOST / PLAYBACK
    // ============================================================

    property bool ghostRunning: false
    property string ghostShipId: ""

    property var ghostPath: []
    property int ghostSegmentIndex: 0
    property real ghostSegmentDistance: 0

    property real ghostSpeedKnots: 0
    property real ghostSpeedMetersPerSecond: 0

    property var ghostObject: null

    // ============================================================
    // H3
    // ============================================================

    property bool showH3Grid: false
    property var h3PolygonPool: []
    property int activePolygons: 0

    // ============================================================
    // MAP STYLE
    // ============================================================

    property int mapStyle: 0

    property string normalStyle:
        "file:///home/aysat/RadarDatabase/json/colored_style.json"

    property string lightStyle:
        "file:///home/aysat/RadarDatabase/json/white_style.json"

    property string darkStyle:
        "file:///home/aysat/RadarDatabase/json/black_style.json"

    // ------------------------------------------------------------
    // ACTIVE A* NAVIGATION ROUTE
    // ------------------------------------------------------------

    property color navigationLineLight:
        "#000000"

    property color navigationLineDark:
        "#60a5fa"

    property color navigationLineNormal:
        "#000000"

    // ------------------------------------------------------------
    // CURRENT TRAVEL / SELECTED SHIP ROUTE
    // ------------------------------------------------------------

    property color currentLineLight:
        "#000000"

    property color currentLineDark:
        "#f59e0b"

    property color currentLineNormal:
        "#000000"

    // ------------------------------------------------------------
    // HISTORICAL SAVED ROUTE
    // ------------------------------------------------------------

    property color historicalLineLight:
        "#FF0000"

    property color historicalLineDark:
        "#f59e0b"

    property color historicalLineNormal:
        "#FF0000"

    // ------------------------------------------------------------
    // RIGHT-CLICK A* PREVIEW
    // ------------------------------------------------------------

    property color previewLineLight:
        "#000000"

    property color previewLineDark:
        "#f97316"

    property color previewLineNormal:
        "#000000"

    // ------------------------------------------------------------
    // WIDTHS
    // ------------------------------------------------------------

    property real navigationLineWidth:
        4.0

    property real currentLineWidth:
        4.0

    property real historicalLineWidth:
        4.0

    property real previewLineWidth:
        4.0

    // ============================================================
    // SHIP MARKERS
    // ============================================================

    property var shipMarkers: ({})

    // ============================================================
    // LOG
    // ============================================================

    ListModel {
        id: logModel
    }

    function appendLog(message) {
        var date = new Date()

        var timeStr =
            date.toLocaleTimeString(
                Qt.locale(),
                "hh:mm:ss"
            )

        logModel.append({
            "time": timeStr,
            "message": message
        })
    }

    // ============================================================
    // ROUTE COLOR HELPERS
    // ============================================================

    function getNavigationLineColor() {

        switch (mapStyle) {

        case 0:
            return navigationLineLight

        case 1:
            return navigationLineDark

        case 2:
        default:
            return navigationLineNormal
        }
    }

    function getCurrentLineColor() {

        switch (mapStyle) {

        case 0:
            return currentLineLight

        case 1:
            return currentLineDark

        case 2:
        default:
            return currentLineNormal
        }
    }

    function getHistoricalLineColor() {

        switch (mapStyle) {

        case 0:
            return historicalLineLight

        case 1:
            return historicalLineDark

        case 2:
        default:
            return historicalLineNormal
        }
    }

    function getPreviewLineColor() {

        switch (mapStyle) {

        case 0:
            return previewLineLight

        case 1:
            return previewLineDark

        case 2:
        default:
            return previewLineNormal
        }
    }

    // ============================================================
    // FIND SHIP
    // ============================================================

    function findShipById(id) {
        if (!id || id === "")
            return null

        return shipMarkers[id] || null
    }

    // ============================================================
    // PANEL ANIMATION
    // ============================================================

    function firstOpenPanel() {
        firstPanelAnimation.to =
            window.height -
            firstInformationPanel.height -
            20

        firstPanelAnimation.start()
    }

    function firstClosePanel() {
        firstPanelAnimation.to =
            window.height

        firstPanelAnimation.start()
    }

    function secondOpenPanel() {
        secondPanelAnimation.to =
            window.height -
            secondInformationPanel.height -
            20

        secondPanelAnimation.start()
    }

    function secondClosePanel() {
        secondPanelAnimation.to =
            window.height

        secondPanelAnimation.start()
    }

    // ============================================================
    // TERMINAL
    // ============================================================

    function toggleTerminal() {
        terminalOpen =
            !terminalOpen

        terminalAnimation.to =
            terminalOpen
            ? window.height - terminalPanel.height
            : window.height

        terminalAnimation.start()
    }

    // ============================================================
    // ACTIVE NAVIGATION ROUTE VISIBILITY
    //
    // THIS IS THE ORIGINAL "SHOW ALL ROUTES".
    //
    // It only controls currently ongoing routes.
    // ============================================================

    function updateRouteVisibility() {

        var keys =
            Object.keys(shipRoutes)

        for (
            var i = 0;
            i < keys.length;
            ++i
        ) {

            var shipId =
                keys[i]

            var routeData =
                shipRoutes[shipId]

            if (
                routeData &&
                routeData.line
            ) {

                routeData.line.visible =
                    showAllRoutes ||
                    shipId === firstPanelShipId ||
                    shipId === secondPanelShipId
            }
        }
    }

    // ============================================================
    // CLEAR ACTIVE NAVIGATION ROUTE
    // ============================================================

    function clearRouteForShip(shipId) {

        if (!shipRoutes[shipId])
            return

        var line =
            shipRoutes[shipId].line

        if (line) {

            map.map.removeMapItem(
                line
            )

            line.destroy()
        }

        delete shipRoutes[shipId]

        updateRouteVisibility()
    }

    // ============================================================
    // CLEAR RIGHT-CLICK DESTINATION PREVIEW
    // ============================================================

    function clearDestinationPreview() {

        if (destinationPreviewLine) {

            map.map.removeMapItem(
                destinationPreviewLine
            )

            destinationPreviewLine.destroy()

            destinationPreviewLine =
                null
        }

        destinationPreviewShipId =
            ""
    }

    // ============================================================
    // CLEAR HISTORICAL ROUTES
    // ============================================================

    function clearDisplayedHistoricalRoutes() {

        if (activeHistoricalRoute) {

            map.map.removeMapItem(
                activeHistoricalRoute
            )

            activeHistoricalRoute.destroy()

            activeHistoricalRoute =
                null
        }

        for (
            var i = 0;
            i < activeHistoricalRoutes.length;
            ++i
        ) {

            var routeLine =
                activeHistoricalRoutes[i]

            if (routeLine) {

                map.map.removeMapItem(
                    routeLine
                )

                routeLine.destroy()
            }
        }

        activeHistoricalRoutes = []
    }

    // ============================================================
    // STOP HISTORICAL PLAYBACK
    // ============================================================

    function stopHistoricalPlayback() {

        historicalPlaybackTimer.stop()

        historicalPlaybackRunning =
            false

        historicalPlaybackIndex =
            -1

        historicalPlaybackRoutes =
            []
    }

    // ============================================================
    // CLEAR GHOST
    // ============================================================

    function clearGhost() {

        ghostTimer.stop()

        ghostRunning =
            false

        ghostShipId =
            ""

        ghostPath =
            []

        ghostSegmentIndex =
            0

        ghostSegmentDistance =
            0

        ghostSpeedKnots =
            0

        ghostSpeedMetersPerSecond =
            0

        if (ghostObject) {

            map.map.removeMapItem(
                ghostObject
            )

            ghostObject.destroy()

            ghostObject =
                null
        }
    }

    // ============================================================
    // CLEAR HISTORICAL VISUALIZATION
    // ============================================================

    function clearHistoricalVisualization() {

        stopHistoricalPlayback()

        clearGhost()

        clearDisplayedHistoricalRoutes()

        displayedRouteMode =
            ""

        displayedRouteShipId =
            ""
    }

    // ============================================================
    // CONVERT ROUTE DATA TO COORDINATES
    // ============================================================

    function makeCoordinates(pathData) {

        var coords = []

        if (!pathData)
            return coords

        for (
            var i = 0;
            i < pathData.length;
            ++i
        ) {

            var latitude =
                Number(
                    pathData[i].latitude
                )

            var longitude =
                Number(
                    pathData[i].longitude
                )

            if (
                !isFinite(latitude) ||
                !isFinite(longitude)
            ) {
                continue
            }

            var coordinate =
                QtPositioning.coordinate(
                    latitude,
                    longitude
                )

            if (coordinate.isValid)
                coords.push(
                    coordinate
                )
        }

        return coords
    }

    // ============================================================
    // REQUEST A* DESTINATION PREVIEW
    //
    // This asks C++ to calculate the actual H3/A* route.
    // It does NOT start navigation.
    // ============================================================

    function requestDestinationPreview(
            shipId,
            latitude,
            longitude)
    {
        var ship =
            findShipById(
                shipId
            )

        if (!ship)
            return

        clearDestinationPreview()

        destinationPreviewShipId =
            shipId

        navigation.previewPath(
            shipId,

            Number(ship.shipLatitude),
            Number(ship.shipLongitude),

            Number(latitude),
            Number(longitude)
        )
    }

    // ============================================================
    // DRAW HISTORICAL ROUTE IN BLUE
    // ============================================================

    function drawHistoricalRoute(pathData) {

        var coords =
            makeCoordinates(pathData)

        if (coords.length < 2)
            return false

        activeHistoricalRoute =
            historicalRouteComponent.createObject(
                map.map
            )

        if (!activeHistoricalRoute)
            return false

        activeHistoricalRoute.path =
            coords

        map.map.addMapItem(
            activeHistoricalRoute
        )

        return true
    }

    // ============================================================
    // SHOW ONE HISTORICAL ROUTE
    // ============================================================

    function showHistoricalRoute(pathData) {

        clearHistoricalVisualization()

        if (
            !drawHistoricalRoute(
                pathData
            )
        ) {
            return
        }

        displayedRouteMode =
            "historical"

        displayedRouteShipId =
            ""
    }

    // ============================================================
    // SHOW ALL HISTORICAL ROUTES
    //
    // DIFFERENT FROM "SHOW ALL ROUTES".
    //
    // Show All Routes:
    //     all currently active navigation routes
    //
    // Show All Historical Routes:
    //     all saved routes for selected ship
    // ============================================================

    function showAllHistoricalRoutes(routes) {

        clearHistoricalVisualization()

        if (
            !routes ||
            routes.length === 0
        ) {
            return
        }

        for (
            var r = 0;
            r < routes.length;
            ++r
        ) {

            var coords =
                makeCoordinates(
                    routes[r].path
                )

            if (coords.length < 2)
                continue

            var line =
                historicalRouteComponent.createObject(
                    map.map
                )

            if (!line)
                continue

            line.path =
                coords

            map.map.addMapItem(
                line
            )

            activeHistoricalRoutes.push(
                line
            )
        }

        displayedRouteMode =
            "historical"

        displayedRouteShipId =
            ""
    }

    // ============================================================
    // PLAY SAVED ROUTES OLDEST -> NEWEST
    // ============================================================

    function playHistoricalRoutes(routes) {

        clearHistoricalVisualization()

        if (
            !routes ||
            routes.length === 0
        ) {
            return
        }

        // ComboBox is newest -> oldest.
        //
        // Playback must be oldest -> newest.
        historicalPlaybackRoutes =
            routes.slice().reverse()

        historicalPlaybackIndex =
            0

        historicalPlaybackRunning =
            true

        historicalPlaybackShowNext()
    }

    // ============================================================
    // NEXT HISTORICAL ROUTE
    // ============================================================

    function historicalPlaybackShowNext() {

        if (!historicalPlaybackRunning)
            return

        if (
            historicalPlaybackIndex < 0 ||
            historicalPlaybackIndex >=
                historicalPlaybackRoutes.length
        ) {

            stopHistoricalPlayback()

            return
        }

        clearDisplayedHistoricalRoutes()

        var route =
            historicalPlaybackRoutes[
                historicalPlaybackIndex
            ]

        if (
            route &&
            route.path &&
            route.path.length >= 2
        ) {

            drawHistoricalRoute(
                route.path
            )

            displayedRouteMode =
                "historical"

            displayedRouteShipId =
                ""
        }

        historicalPlaybackIndex++

        if (
            historicalPlaybackIndex <
            historicalPlaybackRoutes.length
        ) {
            historicalPlaybackTimer.start()
        }
        else {

            historicalPlaybackRunning =
                false

            historicalPlaybackIndex =
                -1

            historicalPlaybackRoutes =
                []
        }
    }

    // ============================================================
    // GET SPEED USED BY PLAYBACK
    //
    // User-entered speed is preferred.
    // If invalid, fall back to actual ship speed.
    //
    // Ship speed is assumed to be knots.
    // ============================================================

    function getPlaybackSpeedKnots(
            shipId,
            speedText)
    {
        var speed =
            Number(speedText)

        if (
            !isFinite(speed) ||
            speed < 0
        ) {

            var ship =
                findShipById(shipId)

            if (ship)
                speed =
                    Number(
                        ship.shipSpeed
                    )
        }

        if (
            !isFinite(speed) ||
            speed < 0
        ) {
            speed = 0
        }

        return speed
    }

    // ============================================================
    // START PLAYBACK / GHOST
    //
    // "Playback" means:
    //
    // 1. Draw selected route in blue.
    // 2. Create translucent ship.
    // 3. Move ship along route using ship speed.
    // ============================================================

    function startGhostRoute(
            routeData,
            shipId,
            speedText)
    {
        clearGhost()
        clearDisplayedHistoricalRoutes()

        if (
            !routeData ||
            !routeData.path ||
            routeData.path.length < 2
        ) {
            return
        }

        var ship =
            findShipById(shipId)

        if (!ship)
            return

        ghostPath =
            makeCoordinates(
                routeData.path
            )

        if (ghostPath.length < 2)
            return

        // ------------------------------------------
        // Draw the historical route in blue.
        // ------------------------------------------

        activeHistoricalRoute =
            historicalRouteComponent.createObject(
                map.map
            )

        if (activeHistoricalRoute) {

            activeHistoricalRoute.path =
                ghostPath

            map.map.addMapItem(
                activeHistoricalRoute
            )
        }

        // ------------------------------------------
        // Set playback speed.
        //
        // Knots -> meters/second.
        // 1 knot = 0.514444 m/s.
        // ------------------------------------------

        ghostSpeedKnots =
            getPlaybackSpeedKnots(
                shipId,
                speedText
            )

        ghostSpeedMetersPerSecond =
            ghostSpeedKnots *
            0.514444

        ghostShipId =
            shipId

        ghostSegmentIndex =
            0

        ghostSegmentDistance =
            0

        // ------------------------------------------
        // Create translucent playback ship.
        // ------------------------------------------

        ghostObject =
            ghostShipComponent.createObject(
                map.map,
                {
                    "coordinate":
                        ghostPath[0],

                    "shipType":
                        ship.shipType
                }
            )

        if (!ghostObject) {

            clearGhost()

            return
        }

        map.map.addMapItem(
            ghostObject
        )

        ghostRunning =
            true

        displayedRouteMode =
            "historical"

        displayedRouteShipId =
            ""

        ghostTimer.start()
    }

    // ============================================================
    // UPDATE GHOST POSITION
    //
    // Movement is based on physical distance rather than
    // simply jumping to the next stored route point.
    // ============================================================

    function updateGhostPosition(deltaSeconds) {

        if (!ghostRunning)
            return

        if (!ghostObject) {
            clearGhost()
            return
        }

        if (
            ghostPath.length < 2
        ) {
            clearGhost()
            return
        }

        var distanceToMove =
            ghostSpeedMetersPerSecond *
            deltaSeconds

        // Zero speed means the ghost stays at its
        // current location.
        if (
            distanceToMove <= 0
        ) {
            return
        }

        while (
            distanceToMove > 0 &&
            ghostSegmentIndex <
                ghostPath.length - 1
        ) {

            var start =
                ghostPath[
                    ghostSegmentIndex
                ]

            var end =
                ghostPath[
                    ghostSegmentIndex + 1
                ]

            var segmentLength =
                start.distanceTo(
                    end
                )

            if (
                segmentLength <= 0.001
            ) {

                ghostSegmentIndex++

                ghostSegmentDistance =
                    0

                continue
            }

            var remaining =
                segmentLength -
                ghostSegmentDistance

            // The ghost reaches the end of this segment.
            if (
                distanceToMove >= remaining
            ) {

                ghostSegmentDistance =
                    segmentLength

                ghostObject.coordinate =
                    end

                distanceToMove -=
                    remaining

                ghostSegmentIndex++

                ghostSegmentDistance =
                    0

            }
            else {

                ghostSegmentDistance +=
                    distanceToMove

                var bearing =
                    start.azimuthTo(
                        end
                    )

                ghostObject.coordinate =
                    start.atDistanceAndAzimuth(
                        ghostSegmentDistance,
                        bearing
                    )

                distanceToMove =
                    0
            }
        }

        // Route finished.
        if (
            ghostSegmentIndex >=
                ghostPath.length - 1
        ) {

            ghostObject.coordinate =
                ghostPath[
                    ghostPath.length - 1
                ]

            clearGhost()
        }
    }

    // ============================================================
    // CURRENT TRAVEL PATH
    // ============================================================

    function startCurrentTravelPath(
            shipId,
            coordinate)
    {
        var path =
            currentTravelPaths[
                shipId
            ] || []

        path =
            path.slice()

        if (
            path.length === 0
        ) {
            path.push(
                coordinate
            )
        }

        currentTravelPaths[
            shipId
        ] =
            path
    }

    function appendCurrentTravelPoint(
            shipId,
            coordinate)
    {
        var path =
            currentTravelPaths[
                shipId
            ] || []

        path =
            path.slice()

        if (
            path.length === 0
        ) {

            path.push(
                coordinate
            )

        }
        else {

            var last =
                path[
                    path.length - 1
                ]

            if (
                last.distanceTo(
                    coordinate
                ) >= 1.0
            ) {

                path.push(
                    coordinate
                )
            }
        }

        currentTravelPaths[
            shipId
        ] =
            path

        if (
            displayedRouteMode ===
                "current" &&
            displayedRouteShipId ===
                shipId &&
            activeHistoricalRoute
        ) {

            activeHistoricalRoute.path =
                path
        }
    }

    // ============================================================
    // DRAW RIGHT-CLICK DESTINATION LINE
    //
    // This does NOT start navigation.
    // It only draws a straight visual line from the ship
    // to the location selected with right-click.
    // ============================================================

    function drawDestinationLine(
            shipId,
            latitude,
            longitude)
    {
        clearDestinationPreview()

        var ship =
            findShipById(
                shipId
            )

        if (!ship)
            return false

        var startCoordinate =
            QtPositioning.coordinate(
                Number(ship.shipLatitude),
                Number(ship.shipLongitude)
            )

        var endCoordinate =
            QtPositioning.coordinate(
                Number(latitude),
                Number(longitude)
            )

        if (
            !startCoordinate.isValid ||
            !endCoordinate.isValid
        ) {
            return false
        }

        var path = [
            startCoordinate,
            endCoordinate
        ]

        destinationPreviewLine =
            destinationRouteComponent.createObject(
                map.map
            )

        if (!destinationPreviewLine) {
            return false
        }

        destinationPreviewLine.path =
            path

        map.map.addMapItem(
            destinationPreviewLine
        )

        destinationPreviewShipId =
            shipId

        return true
    }

    // ============================================================
    // SHOW CURRENT ONGOING NAVIGATION
    // ============================================================

    function showCurrentNavigation(
            shipId)
    {
        stopHistoricalPlayback()
        clearGhost()
        clearDisplayedHistoricalRoutes()

        var path =
            currentTravelPaths[
                shipId
            ] || []

        if (
            path.length === 0
        ) {
            return
        }

        activeHistoricalRoute =
            historicalRouteComponent.createObject(
                map.map
            )

        if (!activeHistoricalRoute)
            return

        activeHistoricalRoute.path =
            path.slice()

        map.map.addMapItem(
            activeHistoricalRoute
        )

        displayedRouteMode =
            "current"

        displayedRouteShipId =
            shipId
    }

    // ============================================================
    // LOAD FIRST PANEL HISTORY
    // ============================================================

    function loadFirstPanelHistory(
            shipId)
    {
        firstRouteModel.clear()

        var history =
            navigation.getShipRouteHistory(
                shipId
            )

        if (!history)
            history = []

        // Assumed database order:
        // oldest -> newest
        //
        // UI order:
        // newest -> oldest
        firstPanelHistoryData =
            history.slice().reverse()

        for (
            var i = 0;
            i < firstPanelHistoryData.length;
            ++i
        ) {

            firstRouteModel.append({
                "display":
                    firstPanelHistoryData[
                        i
                    ].display
            })
        }

        firstCombo.currentIndex =
                    firstPanelHistoryData.length > 0 ? 0 : -1
    }

    // ============================================================
    // LOAD SECOND PANEL HISTORY
    // ============================================================

    function loadSecondPanelHistory(
            shipId)
    {
        secondRouteModel.clear()

        var history =
            navigation.getShipRouteHistory(
                shipId
            )

        if (!history)
            history = []

        secondPanelHistoryData =
            history.slice().reverse()

        for (
            var i = 0;
            i < secondPanelHistoryData.length;
            ++i
        ) {

            secondRouteModel.append({
                "display":
                    secondPanelHistoryData[
                        i
                    ].display
            })
        }

        secondCombo.currentIndex =
                    secondPanelHistoryData.length > 0 ? 0 : -1
    }

    // ============================================================
    // SELECT FIRST SHIP
    // ============================================================

    function selectFirstShip(
            shipMarker)
    {
        secondClosePanel()
        secondPanelOpen =
            false

        secondPanelShipId =
            ""

        firstPanelShipId =
            shipMarker.shipId

        selectedName =
            shipMarker.shipName

        selectedFlag =
            shipMarker.shipFlag

        selectedLatitude =
            shipMarker.shipLatitude

        selectedLongitude =
            shipMarker.shipLongitude

        selectedSpeed =
            shipMarker.shipSpeed

        selectedCourse =
            shipMarker.shipCourse

        selectedClass =
            shipMarker.shipClass

        selectedType =
            shipMarker.shipType

        selectedAffiliation =
            shipMarker.shipAffiliation

        loadFirstPanelHistory(
            shipMarker.shipId
        )

        // --------------------------------------------------------
        // Automatically show current navigation route.
        // --------------------------------------------------------

        if (
            navigatingShips[
                shipMarker.shipId
            ] &&
            currentTravelPaths[
                shipMarker.shipId
            ]
        ) {

            showCurrentNavigation(
                shipMarker.shipId
            )

        }
        else {

            clearHistoricalVisualization()
        }

        firstOpenPanel()

        firstPanelOpen =
            true
    }

    // ============================================================
    // SELECT SECOND SHIP
    // ============================================================

    function selectSecondShip(
            shipMarker)
    {
        firstClosePanel()
        firstPanelOpen = false
        firstPanelShipId = ""
        secondPanelShipId =
            shipMarker.shipId

        secondSelectedName =
            shipMarker.shipName

        secondSelectedFlag =
            shipMarker.shipFlag

        secondSelectedLatitude =
            shipMarker.shipLatitude

        secondSelectedLongitude =
            shipMarker.shipLongitude

        secondSelectedSpeed =
            shipMarker.shipSpeed

        secondSelectedCourse =
            shipMarker.shipCourse

        secondSelectedClass =
            shipMarker.shipClass

        secondSelectedType =
            shipMarker.shipType

        secondSelectedAffiliation =
            shipMarker.shipAffiliation

        loadSecondPanelHistory(
            shipMarker.shipId
        )

        if (
            navigatingShips[
                shipMarker.shipId
            ] &&
            currentTravelPaths[
                shipMarker.shipId
            ]
        ) {

            showCurrentNavigation(
                shipMarker.shipId
            )

        }
        else {

            clearHistoricalVisualization()
        }

        firstClosePanel()
        secondOpenPanel()

        firstPanelOpen =
            false

        secondPanelOpen =
            true
    }

    // ============================================================
    // RELOAD FIRST PANEL USING SECOND PANEL
    // ============================================================

    function reloadFirstPanel() {

        if (
            firstPanelShipId === "" ||
            !firstPanelOpen
        ) {
            return
        }

        var ship =
            findShipById(
                firstPanelShipId
            )

        if (!ship)
            return

        selectSecondShip(ship)
    }


    // ============================================================
    // RELOAD SECOND PANEL USING FIRST PANEL
    // ============================================================

    function reloadSecondPanel() {

        if (
            secondPanelShipId === "" ||
            !secondPanelOpen
        ) {
            return
        }

        var ship =
            findShipById(
                secondPanelShipId
            )

        if (!ship)
            return

        selectFirstShip(ship)
    }

    // ============================================================
    // MAP PLUGIN
    // ============================================================

    Plugin {
        id: mapPlugin

        name:
            "maplibre"

        PluginParameter {

            name:
                "maplibre.map.styles"

            value: [
                window.lightStyle,
                window.darkStyle,
                window.normalStyle
            ]
        }
    }

    // ============================================================
    // SHIP MODEL
    // ============================================================

    Connections {

        target:
            shipModel

        function onModelReset() {

            refreshShips(
                shipModel.getShipsForQml()
            )
        }
    }

    // ============================================================
    // NAVIGATION CONNECTION
    // ============================================================

    Connections {

        target:
            navigation

        // ========================================================
        // SHIP POSITION CHANGED
        // ========================================================

        function onShipPositionChanged(
                shipId,
                latitude,
                longitude,
                calculatedCourse)
        {
            var ship =
                findShipById(
                    shipId
                )

            if (!ship)
                return

            ship.shipLatitude =
                latitude

            ship.shipLongitude =
                longitude

            ship.shipCourse =
                    calculatedCourse

            ship.coordinate =
                QtPositioning.coordinate(
                    latitude,
                    longitude
                )

            if (
                shipId ===
                firstPanelShipId
            ) {

                selectedLatitude =
                    latitude

                selectedLongitude =
                    longitude

                selectedCourse =
                        calculatedCourse
            }

            if (
                shipId ===
                secondPanelShipId
            ) {

                secondSelectedLatitude =
                    latitude

                secondSelectedLongitude =
                    longitude

                secondSelectedCourse =
                        calculatedCourse
            }

            var routeData =
                shipRoutes[
                    shipId
                ]

            if (
                routeData &&
                routeData.line
            ) {

                var pArr =
                    routeData.pathArr.slice()

                var currentPos =
                    QtPositioning.coordinate(
                        latitude,
                        longitude
                    )

                if (
                    pArr.length > 1
                ) {

                    if (
                        currentPos.distanceTo(
                            pArr[1]
                        ) < 20.0
                    ) {

                        pArr.splice(
                            1,
                            1
                        )
                    }

                    pArr[0] =
                        currentPos

                    routeData.line.path =
                        pArr

                    routeData.pathArr =
                        pArr
                }
            }

            appendCurrentTravelPoint(
                shipId,
                QtPositioning.coordinate(
                    latitude,
                    longitude
                )
            )

            if (
                displayedRouteMode ===
                    "current" &&
                displayedRouteShipId ===
                    shipId &&
                activeHistoricalRoute
            ) {

                activeHistoricalRoute.path =
                    currentTravelPaths[
                        shipId
                    ] || []
            }
        }

        // ========================================================
        // NAVIGATION FINISHED
        // ========================================================

        function onNavigationFinished(
                shipId)
        {
            var ship =
                findShipById(
                    shipId
                )

            var sName =
                ship
                ? ship.shipName
                : "Unknown Ship"

            if (
                destinationPreviewShipId ===
                shipId
            ) {
                clearDestinationPreview()
            }

            navigatingShips[
                shipId
            ] = false

            if (
                displayedRouteMode ===
                    "current" &&
                displayedRouteShipId ===
                    shipId
            ) {

                clearDisplayedHistoricalRoutes()

                displayedRouteMode =
                    ""

                displayedRouteShipId =
                    ""
            }

            if (
                shipRoutes[
                    shipId
                ]
            ) {

                var routeData =
                    shipRoutes[
                        shipId
                    ]

                appendLog(
                    sName +
                    " has finished navigation from " +
                    routeData.startLat +
                    " " +
                    routeData.startLon +
                    " to " +
                    routeData.endLat +
                    " " +
                    routeData.endLon
                )

            }
            else {

                appendLog(
                    sName +
                    " has finished its saved navigation."
                )
            }

            clearRouteForShip(
                shipId
            )

            delete currentTravelPaths[
                shipId
            ]
            // Reload the visible panel so the newly saved route/status
            // is fetched from the database and displayed immediately.

            if (
                firstPanelOpen &&
                firstPanelShipId === shipId
            ) {

                reloadFirstPanel()

            }
            else if (
                secondPanelOpen &&
                secondPanelShipId === shipId
            ) {

                reloadSecondPanel()
            }
        }

        // ========================================================
        // NAVIGATION FAILED
        // ========================================================

        function onNavigationFailed(
                shipId,
                reason)
        {
            var ship =
                findShipById(
                    shipId
                )

            var sName =
                ship
                ? ship.shipName
                : "Unknown Ship"

            if (
                destinationPreviewShipId ===
                shipId
            ) {
                clearDestinationPreview()
            }

            navigatingShips[
                shipId
            ] = false

            if (
                displayedRouteMode ===
                    "current" &&
                displayedRouteShipId ===
                    shipId
            ) {

                clearDisplayedHistoricalRoutes()

                displayedRouteMode =
                    ""

                displayedRouteShipId =
                    ""
            }

            appendLog(
                sName +
                " navigation failed: " +
                reason
            )

            clearRouteForShip(
                shipId
            )

            delete currentTravelPaths[
                shipId
            ]
        }

        // ========================================================
        // PATH READY
        // ========================================================

        function onPathReady(
                shipId,
                path)
        {
            var coordinates = []

            for (
                var i = 0;
                i < path.length;
                ++i
            ) {

                var coordinate =
                    QtPositioning.coordinate(
                        Number(
                            path[i].latitude
                        ),
                        Number(
                            path[i].longitude
                        )
                    )

                if (
                    coordinate.isValid
                ) {

                    coordinates.push(
                        coordinate
                    )
                }
            }

            if (
                coordinates.length < 2
            ) {
                return
            }

            navigatingShips[
                shipId
            ] = true

            if (
                !currentTravelPaths[
                    shipId
                ]
            ) {

                startCurrentTravelPath(
                    shipId,
                    coordinates[0]
                )
            }

            var ship =
                findShipById(
                    shipId
                )

            var sName =
                ship
                ? ship.shipName
                : "Unknown Ship"

            var startLat =
                coordinates[0]
                    .latitude
                    .toFixed(6)

            var startLon =
                coordinates[0]
                    .longitude
                    .toFixed(6)

            var endLat =
                coordinates[
                    coordinates.length - 1
                ]
                .latitude
                .toFixed(6)

            var endLon =
                coordinates[
                    coordinates.length - 1
                ]
                .longitude
                .toFixed(6)

            if (
                shipRoutes[
                    shipId
                ] &&
                shipRoutes[
                    shipId
                ].line
            ) {

                shipRoutes[
                    shipId
                ].line.path =
                    coordinates

                shipRoutes[
                    shipId
                ].pathArr =
                    coordinates

                appendLog(
                    sName +
                    " encountered another ship, recalculating the route."
                )

                return
            }

            appendLog(
                sName +
                " has started going from " +
                startLat +
                " " +
                startLon +
                " to " +
                endLat +
                " " +
                endLon
            )

            var newLine =
                navigationRouteComponent.createObject(
                    map.map,
                    {
                        "path":
                            coordinates
                    }
                )

            if (!newLine)
                return

            map.map.addMapItem(
                newLine
            )

            shipRoutes[
                shipId
            ] = {
                line:
                    newLine,

                pathArr:
                    coordinates,

                startLat:
                    startLat,

                startLon:
                    startLon,

                endLat:
                    endLat,

                endLon:
                    endLon
            }

            updateRouteVisibility()

            // Automatically show ongoing route for selected ship.
            if (
                shipId ===
                    firstPanelShipId ||
                shipId ===
                    secondPanelShipId
            ) {

                showCurrentNavigation(
                    shipId
                )
            }

            if (
                firstPanelOpen &&
                firstPanelShipId === shipId
            ) {

                reloadFirstPanel()

            }
            else if (
                secondPanelOpen &&
                secondPanelShipId === shipId
            ) {

                reloadSecondPanel()
            }
        }
        // ========================================================
        // A* PREVIEW PATH READY
        // ========================================================

        function onPreviewPathReady(
                shipId,
                path)
        {
            if (
                shipId !==
                destinationPreviewShipId
            ) {
                return
            }

            if (
                !path ||
                path.length < 2
            ) {
                clearDestinationPreview()

                appendLog(
                    "No A* route could be found to the selected destination."
                )

                return
            }

            var coordinates = []

            for (
                var i = 0;
                i < path.length;
                ++i
            ) {

                var latitude =
                    Number(
                        path[i].latitude
                    )

                var longitude =
                    Number(
                        path[i].longitude
                    )

                if (
                    !isFinite(latitude) ||
                    !isFinite(longitude)
                ) {
                    continue
                }

                var coordinate =
                    QtPositioning.coordinate(
                        latitude,
                        longitude
                    )

                if (
                    coordinate.isValid
                ) {
                    coordinates.push(
                        coordinate
                    )
                }
            }

            if (
                coordinates.length < 2
            ) {
                clearDestinationPreview()
                return
            }

            // ----------------------------------------------------
            // Create the actual A* preview line.
            // ----------------------------------------------------

            destinationPreviewLine =
                previewRouteComponent.createObject(
                    map.map
                )

            if (!destinationPreviewLine) {
                clearDestinationPreview()
                return
            }

            destinationPreviewLine.path =
                coordinates

            map.map.addMapItem(
                destinationPreviewLine
            )

            appendLog(
                "A* destination preview calculated for " +
                shipId
            )
        }
    }

    // ============================================================
    // HISTORICAL PLAYBACK TIMER
    //
    // This is the saved-route-to-saved-route playback.
    // ============================================================

    Timer {

        id:
            historicalPlaybackTimer

        interval:
            400

        repeat:
            false

        onTriggered:
            historicalPlaybackShowNext()
    }

    // ============================================================
    // GHOST MOVEMENT TIMER
    //
    // 50 ms gives reasonably smooth movement.
    // The movement distance is calculated from ship speed.
    // ============================================================

    Timer {

        id:
            ghostTimer

        interval:
            50

        repeat:
            true

        onTriggered: {

            updateGhostPosition(
                interval / 1000.0
            )
        }
    }

    // ============================================================
    // H3 TIMER
    // ============================================================

    Timer {

        id:
            h3RefreshTimer

        interval:
            500

        repeat:
            true

        running:
            showH3Grid

        onTriggered:
            showH3GridOnMap()
    }

    // ============================================================
    // H3 GRID VISUALIZATION
    //
    // Only the closest 300 cells to the current map center are
    // displayed. The timer refreshes them every 500 ms so the grid
    // follows the viewport while the user pans the map.
    // ============================================================

    function clearH3Grid() {

        for (
            var i = 0;
            i < activePolygons;
            ++i
        ) {

            if (h3PolygonPool[i]) {

                h3PolygonPool[i].visible = false
                h3PolygonPool[i].path = []
            }
        }

        activePolygons = 0
        showH3Grid = false
    }


    // ============================================================
    // SHOW H3 GRID AROUND CURRENT MAP CENTER
    // ============================================================

    function showH3GridOnMap() {

        if (!showH3Grid)
            return

        var center =
            map.map.center

        if (!center || !center.isValid)
            return

        // Only request the nearest 300 cells.
        var cells =
            navigation.h3DebugCellsNear(
                center.latitude,
                center.longitude,
                300
            )

        if (!cells)
            cells = []

        // ----------------------------------------------------------
        // Create/reuse polygon objects.
        // ----------------------------------------------------------

        while (
            h3PolygonPool.length <
            cells.length
        ) {

            var polygon =
                h3PolygonComponent.createObject(
                    map.map
                )

            if (!polygon)
                break

            map.map.addMapItem(
                polygon
            )

            h3PolygonPool.push(
                polygon
            )
        }

        // ----------------------------------------------------------
        // Hide polygons that are no longer needed.
        // ----------------------------------------------------------

        for (
            var i = cells.length;
            i < activePolygons;
            ++i
        ) {

            if (h3PolygonPool[i]) {

                h3PolygonPool[i].visible =
                    false

                h3PolygonPool[i].path =
                    []
            }
        }

        activePolygons =
            Math.min(
                cells.length,
                h3PolygonPool.length
            )

        // ----------------------------------------------------------
        // Update visible polygons.
        // ----------------------------------------------------------

        for (
            var j = 0;
            j < activePolygons;
            ++j
        ) {

            var cell =
                cells[j]

            if (
                cell &&
                cell.path &&
                cell.path.length >= 3
            ) {

                h3PolygonPool[j].path =
                    cell.path

                h3PolygonPool[j].visible =
                    true

            }
            else {

                h3PolygonPool[j].visible =
                    false

                h3PolygonPool[j].path =
                    []
            }
        }
    }


    // ============================================================
    // TOGGLE H3 GRID
    // ============================================================

    function toggleH3Grid() {

        if (showH3Grid) {

            clearH3Grid()

        }
        else {

            showH3Grid =
                true

            showH3GridOnMap()
        }
    }

    // ============================================================
    // NAVIGATION ROUTE COMPONENT
    // ============================================================

    Component {

        id:
            navigationRouteComponent

        MapPolyline {

            line.width:
                window.navigationLineWidth

            line.color:
                window.getNavigationLineColor()
        }
    }

    // ============================================================
    // HISTORICAL ROUTE COMPONENT
    // ============================================================

    Component {

        id:
            historicalRouteComponent

        MapPolyline {

            line.width:
                window.historicalLineWidth

            line.color:
                window.getHistoricalLineColor()
        }
    }

    // ============================================================
    // RIGHT-CLICK DESTINATION PREVIEW
    // ============================================================

    Component {

        id:
            previewRouteComponent

        MapPolyline {

            line.width:
                window.previewLineWidth

            line.color:
                window.getPreviewLineColor()
        }
    }

    // ============================================================
    // H3 POLYGON
    // ============================================================

    Component {

        id:
            h3PolygonComponent

        MapPolygon {

            visible:
                false

            color:
                "transparent"

            border.width:
                2

            border.color:
                "yellow"

            opacity:
                1.0
        }
    }

    // ============================================================
    // PLAYBACK / GHOST SHIP
    //
    // IMPORTANT:
    // shipType is now referenced directly from this object's
    // own property. No parent.parent chain.
    // ============================================================

    Component {

        id:
            ghostShipComponent

        MapQuickItem {

            id:
                ghostMarker

            property string shipType:
                "Civilian"

            z:
                740

            anchorPoint.x:
                ghostImage.width / 2

            anchorPoint.y:
                ghostImage.height / 2

            sourceItem:
                Image {

                    id:
                        ghostImage

                    width:
                        60

                    height:
                        30

                    fillMode:
                        Image.PreserveAspectFit

                    opacity:
                        0.40

                    source: {

                        switch (
                            ghostMarker.shipType
                        ) {

                        case "Destroyer":
                            return "qrc:/qt/qml/RadarDatabase/image/destroyer.png"

                        case "Battleship":
                            return "qrc:/qt/qml/RadarDatabase/image/destroyer.png"

                        case "Ironclad":
                            return "qrc:/qt/qml/RadarDatabase/image/frigate.png"

                        case "Submarine":
                            return "qrc:/qt/qml/RadarDatabase/image/submarine.png"

                        case "Battlecruiser":
                            return "qrc:/qt/qml/RadarDatabase/image/cruiser.png"

                        case "Civilian":
                            return "qrc:/qt/qml/RadarDatabase/image/civilian.png"

                        default:
                            return "qrc:/qt/qml/RadarDatabase/image/civilian.png"
                        }
                    }
                }
        }
    }

    // ============================================================
    // INITIAL LOAD
    // ============================================================

    Component.onCompleted:
        refreshShips(
            shipModel.getShipsForQml()
        )

    // ============================================================
    // REFRESH SHIPS
    // ============================================================

    function refreshShips(ships) {

        var currentIds =
            ({})

        for (
            var i = 0;
            i < ships.length;
            ++i
        ) {

            var ship =
                ships[i]

            var id =
                ship.id

            if (
                !id ||
                id === ""
            ) {
                continue
            }

            currentIds[id] =
                true

            var marker =
                shipMarkers[id]

            if (!marker) {

                marker =
                    shipMarkerComponent.createObject(
                        map.map,
                        {
                            "shipId":
                                id,

                            "shipName":
                                ship.name,

                            "shipFlag":
                                ship.flag,

                            "shipLatitude":
                                Number(
                                    ship.latitude
                                ),

                            "shipLongitude":
                                Number(
                                    ship.longitude
                                ),

                            "shipSpeed":
                                Number(
                                    ship.speed
                                ),

                            "shipCourse":
                                Number(
                                    ship.course
                                ),

                            "shipClass":
                                ship.shipClass,

                            "shipType":
                                ship.type,

                            "shipAffiliation":
                                ship.affiliation
                        }
                    )

                if (!marker)
                    continue

                map.map.addMapItem(
                    marker
                )

                shipMarkers[id] =
                    marker
            }

            marker.shipName =
                ship.name

            marker.shipFlag =
                ship.flag

            if (
                !navigatingShips[id] &&
                !marker.dragging &&
                !marker.positionLocked
            ) {

                marker.shipLatitude =
                    Number(
                        ship.latitude
                    )

                marker.shipLongitude =
                    Number(
                        ship.longitude
                    )
            }

            marker.shipSpeed =
                Number(
                    ship.speed
                )
            if (
                id === firstPanelShipId
            ) {
                selectedSpeed =
                    marker.shipSpeed
            }

            if (
                id === secondPanelShipId
            ) {
                secondSelectedSpeed =
                    marker.shipSpeed
            }

            marker.shipCourse =
                Number(
                    ship.course
                )
            if (
                            id === firstPanelShipId
                        ) {
                            selectedSpeed =
                                marker.shipSpeed

                            selectedCourse =        // <--- ADD THIS
                                marker.shipCourse   // <--- ADD THIS
                        }

                        if (
                            id === secondPanelShipId
                        ) {
                            secondSelectedSpeed =
                                marker.shipSpeed

                            secondSelectedCourse =  // <--- ADD THIS
                                marker.shipCourse   // <--- ADD THIS
                        }

            marker.shipClass =
                ship.shipClass

            marker.shipType =
                ship.type

            marker.shipAffiliation =
                ship.affiliation
        }

        var markerIds =
            Object.keys(
                shipMarkers
            )

        for (
            var j = 0;
            j < markerIds.length;
            ++j
        ) {

            var markerId =
                markerIds[j]

            if (
                !currentIds[
                    markerId
                ]
            ) {

                var markerToRemove =
                    shipMarkers[
                        markerId
                    ]

                if (markerToRemove) {

                    map.map.removeMapItem(
                        markerToRemove
                    )

                    markerToRemove.destroy()
                }

                delete shipMarkers[
                    markerId
                ]

                clearRouteForShip(
                    markerId
                )

                delete currentTravelPaths[
                    markerId
                ]
            }
        }
    }

    // ============================================================
    // MAP
    // ============================================================

    MapView {

        id:
            map

        anchors.fill:
            parent

        map.plugin:
            mapPlugin

        map.center:
            QtPositioning.coordinate(
                0,
                0
            )

        map.zoomLevel:
            2

        minimumZoomLevel:
            2

        maximumZoomLevel:
            18

        // ========================================================
        // MAP STYLE BUTTON
        // ========================================================

        Button {

            id:
                styleButton

            anchors {
                top:
                    parent.top

                left:
                    parent.left

                margins:
                    15
            }

            text:
                "Map Style"

            onClicked: {

                mapStyle =
                    (mapStyle + 1) % 3

                map.map.activeMapType =
                    map.map.supportedMapTypes[
                        mapStyle
                    ]
            }
        }

        // ========================================================
        // H3 BUTTON
        // ========================================================

        Button {

            id:
                h3Button

            anchors {
                top:
                    styleButton.bottom

                left:
                    parent.left

                margins:
                    15
            }

            text:
                showH3Grid
                ? "Hide H3 Grid"
                : "Show H3 Grid"

            onClicked:
                toggleH3Grid()
        }

        // ========================================================
        // ORIGINAL "SHOW ALL ROUTES"
        //
        // ONLY CURRENTLY ONGOING ROUTES.
        // ========================================================

        Button {

            id:
                toggleRoutesButton

            anchors {
                top:
                    h3Button.bottom

                left:
                    parent.left

                margins:
                    15
            }

            text:
                showAllRoutes
                ? "Hide All Routes"
                : "Show All Routes"

            onClicked: {

                showAllRoutes =
                    !showAllRoutes

                updateRouteVisibility()
            }
        }

        // ========================================================
        // PINCH
        // ========================================================

        PinchHandler {

            id:
                pinch

            target:
                null

            onScaleChanged:
                (delta) =>
            {
                map.zoomLevel +=
                    Math.log2(delta)
            }
        }

        // ========================================================
        // MAP CLICK AREA
        // ========================================================

        MouseArea {

            id:
                closePanelArea

            parent:
                map.map

            anchors.fill:
                parent

            z:
                500

            enabled:
                firstPanelOpen ||
                secondPanelOpen ||
                selectingNavigationDestination

            acceptedButtons:
                Qt.LeftButton |
                Qt.RightButton

            onClicked:
                function(mouse)
            {
                // =================================================
                // LEFT CLICK
                // =================================================

                if (
                    mouse.button ===
                    Qt.LeftButton
                ) {

                    firstPanelOpen =
                        false

                    secondPanelOpen =
                        false

                    firstClosePanel()
                    secondClosePanel()

                    firstPanelShipId =
                        ""

                    secondPanelShipId =
                        ""

                    navigationShipId =
                        ""

                    selectingNavigationDestination =
                        false

                    pendingNavigationLatitude =
                        NaN

                    pendingNavigationLongitude =
                        NaN

                    clearHistoricalVisualization()
                    clearDestinationPreview()

                    updateRouteVisibility()
                }

                // =================================================
                // RIGHT CLICK
                //
                // ONLY SELECTS DESTINATION.
                //
                // START BUTTON actually calls startNavigation().
                // =================================================

                if (
                    mouse.button ===
                    Qt.RightButton
                ) {

                    var coordinate =
                        map.map.toCoordinate(
                            Qt.point(
                                mouse.x,
                                mouse.y
                            )
                        )

                    if (
                        !coordinate.isValid
                    ) {
                        return
                    }

                    var shipId =
                        firstPanelOpen
                        ? firstPanelShipId
                        : secondPanelOpen
                          ? secondPanelShipId
                          : ""

                    if (
                        shipId === ""
                    ) {
                        return
                    }

                    // ----------------------------------------------------------
                    // Save selected destination.
                    // ----------------------------------------------------------

                    navigationShipId =
                        shipId

                    pendingNavigationLatitude =
                        coordinate.latitude

                    pendingNavigationLongitude =
                        coordinate.longitude

                    selectingNavigationDestination =
                        true

                    // ----------------------------------------------------------
                    // Calculate the real A* route.
                    //
                    // IMPORTANT:
                    // This only previews the route.
                    // It does NOT start navigation.
                    // ----------------------------------------------------------

                    requestDestinationPreview(
                        shipId,
                        coordinate.latitude,
                        coordinate.longitude
                    )

                    appendLog(
                        "Calculating A* destination preview for " +
                        shipId +
                        ": " +
                        coordinate.latitude.toFixed(6) +
                        " " +
                        coordinate.longitude.toFixed(6)
                    )
                }
            }
        }

        // ========================================================
        // SHIP MARKER
        // ========================================================

        Component {

            id:
                shipMarkerComponent

            MapQuickItem {

                id:
                    shipMarker

                property string shipId:
                    ""

                property string shipName:
                    ""

                property string shipFlag:
                    ""

                property real shipLatitude:
                    0

                property real shipLongitude:
                    0

                property bool dragging:
                    false

                property bool positionLocked:
                    false

                property real dragStartLatitude:
                    0

                property real dragStartLongitude:
                    0

                property int shipSpeed:
                    0

                property int shipCourse:
                    0

                property string shipClass:
                    ""

                property string shipType:
                    ""

                property string shipAffiliation:
                    ""

                coordinate:
                    QtPositioning.coordinate(
                        shipLatitude,
                        shipLongitude
                    )

                visible:
                    true

                z:
                    750

                anchorPoint.x:
                    marker.width / 2

                anchorPoint.y:
                    marker.height / 2

                sourceItem:
                    Image {

                        id:
                            marker

                        width:
                            60

                        height:
                            30

                        fillMode:
                            Image.PreserveAspectFit

                        source: {

                            switch (
                                shipMarker.shipType
                            ) {

                            case "Destroyer":
                                return "qrc:/qt/qml/RadarDatabase/image/destroyer.png"

                            case "Battleship":
                                return "qrc:/qt/qml/RadarDatabase/image/destroyer.png"

                            case "Ironclad":
                                return "qrc:/qt/qml/RadarDatabase/image/frigate.png"

                            case "Submarine":
                                return "qrc:/qt/qml/RadarDatabase/image/submarine.png"

                            case "Battlecruiser":
                                return "qrc:/qt/qml/RadarDatabase/image/cruiser.png"

                            case "Civilian":
                                return "qrc:/qt/qml/RadarDatabase/image/civilian.png"

                            default:
                                return "qrc:/qt/qml/RadarDatabase/image/civilian.png"
                            }
                        }

                        MouseArea {

                            anchors.fill:
                                parent

                            preventStealing:
                                true

                            // ------------------------------------------------
                            // CLICK
                            //
                            // Navigation does NOT start here.
                            // ------------------------------------------------

                            onClicked:
                                function(mouse)
                            {
                                // Intentionally empty.
                                //
                                // Right-click map selects a destination,
                                // Start button starts navigation.
                            }

                            // ------------------------------------------------
                            // PRESSED
                            // ------------------------------------------------

                            onPressed:
                                function(mouse)
                            {
                                shipMarker.dragging =
                                    true

                                shipMarker.positionLocked =
                                    true

                                shipMarker.dragStartLatitude =
                                    shipMarker.shipLatitude

                                shipMarker.dragStartLongitude =
                                    shipMarker.shipLongitude

                                // ==========================================
                                // FIRST PANEL
                                // ==========================================

                                if (
                                    !firstPanelOpen &&
                                    !secondPanelOpen
                                ) {

                                    selectFirstShip(
                                        shipMarker
                                    )
                                }

                                // ==========================================
                                // SECOND PANEL
                                // ==========================================

                                else if (
                                    firstPanelOpen
                                ) {

                                    selectSecondShip(
                                        shipMarker
                                    )
                                }

                                // ==========================================
                                // SWITCH BACK TO FIRST PANEL
                                // ==========================================

                                else if (
                                    secondPanelOpen
                                ) {

                                    selectFirstShip(
                                        shipMarker
                                    )

                                    secondClosePanel()

                                    firstOpenPanel()

                                    firstPanelOpen =
                                        true

                                    secondPanelOpen =
                                        false
                                }

                                updateRouteVisibility()
                            }

                            // ------------------------------------------------
                            // DRAGGING
                            // ------------------------------------------------

                            onPositionChanged:
                                function(mouse)
                            {
                                if (
                                    !shipMarker.dragging
                                ) {
                                    return
                                }

                                var mapPoint =
                                    map.mapFromItem(
                                        marker,
                                        mouse.x,
                                        mouse.y
                                    )

                                var coordinate =
                                    map.map.toCoordinate(
                                        mapPoint
                                    )

                                if (
                                    !coordinate.isValid ||
                                    !isFinite(
                                        coordinate.latitude
                                    ) ||
                                    !isFinite(
                                        coordinate.longitude
                                    )
                                ) {
                                    return
                                }

                                shipMarker.shipLatitude =
                                    coordinate.latitude

                                shipMarker.shipLongitude =
                                    coordinate.longitude

                                shipModel.setShipLocation(
                                    shipMarker.shipId,
                                    coordinate.latitude,
                                    coordinate.longitude
                                )

                                if (
                                    secondPanelOpen &&
                                    secondPanelShipId ===
                                        shipMarker.shipId
                                ) {

                                    secondSelectedLatitude =
                                        coordinate.latitude

                                    secondSelectedLongitude =
                                        coordinate.longitude

                                }
                                else if (
                                    firstPanelOpen &&
                                    firstPanelShipId ===
                                        shipMarker.shipId
                                ) {

                                    selectedLatitude =
                                        coordinate.latitude

                                    selectedLongitude =
                                        coordinate.longitude
                                }
                            }

                            // ------------------------------------------------
                            // RELEASE
                            // ------------------------------------------------

                            onReleased:
                                function(mouse)
                            {
                                if (
                                    !shipMarker.dragging
                                ) {
                                    return
                                }

                                var droppedIsWater =
                                    navigation.isWater(
                                        shipMarker.shipLatitude,
                                        shipMarker.shipLongitude
                                    )

                                if (
                                    !droppedIsWater
                                ) {

                                    shipMarker.shipLatitude =
                                        shipMarker.dragStartLatitude

                                    shipMarker.shipLongitude =
                                        shipMarker.dragStartLongitude

                                    shipModel.setShipLocation(
                                        shipMarker.shipId,
                                        shipMarker.dragStartLatitude,
                                        shipMarker.dragStartLongitude
                                    )

                                }
                                else {

                                    shipModel.setShipLocation(
                                        shipMarker.shipId,
                                        shipMarker.shipLatitude,
                                        shipMarker.shipLongitude,
                                        shipMarker.shipCourse
                                    )
                                }

                                shipModel.saveShipLocation(
                                    shipMarker.shipId
                                )

                                shipMarker.dragging =
                                    false

                                shipMarker.positionLocked =
                                    false
                            }
                        }
                    }
            }
        }

        // ============================================================
        // FIRST INFORMATION PANEL
        // ============================================================

        Rectangle {

            id:
                firstInformationPanel

            width:
                470

            height:
                570

            x:
                parent.width -
                width -
                20

            y:
                parent.height

            radius:
                12

            color:
                "#262626"

            z:
                1000

            ListModel {
                id:
                    firstRouteModel
            }

            NumberAnimation {

                id:
                    firstPanelAnimation

                target:
                    firstInformationPanel

                property:
                    "y"

                duration:
                    350

                easing.type:
                    Easing.OutCubic
            }

            Column {

                anchors.fill:
                    parent

                anchors.margins:
                    20

                spacing:
                    0

                // ====================================================
                // NAME / FLAG
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        50

                    Text {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            selectedName

                        color:
                            "white"

                        font.pixelSize:
                            22

                        font.bold:
                            true
                    }

                    Text {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            selectedFlag

                        color:
                            "white"

                        font.pixelSize:
                            22
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // LATITUDE / LONGITUDE
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        70

                    Text {

                        anchors.left:
                            parent.left

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            8

                        text:
                            "Latitude"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.left:
                            parent.left

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            8

                        text:
                            Number(
                                selectedLatitude
                            ).toFixed(6)

                        color:
                            "white"

                        font.pixelSize:
                            19
                    }

                    Rectangle {

                        width:
                            1

                        height:
                            parent.height - 12

                        anchors.centerIn:
                            parent

                        color:
                            "#555555"
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            8

                        text:
                            "Longitude"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            8

                        text:
                            Number(
                                selectedLongitude
                            ).toFixed(6)

                        color:
                            "white"

                        font.pixelSize:
                            19
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // HISTORICAL ROUTE COMBOBOX
                // ====================================================

                ComboBox {

                    id:
                        firstCombo

                    width:
                        parent.width

                    height:
                        46

                    model:
                        firstRouteModel

                    textRole:
                        "display"

                    font.pixelSize:
                        14

                    delegate:
                        ItemDelegate {

                            width:
                                firstCombo.popup.width

                            height:
                                38

                            text:
                                model.display

                            font.pixelSize:
                                14
                        }

                    popup.width:
                        firstCombo.width

                    // 10 visible rows.
                    popup.height:
                        Math.min(
                            10 * 38 + 4,
                            Math.max(
                                42,
                                firstRouteModel.count * 38 + 4
                            )
                        )
                }

                Item {

                    width:
                        1

                    height:
                        6
                }

                // ====================================================
                // SHOW / SHOW ALL / PLAYBACK
                // ====================================================

                Row {

                    width:
                        parent.width

                    height:
                        38

                    spacing:
                        6

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            "Show"

                        enabled:
                            firstCombo.currentIndex >= 0

                        onClicked: {

                            if (
                                firstCombo.currentIndex < 0 ||
                                firstCombo.currentIndex >=
                                    firstPanelHistoryData.length
                            ) {
                                return
                            }

                            var route =
                                firstPanelHistoryData[
                                    firstCombo.currentIndex
                                ]

                            showHistoricalRoute(
                                route.path
                            )
                        }
                    }

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            "Show All"

                        enabled:
                            firstPanelHistoryData.length > 0

                        onClicked:
                            showAllHistoricalRoutes(
                                firstPanelHistoryData
                            )
                    }

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            ghostRunning
                            ? "Playback..."
                            : "Playback"

                        enabled:
                            firstCombo.currentIndex >= 0 &&
                            firstPanelHistoryData.length > 0 &&
                            !ghostRunning

                        onClicked: {

                            var route =
                                firstPanelHistoryData[
                                    firstCombo.currentIndex
                                ]

                            startGhostRoute(
                                route,
                                firstPanelShipId,
                                firstSpeedField.text
                            )
                        }
                    }
                }

                Item {

                    width:
                        1

                    height:
                        6
                }

                // ====================================================
                // PLAY ALL ROUTES OLDEST -> NEWEST
                // ====================================================

                Button {

                    width:
                        parent.width

                    height:
                        38

                    text:
                        historicalPlaybackRunning
                        ? "Playing Historical Routes..."
                        : "Play Routes Older → Newer"

                    enabled:
                        firstPanelHistoryData.length > 0 &&
                        !historicalPlaybackRunning

                    onClicked:
                        playHistoricalRoutes(
                            firstPanelHistoryData
                        )
                }

                Item {

                    width:
                        1

                    height:
                        8
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // SPEED / START / STOP / COURSE
                //
                // Course is on the RIGHT of speed.
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        65

                    Row {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        spacing:
                            6

                        Column {

                            width:
                                78

                            spacing:
                                2

                            Text {

                                text:
                                    "Speed"

                                color:
                                    "#999999"

                                font.pixelSize:
                                    13
                            }

                            TextField {

                                id:
                                    firstSpeedField

                                width:
                                    78

                                height:
                                    32

                                text:
                                    Number(
                                        selectedSpeed
                                    ).toString()

                                horizontalAlignment:
                                    Text.AlignHCenter

                                inputMethodHints:
                                    Qt.ImhFormattedNumbersOnly

                                validator:
                                    DoubleValidator {

                                        bottom:
                                            0

                                        top:
                                            1000

                                        decimals:
                                            2
                                    }

                                onEditingFinished: {

                                    var value =
                                        Number(
                                            text
                                        )

                                    if (
                                        isFinite(value) &&
                                        value >= 0
                                    ) {

                                        selectedSpeed =
                                            value
                                    }
                                }
                            }
                        }

                        Button {

                            width:
                                62

                            height:
                                32

                            anchors.verticalCenter:
                                parent.verticalCenter

                            text:
                                "Start"

                            enabled:
                                firstPanelShipId !== "" &&
                                isFinite(
                                    pendingNavigationLatitude
                                ) &&
                                isFinite(
                                    pendingNavigationLongitude
                                ) &&
                                !navigatingShips[
                                    firstPanelShipId
                                ]

                            onClicked: {

                                var ship =
                                    findShipById(
                                        firstPanelShipId
                                    )

                                if (!ship)
                                    return

                                var speed =
                                    Number(
                                        firstSpeedField.text
                                    )

                                if (
                                    !isFinite(speed) ||
                                    speed < 0
                                ) {
                                    return
                                }

                                var databaseSpeed =
                                    Math.round(speed)

                                /*
                                 * Save the edited speed to:
                                 *
                                 * 1. ShipModel
                                 * 2. MongoDB
                                 *
                                 * before starting navigation.
                                 */
                                if (
                                    !shipModel.setShipSpeed(
                                        firstPanelShipId,
                                        databaseSpeed
                                    )
                                ) {
                                    appendLog(
                                        ship.shipName +
                                        " speed could not be updated."
                                    )

                                    return
                                }

                                /*
                                 * Keep QML values synchronized immediately.
                                 */
                                selectedSpeed =
                                    databaseSpeed

                                ship.shipSpeed =
                                    databaseSpeed

                                clearDestinationPreview()

                                navigation.startNavigation(
                                    firstPanelShipId,

                                    ship.shipLatitude,
                                    ship.shipLongitude,

                                    pendingNavigationLatitude,
                                    pendingNavigationLongitude,

                                    databaseSpeed
                                )

                                navigatingShips[
                                    firstPanelShipId
                                ] = true

                                appendLog(
                                    ship.shipName +
                                    " navigation started."
                                )

                                pendingNavigationLatitude =
                                    NaN

                                pendingNavigationLongitude =
                                    NaN

                                navigationShipId =
                                    ""

                                selectingNavigationDestination =
                                    false
                            }
                        }

                        Button {

                            width:
                                62

                            height:
                                32

                            anchors.verticalCenter:
                                parent.verticalCenter

                            text:
                                "Stop"

                            enabled:
                                firstPanelShipId !== "" &&
                                !!navigatingShips[
                                    firstPanelShipId
                                ]

                            onClicked: {

                                navigation.stopNavigation(
                                    firstPanelShipId
                                )

                            }
                        }

                    }

                    // ------------------------------------------------
                    // COURSE
                    // ------------------------------------------------

                    Column {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        width:
                            80

                        spacing:
                            2

                        Text {

                            width:
                                parent.width

                            text:
                                "Course"

                            color:
                                "#999999"

                            font.pixelSize:
                                13

                            horizontalAlignment:
                                Text.AlignRight
                        }

                        Text {

                            width:
                                parent.width

                            text:
                                selectedCourse

                            color:
                                "white"

                            font.pixelSize:
                                19

                            horizontalAlignment:
                                Text.AlignRight
                        }
                    }
                }

                Text {

                    width:
                        parent.width

                    text:
                        selectingNavigationDestination &&
                        navigationShipId ===
                            firstPanelShipId
                        ? "Destination: " +
                          Number(
                              pendingNavigationLatitude
                          ).toFixed(6) +
                          ", " +
                          Number(
                              pendingNavigationLongitude
                          ).toFixed(6)
                        : "Right-click the map to choose a navigation destination."

                    color:
                        selectingNavigationDestination
                        ? "#60a5fa"
                        : "#777777"

                    font.pixelSize:
                        11

                    elide:
                        Text.ElideRight
                }

                Item {

                    width:
                        1

                    height:
                        5
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // CLASS / TYPE
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        58

                    Text {

                        anchors.left:
                            parent.left

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            6

                        text:
                            "Class"

                        color:
                            "#999999"

                        font.pixelSize:
                            13
                    }

                    Text {

                        anchors.left:
                            parent.left

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            7

                        text:
                            selectedClass

                        color:
                            "white"

                        font.pixelSize:
                            18
                    }

                    Rectangle {

                        width:
                            1

                        height:
                            parent.height - 10

                        anchors.centerIn:
                            parent

                        color:
                            "#555555"
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            6

                        text:
                            "Type"

                        color:
                            "#999999"

                        font.pixelSize:
                            13
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            7

                        text:
                            selectedType

                        color:
                            "white"

                        font.pixelSize:
                            18
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // AFFILIATION
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        55

                    Text {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            "Affiliation"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            selectedAffiliation

                        color:
                            "white"

                        font.pixelSize:
                            18

                        font.bold:
                            true
                    }
                }
            }
        }

        // ============================================================
        // SECOND INFORMATION PANEL
        // ============================================================

        Rectangle {

            id:
                secondInformationPanel

            width:
                470

            height:
                570

            x:
                parent.width -
                width -
                20

            y:
                parent.height

            radius:
                12

            color:
                "#262626"

            z:
                1000

            ListModel {
                id:
                    secondRouteModel
            }

            NumberAnimation {

                id:
                    secondPanelAnimation

                target:
                    secondInformationPanel

                property:
                    "y"

                duration:
                    350

                easing.type:
                    Easing.OutCubic
            }

            Column {

                anchors.fill:
                    parent

                anchors.margins:
                    20

                spacing:
                    0

                // ====================================================
                // NAME / FLAG
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        50

                    Text {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            secondSelectedName

                        color:
                            "white"

                        font.pixelSize:
                            22

                        font.bold:
                            true
                    }

                    Text {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            secondSelectedFlag

                        color:
                            "white"

                        font.pixelSize:
                            22
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // LATITUDE / LONGITUDE
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        70

                    Text {

                        anchors.left:
                            parent.left

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            8

                        text:
                            "Latitude"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.left:
                            parent.left

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            8

                        text:
                            Number(
                                secondSelectedLatitude
                            ).toFixed(6)

                        color:
                            "white"

                        font.pixelSize:
                            19
                    }

                    Rectangle {

                        width:
                            1

                        height:
                            parent.height - 12

                        anchors.centerIn:
                            parent

                        color:
                            "#555555"
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            8

                        text:
                            "Longitude"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            8

                        text:
                            Number(
                                secondSelectedLongitude
                            ).toFixed(6)

                        color:
                            "white"

                        font.pixelSize:
                            19
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // COMBOBOX
                // ====================================================

                ComboBox {

                    id:
                        secondCombo

                    width:
                        parent.width

                    height:
                        46

                    model:
                        secondRouteModel

                    textRole:
                        "display"

                    font.pixelSize:
                        14

                    delegate:
                        ItemDelegate {

                            width:
                                secondCombo.popup.width

                            height:
                                38

                            text:
                                model.display

                            font.pixelSize:
                                14
                        }

                    popup.width:
                        secondCombo.width

                    popup.height:
                        Math.min(
                            10 * 38 + 4,
                            Math.max(
                                42,
                                secondRouteModel.count * 38 + 4
                            )
                        )
                }

                Item {

                    width:
                        1

                    height:
                        6
                }

                // ====================================================
                // SHOW / SHOW ALL / PLAYBACK
                // ====================================================

                Row {

                    width:
                        parent.width

                    height:
                        38

                    spacing:
                        6

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            "Show"

                        enabled:
                            secondCombo.currentIndex >= 0

                        onClicked: {

                            if (
                                secondCombo.currentIndex < 0 ||
                                secondCombo.currentIndex >=
                                    secondPanelHistoryData.length
                            ) {
                                return
                            }

                            var route =
                                secondPanelHistoryData[
                                    secondCombo.currentIndex
                                ]

                            showHistoricalRoute(
                                route.path
                            )
                        }
                    }

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            "Show All"

                        enabled:
                            secondPanelHistoryData.length > 0

                        onClicked:
                            showAllHistoricalRoutes(
                                secondPanelHistoryData
                            )
                    }

                    Button {

                        width:
                            (parent.width - 12) / 3

                        height:
                            parent.height

                        text:
                            ghostRunning
                            ? "Playback..."
                            : "Playback"

                        enabled:
                            secondCombo.currentIndex >= 0 &&
                            secondPanelHistoryData.length > 0 &&
                            !ghostRunning

                        onClicked: {

                            var route =
                                secondPanelHistoryData[
                                    secondCombo.currentIndex
                                ]

                            startGhostRoute(
                                route,
                                secondPanelShipId,
                                secondSpeedField.text
                            )
                        }
                    }
                }

                Item {

                    width:
                        1

                    height:
                        6
                }

                // ====================================================
                // PLAY ROUTES OLDEST -> NEWEST
                // ====================================================

                Button {

                    width:
                        parent.width

                    height:
                        38

                    text:
                        historicalPlaybackRunning
                        ? "Playing Historical Routes..."
                        : "Play Routes Older → Newer"

                    enabled:
                        secondPanelHistoryData.length > 0 &&
                        !historicalPlaybackRunning

                    onClicked:
                        playHistoricalRoutes(
                            secondPanelHistoryData
                        )
                }

                Item {

                    width:
                        1

                    height:
                        8
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // SPEED / START / STOP / COURSE
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        65

                    Row {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        spacing:
                            6

                        Column {

                            width:
                                78

                            spacing:
                                2

                            Text {

                                text:
                                    "Speed"

                                color:
                                    "#999999"

                                font.pixelSize:
                                    13
                            }

                            TextField {

                                id:
                                    secondSpeedField

                                width:
                                    78

                                height:
                                    32

                                text:
                                    Number(
                                        secondSelectedSpeed
                                    ).toString()

                                horizontalAlignment:
                                    Text.AlignHCenter

                                inputMethodHints:
                                    Qt.ImhFormattedNumbersOnly

                                validator:
                                    DoubleValidator {

                                        bottom:
                                            0

                                        top:
                                            1000

                                        decimals:
                                            2
                                    }

                                onEditingFinished: {

                                    var value =
                                        Number(
                                            text
                                        )

                                    if (
                                        isFinite(value) &&
                                        value >= 0
                                    ) {

                                        secondSelectedSpeed =
                                            value
                                    }
                                }
                            }
                        }

                        Button {

                            width:
                                62

                            height:
                                32

                            anchors.verticalCenter:
                                parent.verticalCenter

                            text:
                                "Start"

                            enabled:
                                secondPanelShipId !== "" &&
                                isFinite(
                                    pendingNavigationLatitude
                                ) &&
                                isFinite(
                                    pendingNavigationLongitude
                                ) &&
                                !navigatingShips[
                                    secondPanelShipId
                                ]

                            onClicked: {

                                var ship =
                                    findShipById(
                                        secondPanelShipId
                                    )

                                if (!ship)
                                    return

                                var speed =
                                    Number(
                                        secondSpeedField.text
                                    )

                                if (
                                    !isFinite(speed) ||
                                    speed < 0
                                ) {
                                    return
                                }

                                var databaseSpeed =
                                    Math.round(speed)

                                /*
                                 * Save the edited speed to:
                                 *
                                 * 1. ShipModel
                                 * 2. MongoDB
                                 */
                                if (
                                    !shipModel.setShipSpeed(
                                        secondPanelShipId,
                                        databaseSpeed
                                    )
                                ) {
                                    appendLog(
                                        ship.shipName +
                                        " speed could not be updated."
                                    )

                                    return
                                }

                                /*
                                 * Keep QML synchronized immediately.
                                 */
                                secondSelectedSpeed =
                                    databaseSpeed

                                ship.shipSpeed =
                                    databaseSpeed

                                clearDestinationPreview()

                                navigation.startNavigation(
                                    secondPanelShipId,

                                    ship.shipLatitude,
                                    ship.shipLongitude,

                                    pendingNavigationLatitude,
                                    pendingNavigationLongitude,

                                    databaseSpeed
                                )

                                navigatingShips[
                                    secondPanelShipId
                                ] = true

                                appendLog(
                                    ship.shipName +
                                    " navigation started."
                                )

                                pendingNavigationLatitude =
                                    NaN

                                pendingNavigationLongitude =
                                    NaN

                                navigationShipId =
                                    ""

                                selectingNavigationDestination =
                                    false
                            }
                        }

                        Button {

                            width:
                                62

                            height:
                                32

                            anchors.verticalCenter:
                                parent.verticalCenter

                            text:
                                "Stop"

                            enabled:
                                secondPanelShipId !== "" &&
                                !!navigatingShips[
                                    secondPanelShipId
                                ]

                            onClicked: {

                                navigation.stopNavigation(
                                    secondPanelShipId
                                )

                            }
                        }
                    }

                    Column {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        width:
                            80

                        spacing:
                            2

                        Text {

                            width:
                                parent.width

                            text:
                                "Course"

                            color:
                                "#999999"

                            font.pixelSize:
                                13

                            horizontalAlignment:
                                Text.AlignRight
                        }

                        Text {

                            width:
                                parent.width

                            text:
                                secondSelectedCourse

                            color:
                                "white"

                            font.pixelSize:
                                19

                            horizontalAlignment:
                                Text.AlignRight
                        }
                    }
                }

                Text {

                    width:
                        parent.width

                    text:
                        selectingNavigationDestination &&
                        navigationShipId ===
                            secondPanelShipId
                        ? "Destination: " +
                          Number(
                              pendingNavigationLatitude
                          ).toFixed(6) +
                          ", " +
                          Number(
                              pendingNavigationLongitude
                          ).toFixed(6)
                        : "Right-click the map to choose a navigation destination."

                    color:
                        selectingNavigationDestination
                        ? "#60a5fa"
                        : "#777777"

                    font.pixelSize:
                        11

                    elide:
                        Text.ElideRight
                }

                Item {

                    width:
                        1

                    height:
                        5
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // CLASS / TYPE
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        58

                    Text {

                        anchors.left:
                            parent.left

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            6

                        text:
                            "Class"

                        color:
                            "#999999"

                        font.pixelSize:
                            13
                    }

                    Text {

                        anchors.left:
                            parent.left

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            7

                        text:
                            secondSelectedClass

                        color:
                            "white"

                        font.pixelSize:
                            18
                    }

                    Rectangle {

                        width:
                            1

                        height:
                            parent.height - 10

                        anchors.centerIn:
                            parent

                        color:
                            "#555555"
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.top:
                            parent.top

                        anchors.topMargin:
                            6

                        text:
                            "Type"

                        color:
                            "#999999"

                        font.pixelSize:
                            13
                    }

                    Text {

                        anchors.left:
                            parent.horizontalCenter

                        anchors.leftMargin:
                            20

                        anchors.bottom:
                            parent.bottom

                        anchors.bottomMargin:
                            7

                        text:
                            secondSelectedType

                        color:
                            "white"

                        font.pixelSize:
                            18
                    }
                }

                Rectangle {

                    width:
                        parent.width

                    height:
                        1

                    color:
                        "#555555"
                }

                // ====================================================
                // AFFILIATION
                // ====================================================

                Item {

                    width:
                        parent.width

                    height:
                        55

                    Text {

                        anchors.left:
                            parent.left

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            "Affiliation"

                        color:
                            "#999999"

                        font.pixelSize:
                            14
                    }

                    Text {

                        anchors.right:
                            parent.right

                        anchors.verticalCenter:
                            parent.verticalCenter

                        text:
                            secondSelectedAffiliation

                        color:
                            "white"

                        font.pixelSize:
                            18

                        font.bold:
                            true
                    }
                }
            }
        }
    }

    // ============================================================
    // TERMINAL
    // ============================================================

    Rectangle {

        id:
            terminalPanel

        width:
            Math.min(
                parent.width * 0.6,
                800
            )

        height:
            250

        x:
            (parent.width - width) / 2

        y:
            window.height

        color:
            window.dark

        z:
            1100

        radius:
            8

        NumberAnimation {

            id:
                terminalAnimation

            target:
                terminalPanel

            property:
                "y"

            duration:
                350

            easing.type:
                Easing.OutCubic
        }

        Rectangle {

            width:
                70

            height:
                30

            anchors.bottom:
                parent.top

            anchors.horizontalCenter:
                parent.horizontalCenter

            color:
                window.dark

            radius:
                8

            Rectangle {

                width:
                    parent.width

                height:
                    10

                anchors.bottom:
                    parent.bottom

                color:
                    parent.color
            }

            Text {

                anchors.centerIn:
                    parent

                text:
                    terminalOpen
                    ? "▼"
                    : "▲"

                color:
                    "white"

                font.pixelSize:
                    16
            }

            MouseArea {

                anchors.fill:
                    parent

                onClicked:
                    toggleTerminal()
            }
        }

        ListView {

            id:
                logList

            anchors.fill:
                parent

            anchors.margins:
                15

            model:
                logModel

            clip:
                true

            spacing:
                6

            onCountChanged:
                positionViewAtEnd()

            delegate:
                Row {

                    spacing:
                        12

                    Text {

                        text:
                            "[" +
                            model.time +
                            "]"

                        color:
                            "#888888"

                        font.pixelSize:
                            13

                        font.family:
                            "monospace"
                    }

                    Text {

                        text:
                            model.message

                        color:
                            "#4ade80"

                        font.pixelSize:
                            13

                        font.family:
                            "monospace"

                        wrapMode:
                            Text.Wrap

                        width:
                            logList.width - 100
                    }
                }
        }
    }
}
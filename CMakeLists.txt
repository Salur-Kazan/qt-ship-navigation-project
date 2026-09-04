cmake_minimum_required(VERSION 3.16)

project(RadarDatabase VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH
    "/home/aysat/Qt/6.11.1/gcc_64"
    "/home/aysat/maplibre-qt-install"
    "/home/aysat/vcpkg/installed/x64-linux"
    "/home/aysat/h3-install"
    ${CMAKE_PREFIX_PATH}
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Location Positioning)
find_package(QMapLibre COMPONENTS Location REQUIRED)

find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)
find_package(PkgConfig REQUIRED)

find_package(h3 CONFIG REQUIRED)

pkg_check_modules(GDAL REQUIRED IMPORTED_TARGET gdal)

qt_standard_project_setup(REQUIRES 6.10)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appRadarDatabase
    main.cpp
)

add_custom_command(
    TARGET appRadarDatabase
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory
    "${CMAKE_SOURCE_DIR}/navigation_data"
    "$<TARGET_FILE_DIR:appRadarDatabase>/navigation_data"
)

qt_add_qml_module(appRadarDatabase
    URI RadarDatabase
    QML_FILES
        Main.qml
        SOURCES worker.h worker.cpp
        SOURCES database.h database.cpp
        SOURCES shipmodel.h shipmodel.cpp
        SOURCES navigation.h navigation.cpp
    RESOURCES
        image/submarine.png
        image/cruiser.png
        image/destroyer.png
        image/frigate.png
        image/civilian.png
        image/carrier.png
)

set_target_properties(appRadarDatabase PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appRadarDatabase
    PRIVATE
        Qt6::Quick
        Qt6::Location
        Qt6::Positioning
        QMapLibre::Location
        mongo::mongocxx_static
        mongo::bsoncxx_static
        PkgConfig::GDAL
        h3::h3
)

qmaplibre_location_setup_plugins(appRadarDatabase)

install(TARGETS appRadarDatabase
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
cmake_minimum_required(VERSION 3.19)
project(DroneGroundStation LANGUAGES CXX C)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg)

qt_standard_project_setup()

qt_add_executable(DroneGroundStation
    WIN32 MACOSX_BUNDLE
    assets/res.qrc
    main.cpp
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    db/sqlite3.c db/sqlite3.h
    map/mapcore.cpp map/mapcore.h map/mapwidget.cpp map/mapwidget.h
    map/drawinglayer.cpp map/drawinglayer.h map/maplayer.h
    backend/settings/settings.h backend/settings/settings.cpp
    ui/windows/overlay/overlaylayer.cpp ui/windows/overlay/overlaylayer.h ui/windows/overlay/overlaywidget.cpp ui/windows/overlay/overlaywidget.h
    ui/windows/overlay/overlaycontainer.h ui/windows/overlay/overlaycontainer.cpp
    ui/modules/leftMenu/leftmenu.cpp ui/modules/leftMenu/leftmenu.h
    backend/utilities/utilities.cpp backend/utilities/utilities.h
    ui/components/styleconstants.h
    ui/modules/coordinatesPanel/coordinatespanel.h ui/modules/coordinatesPanel/coordinatespanel.cpp ui/modules/coordinatesPanel/coordinatespanel.ui
)

target_include_directories(DroneGroundStation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/map
        ${CMAKE_CURRENT_SOURCE_DIR}/db
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_link_libraries(DroneGroundStation
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Svg
)

# add_library(sqlite3 STATIC db/sqlite3.c)
# target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/db)

# target_link_libraries(MyApp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets sqlite3)

include(GNUInstallDirs)

install(TARGETS DroneGroundStation
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET DroneGroundStation
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

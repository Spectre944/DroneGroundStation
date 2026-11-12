cmake_minimum_required(VERSION 3.19)
project(DroneGroundStation LANGUAGES CXX C)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Core5Compat)

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
    backend/coords/coordhandler.cpp backend/coords/coordhandler.h backend/coords/mgrs.cpp backend/coords/mgrs.h backend/coords/polarst.cpp backend/coords/polarst.h backend/coords/tranmerc.cpp backend/coords/tranmerc.h backend/coords/ups.cpp backend/coords/ups.h backend/coords/utm.cpp backend/coords/utm.h
    ui/modules/notifier/notifier.cpp ui/modules/notifier/notifier.h
    ui/components/custom/custom.cpp ui/components/custom/custom.h
    ui/pages/database/databasepage.cpp ui/pages/database/databasepage.h ui/pages/database/databasepage.ui ui/pages/device/devicepage.cpp ui/pages/device/devicepage.h ui/pages/device/devicepage.ui ui/pages/recon/reconpage.cpp ui/pages/recon/reconpage.h ui/pages/recon/reconpage.ui ui/pages/settings/settingspage.cpp ui/pages/settings/settingspage.h ui/pages/settings/settingspage.ui
    ui/modules/tables/tabledata.cpp ui/modules/tables/tabledata.h ui/modules/tables/tabledata.ui
    backend/tables/tablemodel.cpp backend/tables/tablemodel.h
    ui/components/dialog/dialog.cpp ui/components/dialog/dialog.h
    ui/components/lineedit/lineedit.cpp ui/components/lineedit/lineedit.h ui/components/textedit/textedit.cpp ui/components/textedit/textedit.h
    ui/modules/settingsfield/settingsdeviceitem.cpp ui/modules/settingsfield/settingsdeviceitem.h ui/modules/settingsfield/settingsdeviceitem.ui ui/modules/settingsfield/settingsfield.cpp ui/modules/settingsfield/settingsfield.h ui/modules/settingsfield/settingsfield.ui
    ui/components/combobox/combobox.cpp ui/components/combobox/combobox.h ui/components/spinbox/spinbox.cpp ui/components/spinbox/spinbox.h
    ui/modules/searchBar/searchbar.cpp ui/modules/searchBar/searchbar.h ui/modules/searchBar/searchbar.ui
    ui/components/button/button.cpp ui/components/button/button.h ui/components/menu/menu.cpp ui/components/menu/menu.h
    map/coordinatesmanager.cpp map/coordinatesmanager.h
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
        Qt6::Core5Compat
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

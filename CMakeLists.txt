cmake_minimum_required(VERSION 3.19)
project(DroneGroundStation LANGUAGES CXX C)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg Core5Compat SerialPort Sql)

qt_standard_project_setup()

# qt_standard_project_setup() включает только AUTOMOC и AUTOUIC.
# Без AUTORCC файл assets/res.qrc в списке исходников молча игнорируется
# и ресурсы (:/icons, :/fonts, :/locale ...) в программу не попадают.
set(CMAKE_AUTORCC ON)

qt_add_executable(DroneGroundStation
    WIN32 MACOSX_BUNDLE
    assets/res.qrc
    main.cpp
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    db/sqlite3.c db/sqlite3.h
    map/mapcore.cpp map/mapcore.h map/mapwidget.cpp map/mapwidget.h map/mapwidget.ui
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
    ui/pages/database/databasepage.cpp ui/pages/database/databasepage.h ui/pages/database/databasepage.ui ui/pages/device/devicepage.cpp ui/pages/device/devicepage.h ui/pages/device/devicepage.ui ui/pages/instruments/instrumentspage.cpp ui/pages/instruments/instrumentspage.h ui/pages/instruments/instrumentspage.ui ui/pages/recon/reconpage.cpp ui/pages/recon/reconpage.h ui/pages/recon/reconpage.ui ui/pages/settings/settingspage.cpp ui/pages/settings/settingspage.h ui/pages/settings/settingspage.ui
    ui/modules/tables/tabledata.cpp ui/modules/tables/tabledata.h ui/modules/tables/tabledata.ui
    backend/tables/tablemodel.cpp backend/tables/tablemodel.h
    ui/components/dialog/dialog.cpp ui/components/dialog/dialog.h
    ui/components/lineedit/lineedit.cpp ui/components/lineedit/lineedit.h ui/components/textedit/textedit.cpp ui/components/textedit/textedit.h
    ui/modules/settingsfield/settingsdeviceitem.cpp ui/modules/settingsfield/settingsdeviceitem.h ui/modules/settingsfield/settingsdeviceitem.ui ui/modules/settingsfield/settingsfield.cpp ui/modules/settingsfield/settingsfield.h ui/modules/settingsfield/settingsfield.ui
    ui/components/combobox/combobox.cpp ui/components/combobox/combobox.h ui/components/spinbox/spinbox.cpp ui/components/spinbox/spinbox.h
    ui/modules/searchBar/searchbar.cpp ui/modules/searchBar/searchbar.h ui/modules/searchBar/searchbar.ui
    ui/components/button/button.cpp ui/components/button/button.h ui/components/menu/menu.cpp ui/components/menu/menu.h
    map/mappicker.cpp map/mappicker.h
    backend/classes/core_types.cpp backend/classes/core_types.h
    backend/classes/drone_core.cpp backend/classes/drone_core.h
    backend/classes/core_stub.cpp backend/classes/core_stub.h
    backend/classes/drone_commands.cpp backend/classes/drone_commands.h
    backend/classes/drone_link.cpp backend/classes/drone_link.h
    backend/classes/message_parser.cpp backend/classes/message_parser.h
    backend/classes/recon_area.cpp backend/classes/recon_area.h
    backend/classes/logger_state.cpp backend/classes/logger_state.h
    backend/classes/core_engine.cpp backend/classes/core_engine.h
    backend/classes/session_store.cpp backend/classes/session_store.h
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
        Qt6::SerialPort
        Qt6::Sql
)

# ---- link_test: проверка DroneLink без окна (консольная программа) ----
qt_add_executable(link_test
    tests/link_test.cpp
    backend/classes/drone_link.cpp backend/classes/drone_link.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(link_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(link_test PRIVATE Qt6::Core Qt6::SerialPort)


# ---- parser_test: проверка MessageParser без окна (консольная программа) ----
qt_add_executable(parser_test
    tests/parser_test.cpp
    backend/classes/message_parser.cpp backend/classes/message_parser.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(parser_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(parser_test PRIVATE Qt6::Core)


# ---- commands_test: проверка DroneCommands без окна и без порта ----
qt_add_executable(commands_test
    tests/commands_test.cpp
    backend/classes/drone_commands.cpp backend/classes/drone_commands.h
    backend/classes/drone_link.cpp backend/classes/drone_link.h
    backend/classes/recon_area.cpp backend/classes/recon_area.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(commands_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(commands_test PRIVATE Qt6::Core Qt6::SerialPort)


# ---- area_test: проверка ReconArea без окна ----
qt_add_executable(area_test
    tests/area_test.cpp
    backend/classes/recon_area.cpp backend/classes/recon_area.h
    backend/classes/drone_commands.cpp backend/classes/drone_commands.h
    backend/classes/drone_link.cpp backend/classes/drone_link.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(area_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(area_test PRIVATE Qt6::Core Qt6::SerialPort)


# ---- state_test: проверка LoggerState без окна ----
qt_add_executable(state_test
    tests/state_test.cpp
    backend/classes/logger_state.cpp backend/classes/logger_state.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(state_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(state_test PRIVATE Qt6::Core)


# ---- engine_test: проверка всей цепочки ядра (CoreEngine) без окна и без логгера ----
qt_add_executable(engine_test
    tests/engine_test.cpp
    backend/classes/core_engine.cpp backend/classes/core_engine.h
    backend/classes/session_store.cpp backend/classes/session_store.h
    backend/classes/drone_link.cpp backend/classes/drone_link.h
    backend/classes/message_parser.cpp backend/classes/message_parser.h
    backend/classes/drone_commands.cpp backend/classes/drone_commands.h
    backend/classes/logger_state.cpp backend/classes/logger_state.h
    backend/classes/recon_area.cpp backend/classes/recon_area.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(engine_test PRIVATE Qt6::Core Qt6::SerialPort Qt6::Sql)


# ---- session_test: проверка SessionStore (база полётов) без окна ----
qt_add_executable(session_test
    tests/session_test.cpp
    backend/classes/session_store.cpp backend/classes/session_store.h
    backend/classes/recon_area.cpp backend/classes/recon_area.h
    backend/classes/core_types.cpp backend/classes/core_types.h
)
target_include_directories(session_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(session_test PRIVATE Qt6::Core Qt6::Sql)

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

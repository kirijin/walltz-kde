cmake_minimum_required(VERSION 3.22)
project(walltz)
set(VERSION "0.2.0")

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

include(ECMFindQmlModule)
include(ECMQmlModule)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Quick
    Gui
    QuickControls2
    Widgets
    Concurrent
    Svg
)

find_package(KF6 REQUIRED COMPONENTS
    Kirigami
    I18n
    CoreAddons
    IconThemes
)

ecm_find_qmlmodule(QtQuick)
ecm_find_qmlmodule(QtQuick.Layouts)
ecm_find_qmlmodule(QtQuick.Controls)
ecm_find_qmlmodule(org.kde.kirigami)

add_subdirectory(src)

# Install desktop file
install(FILES org.walltz.walltz.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

# Install icon
install(FILES icons/hicolor/scalable/apps/org.walltz.walltz.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps
)

# Install appdata
install(FILES src/org.walltz.walltz.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)

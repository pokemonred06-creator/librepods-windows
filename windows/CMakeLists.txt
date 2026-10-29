cmake_minimum_required(VERSION 3.16)

project(librepods-windows VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Quick Widgets Bluetooth Multimedia Network)
find_package(OpenSSL REQUIRED)

qt_standard_project_setup()

qt_add_executable(librepods-windows
    main.cpp
    logger.h
    media/mediacontroller.cpp
    media/mediacontroller.h
    media/windowsaudiocontroller.cpp
    media/windowsaudiocontroller.h
    media/playerstatuswatcher.cpp
    media/playerstatuswatcher.h
    airpods_packets.h
    trayiconmanager.cpp
    trayiconmanager.h
    enums.h
    battery.hpp
    BluetoothMonitor.cpp
    BluetoothMonitor.h
    autostartmanager.hpp
    BasicControlCommand.hpp
    deviceinfo.hpp
    ble/bleutils.cpp
    ble/bleutils.h
    ble/blemanager.cpp
    ble/blemanager.h
    thirdparty/QR-Code-generator/qrcodegen.cpp
    thirdparty/QR-Code-generator/qrcodegen.hpp
    QRCodeImageProvider.hpp
    eardetection.hpp
    windowssleepmonitor.hpp
    windowssleepmonitor.cpp
    spatialaudiotracker.hpp
)

qt_add_qml_module(librepods-windows
    URI windows
    VERSION 1.0
    QML_FILES
        Main.qml
        BatteryIndicator.qml
        SegmentedControl.qml
        PodColumn.qml
        Icon.qml
        KeysQRDialog.qml
)

# Add the resource file
qt_add_resources(librepods-windows "resources"
    PREFIX "/icons"
    FILES
        assets/airpods.png
        assets/pod.png
        assets/pod_case.png
        assets/pod3.png
        assets/pod3_case.png
        assets/pod4_case.png
        assets/podpro.png
        assets/podpro_case.png
        assets/podmax.png
        assets/fonts/SF-Symbols-6.ttf
)

# Link against required libraries
target_link_libraries(librepods-windows
    PRIVATE 
    Qt6::Quick 
    Qt6::Widgets 
    Qt6::Bluetooth 
    Qt6::Multimedia
    Qt6::Network
    OpenSSL::SSL 
    OpenSSL::Crypto
    # Windows-specific libraries
    Ole32
    User32
    PowrProf
    Cfgmgr32
)

# Set Windows subsystem to console for debugging, change to WINDOWS for release
set_target_properties(librepods-windows PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Installation
include(GNUInstallDirs)
install(TARGETS librepods-windows
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_add_executable(librepods-ctl
    librepods-ctl.cpp
)
target_link_libraries(librepods-ctl
    PRIVATE Qt6::Core Qt6::Network
)
install(TARGETS librepods-ctl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_add_executable(test-ear-detection
    tests/test_ear_detection.cpp
    media/mediacontroller.cpp
    media/mediacontroller.h
    media/windowsaudiocontroller.cpp
    media/windowsaudiocontroller.h
    media/playerstatuswatcher.cpp
    media/playerstatuswatcher.h
    logger.h
    enums.h
    eardetection.hpp
)
target_include_directories(test-ear-detection
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test-ear-detection
    PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
    Ole32
    User32
)

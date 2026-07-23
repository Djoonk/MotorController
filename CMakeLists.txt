cmake_minimum_required(VERSION 3.16)

project(MotorController VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пошук необхідних базових пакетів Qt та модуля Bluetooth
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Bluetooth)

# Список вихідних файлів проекту
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

# Створення виконуваного файлу (таргету) залежно від версії Qt та ОС
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MotorController
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        res.qrc
    )

set_target_properties(MotorController PROPERTIES
   QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")

set_target_properties(MotorController PROPERTIES
   QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
else()
    if(ANDROID)
        add_library(MotorController SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(MotorController
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# ЛІНКОВКА БІБЛІОТЕК (Викликається строго ПІСЛЯ створення MotorController)
target_link_libraries(MotorController PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Bluetooth
)

# Налаштування специфічних властивостей для різних ОС (iOS/macOS/Windows)
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MotorController)
endif()

set_target_properties(MotorController PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MotorController
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MotorController)
endif()
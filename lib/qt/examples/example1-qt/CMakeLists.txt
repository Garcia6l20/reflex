find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2)

if(NOT Qt6_FOUND)
  message(STATUS "QML tests disabled (some components are missing)")
  return()
endif()

set(QT_QML_GENERATE_QMLLS_INI ON) # Generate QML language server infos

qt_policy(SET QTP0001 NEW) # ':/qt/qml/' is the default resource prefix for QML modules.
qt_policy(SET QTP0004 NEW) # Extra directories with QML files in a QML module need extra qmldir files.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(reflex-qt-example1-qt
  src/main.cpp
  src/Example.hpp
)
target_link_libraries(reflex-qt-example1-qt PRIVATE reflex.qt Qt6::Gui Qt6::Qml)
target_include_directories(reflex-qt-example1-qt PRIVATE src)

qt_add_qml_module(reflex-qt-example1-qt
  URI Example1Qt
  VERSION 1.0
  QML_FILES
    qml/App.qml
  RESOURCE_PREFIX /reflex
  OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Example1Qt
  DEPENDENCIES QtQuick
)


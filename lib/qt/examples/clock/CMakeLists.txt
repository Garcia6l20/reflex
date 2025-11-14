find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2)

if(NOT Qt6_FOUND)
  message(STATUS "QML tests disabled (some components are missing)")
  return()
endif()

set(QT_QML_GENERATE_QMLLS_INI ON) # Generate QML language server infos

qt_policy(SET QTP0001 NEW) # ':/qt/qml/' is the default resource prefix for QML modules.
qt_policy(SET QTP0004 NEW) # Extra directories with QML files in a QML module need extra qmldir files.

set(CMAKE_AUTOMOC OFF) # turn auto-moc off
set(CMAKE_AUTORCC ON)

add_executable(reflex-qclock
  src/main.cpp
  src/ClockController.hpp
)
target_link_libraries(reflex-qclock PRIVATE reflex.qt Qt6::Gui Qt6::Qml)

include(ReflexQt)
reflex_qt_add_qml_module(reflex-qclock
  URI Clock
  VERSION 1.0
  QML_FILES
    qml/App.qml
  RESOURCE_PREFIX /clock
  OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Clock
  DEPENDENCIES QtQuick
)

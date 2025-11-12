find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2)

if(NOT Qt6_FOUND)
  message(STATUS "QML tests disabled (some components are missing)")
  return()
endif()


set(QT_QML_GENERATE_QMLLS_INI ON) # Generate QML language server infos

qt_policy(SET QTP0001 NEW) # ':/qt/qml/' is the default resource prefix for QML modules.
qt_policy(SET QTP0004 NEW) # Extra directories with QML files in a QML module need extra qmldir files.

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC ON)

add_executable(reflex-qt-example1
  src/main.cpp

  # 1. add headers to your target
  src/Example.hpp
)
target_link_libraries(reflex-qt-example1 PRIVATE reflex.qt Qt6::Gui Qt6::Qml)

# 2. include ReflexQt module
include(ReflexQt)

# 3. create your module
reflex_qt_add_qml_module(reflex-qt-example1
  URI Example1
  VERSION 1.0
  TYPES
    Example # 4. inform types to export
  QML_FILES
    qml/App.qml
  RESOURCE_PREFIX /reflex
  OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Example1
  DEPENDENCIES QtQuick
)

# 5. (optional) create dump target
reflex_qt_add_dump(reflex-qt-example1
  TYPES Example
)

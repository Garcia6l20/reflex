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

# 1. include ReflexQt module
include(ReflexQt)

set(REFLEX_QT_EXAMPLE1_MODE SHARED)

if ("${REFLEX_QT_EXAMPLE1_MODE}" STREQUAL "MAIN")
  reflex_qt_add_executable(reflex-qt-example1
    # 2. add headers to your target
    src/Example.hpp
    src/Example.cpp
    src/main.cpp
  )
  target_link_libraries(reflex-qt-example1 PUBLIC reflex.qt Qt6::Gui Qt6::Qml)

  # 3. use reflex overridden qt functions
  reflex_qt_add_qml_module(reflex-qt-example1
    URI Example1
    VERSION 1.0
    QML_FILES
      qml/App.qml
    RESOURCE_PREFIX /reflex
    OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Example1
    DEPENDENCIES QtQuick
  )

elseif ("${REFLEX_QT_EXAMPLE1_MODE}" STREQUAL "STATIC")

  reflex_qt_add_library(reflex-qt-example1
    # 2. add headers to your target
    src/Example.hpp
    src/Example.cpp
  )
  target_link_libraries(reflex-qt-example1 PUBLIC reflex.qt Qt6::Gui Qt6::Qml)

  add_executable(reflex-qt-example1-main src/main.cpp)
  target_link_libraries(reflex-qt-example1-main PRIVATE reflex-qt-example1-plugin)

  # 3. use reflex overridden qt functions
  reflex_qt_add_qml_module(reflex-qt-example1
    PLUGIN_TARGET reflex-qt-example1-plugin
    URI Example1
    VERSION 1.0
    QML_FILES
      qml/App.qml
    RESOURCE_PREFIX /reflex
    OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Example1
    DEPENDENCIES QtQuick
  )
else() # assume shared

  reflex_qt_add_library(reflex-qt-example1 SHARED
    # 2. add headers to your target
    src/Example.hpp
    src/Example.cpp
  )
  target_link_libraries(reflex-qt-example1 PUBLIC reflex.qt Qt6::Gui Qt6::Qml)

  add_executable(reflex-qt-example1-main src/main.cpp)
  target_link_libraries(reflex-qt-example1-main PUBLIC Qt6::Gui Qt6::Qml) # main dont need reflex.qt

  # just to make sure plugin is compiled when main is built
  add_dependencies(reflex-qt-example1-main reflex-qt-example1-plugin)

  # 3. use reflex overridden qt functions
  reflex_qt_add_qml_module(reflex-qt-example1
    PLUGIN_TARGET reflex-qt-example1-plugin
    URI Example1
    VERSION 1.0
    QML_FILES
      qml/App.qml
    RESOURCE_PREFIX /reflex
    OUTPUT_DIRECTORY ${QML_IMPORT_PATH}/Example1
    DEPENDENCIES QtQuick
  )

endif()


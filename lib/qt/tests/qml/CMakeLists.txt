find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QuickTest)

if(NOT Qt6_FOUND)
  message(STATUS "QML tests disabled (some components are missing)")
  return()
endif()

qt_standard_project_setup(REQUIRES 6.6)

add_definitions(-DQUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(reflex-qt-qml-test test-qml.cpp)
target_link_libraries(reflex-qt-qml-test
  PRIVATE
    Qt6::QuickTest
    Qt6::Qml
    Qt6::Gui
    reflex.qt
)

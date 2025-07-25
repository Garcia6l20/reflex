find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QuickTest)

if(NOT Qt6_FOUND)
  message(STATUS "Qt tests disabled (some components are missing)")
  return()
endif()

link_libraries(reflex.qt Qt6::Core Qt6::Gui Qt6::Qml)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
add_library(reflex-qt-test-objects
  test_object.cpp
)
set(CMAKE_AUTOMOC OFF)

add_executable(reflex-qt-qobject test-qobject.cpp)
target_link_libraries(reflex-qt-qobject PRIVATE reflex-qt-test-objects)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
add_executable(reflex-qt-widgets test-widgets.cpp)
target_link_libraries(reflex-qt-widgets PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME reflex-qt-widgets COMMAND reflex-qt-widgets)

add_subdirectory(qml)

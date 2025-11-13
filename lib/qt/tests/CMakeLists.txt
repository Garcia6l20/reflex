find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QuickTest)

if(NOT Qt6_FOUND)
  message(STATUS "Qt tests disabled (some components are missing)")
  return()
endif()

include(ReflexQt)

link_libraries(reflex.qt Qt6::Core Qt6::Gui Qt6::Qml)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
add_library(reflex-qt-test-reference-objects
  ref_object.cpp
)
set(CMAKE_AUTOMOC OFF)

add_executable(reflex-qt-qobject
  test-qobject.cpp

  test_object.hpp
)
reflex_qt_moc(reflex-qt-qobject OBJECTS test_object)
target_link_libraries(reflex-qt-qobject PRIVATE reflex-qt-test-reference-objects)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
add_executable(reflex-qt-widgets test-widgets.cpp SimpleWidget.hpp)
target_link_libraries(reflex-qt-widgets PRIVATE Qt6::Widgets Qt6::Test)
reflex_qt_moc(reflex-qt-widgets OBJECTS SimpleWidget)
add_test(NAME reflex-qt-widgets COMMAND reflex-qt-widgets)

add_subdirectory(qml)

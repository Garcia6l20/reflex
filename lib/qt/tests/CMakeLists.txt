include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.qt Qt6::Core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
add_library(reflex-qt-test-objects
  test_object.cpp
)
set(CMAKE_AUTOMOC OFF)

add_executable(reflex-qt-qobject test-qobject.cpp)
target_link_libraries(reflex-qt-qobject PRIVATE reflex-qt-test-objects)



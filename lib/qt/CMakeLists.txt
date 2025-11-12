find_package(Qt6 COMPONENTS Core)
if (NOT Qt6_FOUND)
  message(STATUS "reflex.qt skipped: Qt6 not found")
  return()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_library(reflex.qt INTERFACE)
target_include_directories(reflex.qt INTERFACE include)
target_link_libraries(reflex.qt INTERFACE reflex.utility Qt6::Core)

add_library(reflex.qt.moc src/moc.cpp)
target_link_libraries(reflex.qt.moc PUBLIC reflex.qt PRIVATE reflex.serialize)

if(REFLEX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(REFLEX_BUILD_TESTS)
  add_subdirectory(tests)
endif()


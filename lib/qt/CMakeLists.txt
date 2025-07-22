find_package(Qt6 COMPONENTS Core)
if (NOT Qt6_FOUND)
  message(STATUS "reflex.qt skipped: Qt6 not found")
  return()
endif()

add_library(reflex.qt INTERFACE)
target_include_directories(reflex.qt INTERFACE include)
target_link_libraries(reflex.qt INTERFACE reflex.utility)

add_subdirectory(tests)
add_subdirectory(examples)

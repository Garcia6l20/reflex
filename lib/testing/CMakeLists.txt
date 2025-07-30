add_library(reflex.testing INTERFACE)
target_include_directories(reflex.testing INTERFACE include)
target_link_libraries(reflex.testing INTERFACE reflex.utility reflex.cli)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(tests)

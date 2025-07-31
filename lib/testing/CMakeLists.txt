add_library(reflex.testing INTERFACE)
target_include_directories(reflex.testing INTERFACE include)
target_link_libraries(reflex.testing INTERFACE reflex.utility reflex.cli)
add_subdirectory(tests)

add_library(reflex.cli INTERFACE)
target_include_directories(reflex.cli INTERFACE include)
target_link_libraries(reflex.cli INTERFACE reflex.utility)

add_subdirectory(examples)
add_subdirectory(tests)

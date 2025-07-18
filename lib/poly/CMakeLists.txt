add_library(reflex.poly INTERFACE)
target_include_directories(reflex.poly INTERFACE include)
target_link_libraries(reflex.poly INTERFACE reflex.utility)

add_subdirectory(tests)

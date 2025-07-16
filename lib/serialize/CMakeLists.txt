add_library(reflex.serialize INTERFACE)
target_include_directories(reflex.serialize INTERFACE include)
target_link_libraries(reflex.serialize INTERFACE reflex.utility)

# add_subdirectory(examples)
add_subdirectory(tests)

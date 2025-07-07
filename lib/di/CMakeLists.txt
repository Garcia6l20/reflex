add_library(reflex.di INTERFACE)
target_include_directories(reflex.di INTERFACE include)
target_link_libraries(reflex.di INTERFACE reflex.utility)

# add_subdirectory(examples)
add_subdirectory(tests)

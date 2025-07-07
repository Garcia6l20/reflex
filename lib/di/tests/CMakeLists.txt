include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.di Catch2::Catch2WithMain)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reflex-di-simple-test test-simple.cpp)
add_executable(reflex-di6-interfaces-test test-interfaces.cpp)
# add_executable(reflex-di-concept-binding-test test-concept-binding.cpp)
add_executable(reflex-di-concepts-test test-concepts.cpp)

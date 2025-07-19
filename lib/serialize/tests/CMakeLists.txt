include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.serialize Catch2::Catch2WithMain)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reflex-serialize-simple-test test-simple.cpp)
add_executable(reflex-serialize-poly-test test-poly.cpp)
target_link_libraries(reflex-serialize-poly-test PRIVATE reflex.poly)

include(CPM)
CPMAddPackage("gh:catchorg/Catch2@3.4.0")

link_libraries(reflex.utility Catch2::Catch2WithMain)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reflex-utility-named-tuple-test test-named-tuple.cpp)

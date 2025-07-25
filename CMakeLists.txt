cmake_minimum_required(VERSION 3.20)

project(reflex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(REFLEX_BUILD_EXAMPLES "Build reflex examples" OFF)
option(REFLEX_BUILD_TESTS "Build reflex tests" OFF)
if(REFLEX_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(lib)

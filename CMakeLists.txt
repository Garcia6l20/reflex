cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ReflexCppModules)

project(reflex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# include(Sanitizers)
# link_libraries(sanitizers::all)

option(REFLEX_BUILD_EXAMPLES "Build reflex examples" OFF)
option(REFLEX_BUILD_TESTS "Build reflex tests" OFF)

include(ReflexRequirements)

if(REFLEX_BUILD_TESTS)
  include(ReflexTesting)
  enable_testing()
endif()


add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(serde)

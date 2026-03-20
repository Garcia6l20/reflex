if (CMAKE_VERSION VERSION_LESS 4.2)
    message(FATAL_ERROR "CMake 4.2 or higher is required to build this project.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
function(reflex_add_cxx_module_library target)

  set(options SHARED STATIC MODULE_STD)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(ARGS
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )

  set(_type STATIC)

  if (ARGS_SHARED)
    if (ARGS_STATIC)
      message(FATAL_ERROR "Cannot use SHARED and STATIC together")
    endif()
    set(_type SHARED)
  endif()

  if (ARGS_STATIC)
    if (ARGS_SHARED)
      message(FATAL_ERROR "Cannot use SHARED and STATIC together")
    endif()
    set(_type STATIC)
  endif()

  set(_all_sources ${ARGS_UNPARSED_ARGUMENTS})
  if (NOT _all_sources)
    set(_type INTERFACE)
  endif()

  if (${_type} STREQUAL INTERFACE)
    set(_dep_mode INTERFACE)
  else()
    set(_dep_mode PUBLIC)
  endif()

  add_library(${target} ${_type} ${_all_sources})
  target_compile_features(${target} ${_dep_mode} cxx_std_26)
  target_include_directories(${target} ${_dep_mode} ${CMAKE_CURRENT_SOURCE_DIR}/include)

endfunction()

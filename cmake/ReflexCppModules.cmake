if (CMAKE_VERSION VERSION_LESS 4.2)
    message(FATAL_ERROR "CMake 4.2 or higher is required to build this project.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_BMI_DIRECTORY "${CMAKE_BINARY_DIR}/bmi")
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CMAKE_CXX_MODULE_STD OFF)

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

  set(_cpp_sources ${_all_sources})
  list(FILTER _cpp_sources INCLUDE REGEX ".*\\.cpp$")

  set(_cppm_sources ${_all_sources})
  list(FILTER _cppm_sources INCLUDE REGEX ".*\\.cppm$")

  set(_hpp_sources ${_all_sources})
  list(FILTER _hpp_sources INCLUDE REGEX ".*\\.h$|.*\\.hpp$")

  if (${_type} STREQUAL INTERFACE)
    set(_public_dep_mode INTERFACE)
  else()
    set(_public_dep_mode PUBLIC)
  endif()

  add_library(${target} ${_type} ${_cpp_sources})
  if (_hpp_sources)
    set(_public_hpp_source ${_hpp_sources})
    list(FILTER _public_hpp_source INCLUDE REGEX ".?include/.*\\.h(pp)?$")
    set(_private_hpp_source ${_hpp_sources})
    list(FILTER _private_hpp_source EXCLUDE REGEX ".?include/.*\\.h(pp)?$")
    if (_public_hpp_source)
      target_sources(${target}
        ${_public_dep_mode}
          FILE_SET public_headers TYPE HEADERS FILES
            ${_public_hpp_source}
      )
    endif()
    if (_private_hpp_source)
      target_sources(${target}
        PRIVATE
          FILE_SET private_headers TYPE HEADERS FILES
            ${_private_hpp_source}
      )
    endif()
  endif()

  if (_cppm_sources AND REFLEX_CXX_MODULES_ENABLED)
    target_sources(${target}
      PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
          ${_cppm_sources}
    )
    set_property(TARGET ${target} PROPERTY CXX_MODULE_BMI_DIRECTORY "${CMAKE_CXX_MODULE_BMI_DIRECTORY}")
  endif()

  target_compile_features(${target} ${_public_dep_mode} cxx_std_26)
  target_include_directories(${target} ${_public_dep_mode} ${CMAKE_CURRENT_SOURCE_DIR}/include)

endfunction()

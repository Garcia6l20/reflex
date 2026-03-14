if (CMAKE_VERSION VERSION_LESS 4.2)
    message(FATAL_ERROR "CMake 4.2 or higher is required to build this project.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_BMI_DIRECTORY "${CMAKE_BINARY_DIR}/bmi")
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CMAKE_CXX_MODULE_STD OFF)

if (CMAKE_CXX_MODULE_STD)
  if (CMAKE_VERSION VERSION_LESS 4.3)
      set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
  else()
      set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
  endif()
endif()

function(reflex_enable_cxx_module_std target)
  if (CMAKE_CXX_MODULE_STD)
    set_property(TARGET ${target} PROPERTY CXX_MODULE_STD ON)
    target_compile_features(${target} PRIVATE cxx_std_26)
  endif()
endfunction()

function(reflex_add_cxx_module_library target)

  set(options MODULE_STD)
  set(oneValueArgs TYPE BMI_DIR)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(ARGS
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )

  if (NOT ARGS_TYPE)
    set(ARGS_TYPE STATIC)
  endif()

  if (NOT ARGS_SOURCES)
    message(FATAL_ERROR "No sources provided for target ${target}")
  endif()

  add_library(${target} ${ARGS_TYPE})
  target_sources(${target}
    PUBLIC
      FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ${ARGS_SOURCES}
  )
  target_compile_features(${target} PUBLIC cxx_std_26)

  if (ARGS_MODULE_STD)
    reflex_enable_cxx_module_std(${target})
  endif()

  # Set BMI output directory (per-target or global)
  if (ARGS_BMI_DIR)
    set_property(TARGET ${target} PROPERTY CXX_MODULE_BMI_DIRECTORY "${ARGS_BMI_DIR}")
  elseif(CMAKE_CXX_MODULE_BMI_DIRECTORY)
    set_property(TARGET ${target} PROPERTY CXX_MODULE_BMI_DIRECTORY "${CMAKE_CXX_MODULE_BMI_DIRECTORY}")
  endif()

endfunction()

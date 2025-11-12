set(REFLEX_EXPORT_JSON_CPP_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/reflex_qt_export_json.cpp.in)
set(REFLEX_MOC_CPP_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/reflex_qt_moc.cpp.in)
set(REFLEX_DUMP_CPP_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/reflex_qt_dump.cpp.in)

function(_get_includes target headers_var include_dirs_var includes_var)
    get_target_property(srcs ${target} SOURCES)

    set(headers)
    set(include_dirs)
    set(includes)

    foreach(file IN LISTS srcs)
        get_filename_component(ext "${file}" EXT)
        if(ext MATCHES "\\.h(pp)?$")
            get_filename_component(name "${file}" NAME)
            get_filename_component(dir "${file}" DIRECTORY)
            list(APPEND include_dirs ${dir})
            list(APPEND includes "#include <${name}>")
            list(APPEND headers ${file})
        endif()
    endforeach()

    list(REMOVE_DUPLICATES include_dirs)
    
    set(${headers_var} ${headers} PARENT_SCOPE)
    set(${include_dirs_var} ${include_dirs} PARENT_SCOPE)
    set(${includes_var} ${includes} PARENT_SCOPE)

endfunction()


function(reflex_qt_moc target)

    set(options)
    set(singleValueArgs
        TYPE_IMPLEMENTATIONS_OUTPUT_VAR
    )
    set(multiValueArgs
        TYPES
    )
    cmake_parse_arguments(ARGS "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    _get_includes(${target} headers include_dirs INCLUDES)

    set(TYPE_IMPLEMENTATIONS)
    foreach(type IN LISTS ARGS_TYPES)
        list(APPEND TYPE_IMPLEMENTATIONS "REFLEX_QT_OBJECT_IMPL(${type})")
    endforeach()

    list(JOIN ARGS_TYPES ", " TYPES)
    list(JOIN TYPE_IMPLEMENTATIONS "\n" TYPE_IMPLEMENTATIONS)

    #
    # MOC objects
    #

    set(MOC_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-moc.cpp)

    configure_file(${REFLEX_MOC_CPP_TEMPLATE} ${MOC_OUTPUT_FILE} @ONLY)

    add_library(${target}-moc OBJECT ${MOC_OUTPUT_FILE})

    target_link_libraries(${target}-moc PRIVATE reflex.qt.moc)
    target_include_directories(${target}-moc PRIVATE ${include_dirs})

    get_target_property(target_include_dirs ${target} INCLUDE_DIRECTORIES)
    if (target_include_dirs)
        target_include_directories(${target}-moc PRIVATE ${target_include_dirs})
    endif()

    get_target_property(target_link_libs ${target} LINK_LIBRARIES)
    if (target_link_libs)
        target_link_libraries(${target}-moc PRIVATE ${target_link_libs})
    endif()

    get_target_property(target_compile_defs ${target} COMPILE_DEFINITIONS)
    if (target_compile_defs)
        target_compile_definitions(${target}-moc PRIVATE ${target_compile_defs})
    endif()

    # FIXME is there another way to get rid of this warning ???
    target_compile_options(${target}-moc PUBLIC -Wno-undefined-var-template)

    #
    # Patch target
    #

    target_link_libraries(${target} PUBLIC ${target}-moc)
    target_include_directories(${target} PRIVATE ${include_dirs})

    if (ARGS_TYPE_IMPLEMENTATIONS_OUTPUT_VAR)
        set(${ARGS_TYPE_IMPLEMENTATIONS_OUTPUT_VAR} ${TYPE_IMPLEMENTATIONS} PARENT_SCOPE)
    endif()

endfunction()

function(reflex_qt_add_qml_module target)

    set(options)
    set(singleValueArgs
        PLUGIN_TARGET
        URI
        VERSION
        OUTPUT_DIRECTORY
        RESOURCE_PREFIX
        DEPENDENCIES
    )
    set(multiValueArgs
        TYPES
        QML_FILES
    )
    cmake_parse_arguments(ARGS "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARGS_URI)
        message(FATAL_ERROR "Module URI is required")
    endif()

    if (NOT ARGS_VERSION)
        set(ARGS_VERSION 1.0)
    endif()

    if (NOT ARGS_OUTPUT_DIRECTORY)
        set(ARGS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_URI})
    endif()

    string(REPLACE "." ";" VERSION_LIST ${ARGS_VERSION})
    list(GET VERSION_LIST 0 MAJOR_VERSION)
    list(GET VERSION_LIST 1 MINOR_VERSION)

    _get_includes(${target} headers include_dirs INCLUDES)

    list(JOIN ARGS_TYPES ", " TYPES)

    set(JSON_OUTPUT_FILE ${ARGS_OUTPUT_DIRECTORY}/${ARGS_URI}.json)
    set(QML_TYPES_OUTPUT_FILE ${ARGS_OUTPUT_DIRECTORY}/${ARGS_URI}.qmltypes)
    set(QML_TYPE_REGISTRATION_OUTPUT_FILE ${ARGS_OUTPUT_DIRECTORY}/${target}-qmltypes-registration.cpp)

    message(STATUS "Reflex MOC ${target} header files: ${headers}")

    #
    # MOC objects
    #
    reflex_qt_moc(${target} TYPES ${ARGS_TYPES} TYPE_IMPLEMENTATIONS_OUTPUT_VAR TYPE_IMPLEMENTATIONS)

    #
    # JSON export
    #

    set(EXPORT_JSON_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-export-json.cpp)
    configure_file(${REFLEX_EXPORT_JSON_CPP_TEMPLATE} ${EXPORT_JSON_OUTPUT_FILE} @ONLY)

    add_executable(${target}-export-json ${EXPORT_JSON_OUTPUT_FILE})
    target_link_libraries(${target}-export-json PRIVATE reflex.qt.moc ${target}-moc)
    target_include_directories(${target}-export-json PRIVATE ${include_dirs})

    add_custom_command(OUTPUT ${JSON_OUTPUT_FILE}
        COMMAND ${target}-export-json
        DEPENDS ${target}-export-json
        COMMENT "Exporting ${target} json data..."
    )

    add_custom_command(OUTPUT ${QML_TYPE_REGISTRATION_OUTPUT_FILE}
        COMMAND /usr/lib/qt6/qmltyperegistrar ${JSON_OUTPUT_FILE}
            --import-name        ${ARGS_URI}
            --generate-qmltypes  ${QML_TYPES_OUTPUT_FILE}
            --major-version      ${MAJOR_VERSION}
            --minor-version      ${MINOR_VERSION}
            -o                   ${QML_TYPE_REGISTRATION_OUTPUT_FILE}
        DEPENDS ${JSON_OUTPUT_FILE}
        COMMENT "Generating ${target} qml registration..."
    )

    #
    # Patch target
    #

    target_sources(${target} PRIVATE ${QML_TYPE_REGISTRATION_OUTPUT_FILE})

    #
    # Create QML module
    #

    set(EXTRA_ARGS)
    if (ARGS_PLUGIN_TARGET)
        list(APPEND EXTRA_ARGS PLUGIN_TARGET ${ARGS_PLUGIN_TARGET})
    endif()

    if (ARGS_DEPENDENCIES)
        list(APPEND EXTRA_ARGS DEPENDENCIES ${ARGS_DEPENDENCIES})
    endif()

    qt_add_qml_module(${target}
        URI ${ARGS_URI}
        VERSION ${ARGS_VERSION}
        QML_FILES ${ARGS_QML_FILES}
        RESOURCE_PREFIX ${ARGS_RESOURCE_PREFIX}
        OUTPUT_DIRECTORY ${ARGS_OUTPUT_DIRECTORY}

        # Use our generated types
        NO_GENERATE_QMLTYPES
        TYPEINFO ${ARGS_URI}.qmltypes

        ${EXTRA_ARGS}
    )

endfunction()


function(reflex_qt_add_dump target)
    set(options)
    set(singleValueArgs
    )
    set(multiValueArgs
        TYPES
    )
    cmake_parse_arguments(ARGS "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})
    
    _get_includes(${target} headers include_dirs INCLUDES)

    list(JOIN ARGS_TYPES ", ^^" TYPES)
    set(TYPES "^^${TYPES}")

    #
    # DUMP objects
    #

    set(DUMP_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-dump.cpp)

    configure_file(${REFLEX_DUMP_CPP_TEMPLATE} ${DUMP_OUTPUT_FILE} @ONLY)

    add_executable(${target}-dump ${DUMP_OUTPUT_FILE})
    target_link_libraries(${target}-dump PRIVATE reflex.qt ${target}-moc)
    target_include_directories(${target}-dump PRIVATE ${include_dirs})

endfunction()


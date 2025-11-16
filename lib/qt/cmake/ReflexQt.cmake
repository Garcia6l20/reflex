set(REFLEX_EXPORT_JSON_CPP_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/reflex_qt_export_json.cpp.in)
set(REFLEX_QMLTYPES_REGISTRATION_CPP_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/reflex_qt_qmltypes_registration.cpp.in)
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
    list(JOIN includes "\n" includes)

    set(${headers_var} ${headers} PARENT_SCOPE)
    set(${include_dirs_var} ${include_dirs} PARENT_SCOPE)
    set(${includes_var} ${includes} PARENT_SCOPE)

endfunction()

function(_bind_target src dst)
    get_target_property(src_type ${src} TYPE)

    if (${src_type} STREQUAL EXECUTABLE)
        target_link_libraries(${src} INTERFACE)
        target_link_libraries(${dst}
            PUBLIC
                $<LIST:FILTER,
                    $<TARGET_PROPERTY:${src},INTERFACE_LINK_LIBRARIES>
                    ,EXCLUDE,${dst}>
        )

        target_include_directories(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_INCLUDE_DIRECTORIES>
        )

        target_compile_definitions(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_DEFINITIONS>
        )

        target_compile_features(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_FEATURES>
        )

        target_compile_options(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_OPTIONS>
        )
    else()
        target_link_libraries(${dst}
            PUBLIC
                $<LIST:FILTER,
                    $<TARGET_PROPERTY:${src},LINK_ONLY>
                    ,EXCLUDE,${dst}>
            # PUBLIC
            #     $<LIST:FILTER,
            #         $<TARGET_PROPERTY:${src},INTERFACE_LINK_LIBRARIES>
            #         ,EXCLUDE,${dst}>
        )

        target_include_directories(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_INCLUDE_DIRECTORIES>
        )

        target_compile_definitions(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_DEFINITIONS>
        )

        target_compile_features(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_FEATURES>
        )

        target_compile_options(${dst}
            PUBLIC $<TARGET_PROPERTY:${src},INTERFACE_COMPILE_OPTIONS>
        )
    endif()
endfunction()

function(_ensure_objects_target)
    if (NOT TARGET ${target}-objects)
        message(FATAL_ERROR "Objects target not found, you must use reflex_qt_add_executable/reflex_qt_add_library")
    endif()
endfunction()


function(reflex_qt_moc target)
    if(TARGET ${target}-moc)
        return()
    endif()

    _ensure_objects_target()

    _get_includes(${target}-objects headers include_dirs INCLUDES)
    set(MOC_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-moc.cpp)
    configure_file(${REFLEX_MOC_CPP_TEMPLATE} ${MOC_OUTPUT_FILE} @ONLY)
    target_sources(${target}-objects PUBLIC ${MOC_OUTPUT_FILE})
    target_include_directories(${target}-objects PUBLIC ${include_dirs})
    add_library(${target}-moc ALIAS ${target}-objects)

endfunction()


function(reflex_qt_add_executable target)
    add_library(${target}-objects OBJECT ${ARGN})
    target_link_libraries(${target}-objects PRIVATE reflex.qt)

    reflex_qt_moc(${target})

    file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/${target}-dummy.cpp)
    add_executable(${target} ${CMAKE_CURRENT_BINARY_DIR}/${target}-dummy.cpp)
    target_link_libraries(${target} INTERFACE)
    target_link_libraries(${target} PRIVATE ${target}-objects reflex.qt)
    _bind_target(${target} ${target}-objects)

endfunction()

function(reflex_qt_add_library target)

    set(ARGS "${ARGN}")
    if (ARGS)
        list(GET ARGS 0 lib_type)
        set(valid_lib_types STATIC SHARED)
        set(invalid_lib_types MODULE OBJECT INTERFACE)
        if (lib_type IN_LIST valid_lib_types)
            # OK
            list(REMOVE_AT ARGS 0)
        elseif(lib_type IN_LIST invalid_lib_types)
            message(FATAL_ERROR "Unsupported library type: ${lib_type}")
        else()
            # assume not a lib type
            set(lib_type)
        endif()
    endif()

    add_library(${target}-objects OBJECT ${ARGS})
    target_link_libraries(${target}-objects PRIVATE reflex.qt)

    reflex_qt_moc(${target})

    add_library(${target} ${lib_type})
    target_link_libraries(${target} INTERFACE)
    target_link_libraries(${target} PRIVATE ${target}-objects reflex.qt)
    _bind_target(${target} ${target}-objects)

endfunction()


function(reflex_qt_add_qml_module target)

    _ensure_objects_target(${target})

    set(options)
    set(singleValueArgs
        URI
        VERSION
        OUTPUT_DIRECTORY
    )
    set(multiValueArgs)
    cmake_parse_arguments(ARGS "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARGS_URI)
        message(FATAL_ERROR "Module URI is required")
    endif()

    if(NOT ARGS_VERSION)
        set(ARGS_VERSION 1.0)
    endif()

    if(NOT ARGS_OUTPUT_DIRECTORY)
        set(ARGS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_URI})
    endif()

    string(REPLACE "." ";" VERSION_LIST ${ARGS_VERSION})
    list(GET VERSION_LIST 0 MAJOR_VERSION)
    list(GET VERSION_LIST 1 MINOR_VERSION)

    _get_includes(${target}-objects headers include_dirs INCLUDES)

    set(QML_TYPES_OUTPUT_FILE ${ARGS_OUTPUT_DIRECTORY}/${ARGS_URI}.qmltypes)
    set(JSON_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_URI}.json)
    set(QML_TYPE_REGISTRATION_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qmltypes-registration.cpp)

    message(STATUS "Reflex MOC ${target} header files: ${headers}")

    #
    # MOC objects
    #
    reflex_qt_moc(${target})

    #
    # JSON export
    #

    set(EXPORT_JSON_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-export-json.cpp)
    configure_file(${REFLEX_EXPORT_JSON_CPP_TEMPLATE} ${EXPORT_JSON_OUTPUT_FILE} @ONLY)

    set(MODULE_NAME ${ARGS_URI})
    string(REPLACE "." "_" MODULE_FN_NAME "${MODULE_NAME}")
    configure_file(${REFLEX_QMLTYPES_REGISTRATION_CPP_TEMPLATE} ${QML_TYPE_REGISTRATION_OUTPUT_FILE} @ONLY)

    add_executable(${target}-export-json ${EXPORT_JSON_OUTPUT_FILE})
    target_link_libraries(${target}-export-json PRIVATE reflex.qt.moc ${target}-moc)
    _bind_target(${target} ${target}-export-json)
    target_include_directories(${target}-export-json PRIVATE ${include_dirs})
    target_link_options(${target}-export-json PRIVATE -Wl,--wrap=main)

    add_custom_command(OUTPUT ${JSON_OUTPUT_FILE}
        COMMAND ${target}-export-json ${JSON_OUTPUT_FILE}
        DEPENDS ${target}-export-json
        COMMENT "Exporting ${target} json data..."
    )

    add_custom_command(
        OUTPUT ${QML_TYPES_OUTPUT_FILE}
        COMMAND Qt6::qmltyperegistrar ${JSON_OUTPUT_FILE}
        --import-name ${ARGS_URI}
        --generate-qmltypes ${QML_TYPES_OUTPUT_FILE}
        --major-version ${MAJOR_VERSION}
        --minor-version ${MINOR_VERSION}
        # -o ${QML_TYPE_REGISTRATION_OUTPUT_FILE}
        -o /dev/null # we use our reflection based registration
        DEPENDS ${JSON_OUTPUT_FILE}
        COMMENT "Generating ${target} qml types (${QML_TYPES_OUTPUT_FILE})..."
    )

    #
    # Patch target
    #

    target_sources(${target} PRIVATE
        ${QML_TYPE_REGISTRATION_OUTPUT_FILE}
        ${QML_TYPES_OUTPUT_FILE}
    )

    #
    # Create QML module
    #

    set(EXTRA_ARGS)
    if(ARGS_PLUGIN_TARGET)
        list(APPEND EXTRA_ARGS PLUGIN_TARGET ${ARGS_PLUGIN_TARGET})
    endif()

    if(ARGS_DEPENDENCIES)
        list(APPEND EXTRA_ARGS DEPENDENCIES ${ARGS_DEPENDENCIES})
    endif()

    # Forward all unparsed args to qt_add_qml_module
    list(APPEND EXTRA_ARGS ${ARGS_UNPARSED_ARGUMENTS})

    qt_add_qml_module(${target}
        URI ${ARGS_URI}
        VERSION ${ARGS_VERSION}
        OUTPUT_DIRECTORY ${ARGS_OUTPUT_DIRECTORY}

        # Use our generated types
        NO_GENERATE_QMLTYPES
        TYPEINFO ${ARGS_URI}.qmltypes

        ${EXTRA_ARGS}
    )

endfunction()


function(reflex_qt_add_dump target)

    _ensure_objects_target(${target})

    _get_includes(${target}-objects headers include_dirs INCLUDES)

    #
    # DUMP objects
    #

    set(DUMP_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}-qt-dump.cpp)

    configure_file(${REFLEX_DUMP_CPP_TEMPLATE} ${DUMP_OUTPUT_FILE} @ONLY)

    add_executable(${target}-dump ${DUMP_OUTPUT_FILE})
    target_link_options(${target}-dump PRIVATE -Wl,--wrap=main)
    target_link_libraries(${target}-dump PRIVATE reflex.qt ${target}-moc)

endfunction()


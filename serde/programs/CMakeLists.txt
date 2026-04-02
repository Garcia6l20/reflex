if (NOT REFLEX_CXX_MODULES_ENABLED)
    return()
endif()

add_executable(reflex-convert
    convert.cpp
)
target_link_libraries(reflex-convert PRIVATE reflex.serde_json reflex.serde_bson reflex.cli)

if (NOT REFLEX_CXX_MODULES_ENABLED)
    return()
endif()

add_executable(reflex-convert
    convert.cpp
)
target_link_libraries(reflex-convert PRIVATE reflex.serde.json reflex.serde.bson reflex.cli)

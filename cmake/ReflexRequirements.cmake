include(CPM)

if (REFLEX_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY doctest/doctest
        GIT_TAG v2.4.12
    )
    list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
endif()
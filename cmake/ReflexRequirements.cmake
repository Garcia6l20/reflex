include(CPM)

if (REFLEX_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY doctest/doctest
        GIT_TAG v2.4.12
        OPTIONS "DOCTEST_NO_INSTALL ON"
    )
    list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
    target_compile_features(doctest_with_main PUBLIC cxx_std_26)
endif()

#
# Check available headers
#
include(CheckIncludeFileCXX)

check_include_file_cxx("termios.h" REFLEX_HAVE_TERMIOS_H)
check_include_file_cxx("unistd.h"  REFLEX_HAVE_UNISTD_H)

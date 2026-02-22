#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is executed during module configuration.
# Integrates mod-arena-3v3-solo-queue unit tests with AzerothCore's test framework.
#

if (BUILD_TESTING)
    message(STATUS "Configuring mod-arena-3v3-solo-queue tests...")

    # Collect all test sources
    file(GLOB_RECURSE MODULE_TEST_SOURCES
        "${CMAKE_SOURCE_DIR}/modules/mod-arena-3v3-solo-queue/tests/*.cpp"
    )

    if(MODULE_TEST_SOURCES)
        # Register test sources with the global test runner
        set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES ${MODULE_TEST_SOURCES})

        # Expose the MatchmakingComposer header (header-only, no WoW server deps)
        set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
            "${CMAKE_SOURCE_DIR}/modules/mod-arena-3v3-solo-queue/src"
            "${CMAKE_SOURCE_DIR}/modules/mod-arena-3v3-solo-queue/tests"
        )

        list(LENGTH MODULE_TEST_SOURCES TEST_FILE_COUNT)
        message(STATUS "  +- Registered ${TEST_FILE_COUNT} test file(s) from mod-arena-3v3-solo-queue")
    else()
        message(STATUS "  +- No test files found in mod-arena-3v3-solo-queue/tests")
    endif()
endif()

set(SOURCES
    src/EncodeTest.cpp
    src/HashQuadtreeTest.cpp
    src/LifeRuleTest.cpp
    src/TopologyTest.cpp
    src/DummyAlgorithmTest.cpp
    src/PatternRegressionTest.cpp
    src/TestMain.cpp
)

if(GOLDE_BUILD_GUI)
    list(APPEND SOURCES src/ShaderParseTest.cpp)
endif()

add_executable(GOLTest ${SOURCES})

# Modern GTest discovery
include(GoogleTest)
gtest_discover_tests(GOLTest
    WORKING_DIRECTORY $<TARGET_FILE_DIR:GOLTest>
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 90
)

target_link_libraries(GOLTest PRIVATE
    gol_compiler_options
    GOLAlgoLib
    GOLIOLib
    GTest::gtest
)

if(GOLDE_BUILD_GUI)
    target_link_libraries(GOLTest PRIVATE GOLGraphicsLib)
endif()

add_custom_command(TARGET GOLTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/universes
        $<TARGET_FILE_DIR:GOLTest>/universes
)


if(UNIX AND NOT APPLE)
    # Linux: Link required system libraries
    target_link_libraries(GOLTest PRIVATE
        ${CMAKE_DL_LIBS}  # for dlopen, dlsym, etc.
        pthread           # for threading
    )
endif()
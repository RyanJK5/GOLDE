set(SOURCES
    src/EncodeTest.cpp
    src/HashQuadtreeTest.cpp
    src/TestMain.cpp
)

add_executable(GOLTest ${SOURCES})

# Modern GTest discovery
include(GoogleTest)
gtest_discover_tests(GOLTest
    WORKING_DIRECTORY $<TARGET_FILE_DIR:GOLTest>
)

target_link_libraries(GOLTest PRIVATE
    gol_compiler_options
    GOLCoreLib
    GOLGuiLib
    GOLGraphicsLib
    GOLAlgoLib
    GOLLoggingLib
    imgui_lib
    glew
    glfw
    nfd
    OpenGL::GL
    GTest::gtest
)

add_custom_command(TARGET GOLTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/universes
        $<TARGET_FILE_DIR:GOLTest>/universes
)


# Windows system libs
if(WIN32)
    target_link_libraries(GOLTest PRIVATE
        User32.lib 
        Gdi32.lib 
        Shell32.lib 
        ole32.lib
    )
elseif(UNIX AND NOT APPLE)
    # Linux: Link required system libraries
    target_link_libraries(GOLTest PRIVATE
        ${CMAKE_DL_LIBS}  # for dlopen, dlsym, etc.
        pthread           # for threading
    )
endif()
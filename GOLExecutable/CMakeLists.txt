add_executable(GOLExecutable Application.cpp)

target_link_libraries(GOLExecutable PRIVATE
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
)

if(UNIX AND NOT APPLE)
    # Linux: Link required system libraries
    target_link_libraries(GOLExecutable PRIVATE
        ${CMAKE_DL_LIBS}  # for dlopen, dlsym, etc.
        pthread           # for threading
    )
endif()

# 1. Set the Windows executable property only in Release
set_target_properties(GOLExecutable PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>
)

# 2. Fix the Entry Point for Windows (MSVC and Clang-cl)
if(WIN32)
    # look for 'main' instead of 'WinMain'.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(GOLExecutable PRIVATE 
            $<$<CONFIG:Release>:SHELL:-Xlinker /subsystem:windows -Xlinker /ENTRY:mainCRTStartup>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_link_options(GOLExecutable PRIVATE 
            $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
        )
    endif()
endif()

# Copy resources
add_custom_command(TARGET GOLExecutable POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:GOLExecutable>/resources
)

add_custom_command(TARGET GOLExecutable POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:GOLExecutable>/config
)

add_custom_command(TARGET GOLExecutable POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/presets
        $<TARGET_FILE_DIR:GOLExecutable>/presets
)
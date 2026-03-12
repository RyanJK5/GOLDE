add_executable(GOLExecutable Application.cpp)

target_link_libraries(GOLExecutable PRIVATE
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

# Windows system libs
if(WIN32)
    target_link_libraries(GOLExecutable PRIVATE
        User32.lib 
        Gdi32.lib 
        Shell32.lib 
        ole32.lib
    )
elseif(UNIX AND NOT APPLE)
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
    # This tells the linker: "Even though I'm a Windows subsystem app, 
    # look for 'main' instead of 'WinMain'."
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(GOLExecutable PRIVATE 
            $<$<CONFIG:Release>:-Xlinker;/ENTRY:mainCRTStartup>
        )
    else()
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
        ${CMAKE_CURRENT_SOURCE_DIR}/templates
        $<TARGET_FILE_DIR:GOLExecutable>/templates
)
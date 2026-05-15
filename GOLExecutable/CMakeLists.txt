if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set_source_files_properties(icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    add_executable(GOLDE src/Application.cpp icon.icns)

    set_target_properties(GOLDE PROPERTIES
        MACOSX_BUNDLE $<CONFIG:Release>
        MACOSX_BUNDLE_BUNDLE_NAME "GOLDE"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )
elseif(WIN32)
    add_executable(GOLDE src/Application.cpp resources.rc)

    set_target_properties(GOLDE PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
else()
    add_executable(GOLDE src/Application.cpp)
endif()

target_link_libraries(GOLDE PRIVATE
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
    target_link_libraries(GOLDE PRIVATE
        ${CMAKE_DL_LIBS}  # for dlopen, dlsym, etc.
        pthread           # for threading
    )
endif()

# 2. Fix the Entry Point for Windows (MSVC and Clang-cl)
if(WIN32)
    # look for 'main' instead of 'WinMain'.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(GOLDE PRIVATE 
            $<$<CONFIG:Release>:SHELL:-Xlinker /ENTRY:mainCRTStartup>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_link_options(GOLDE PRIVATE 
            $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
        )
    endif()
endif()

# Copy resources
add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:GOLDE>
)

add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:GOLDE>
)

add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Patterns
        $<TARGET_FILE_DIR:GOLDE>
)

# Install rules so CPack can package the app and so `make install` works
install(TARGETS GOLDE DESTINATION bin)

# Install runtime data (resources/config/patterns)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Patterns DESTINATION bin)

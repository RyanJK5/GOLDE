get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(IS_MULTI_CONFIG)
    set(GOLDE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/GOLExecutable/$<CONFIG>")
else()
    set(GOLDE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/GOLExecutable")
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set_source_files_properties(icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    add_executable(GOLDE src/Application.cpp icon.icns)

    set_target_properties(GOLDE PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "GOLDE"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )

    set(GOLDE_INSTALL_DATA_DIR "GOLDE.app/Contents/Resources")

    target_link_libraries(GOLDE PRIVATE "-framework CoreFoundation")
elseif(WIN32)
    add_executable(GOLDE src/Application.cpp resources.rc)

    set(GOLDE_INSTALL_DATA_DIR "share")

    set_target_properties(GOLDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${GOLDE_OUTPUT_DIR}/bin"
        WIN32_EXECUTABLE TRUE
    )
else()
    add_executable(GOLDE src/Application.cpp)

    set(GOLDE_INSTALL_DATA_DIR "share")

    set_target_properties(GOLDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:GOLDE>/bin"
    )
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

# Fix the Entry Point for Windows (MSVC and Clang-cl)
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

if(APPLE)
    set(GOLDE_RUNTIME_DATA_DIR "$<TARGET_BUNDLE_CONTENT_DIR:GOLDE>/Resources")
else()
    set(GOLDE_RUNTIME_DATA_DIR "$<TARGET_FILE_DIR:GOLDE>/share")
endif()

# Copy resources
add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${GOLDE_RUNTIME_DATA_DIR}/resources
)

add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${GOLDE_RUNTIME_DATA_DIR}/config
)

add_custom_command(TARGET GOLDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Patterns
        ${GOLDE_RUNTIME_DATA_DIR}/Patterns
)

# Install rules so CPack can package the app
if(APPLE)
    install(TARGETS GOLDE
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS GOLDE DESTINATION bin)
endif()

# Install runtime data (resources/config/patterns)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${GOLDE_INSTALL_DATA_DIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config DESTINATION ${GOLDE_INSTALL_DATA_DIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Patterns DESTINATION ${GOLDE_INSTALL_DATA_DIR})

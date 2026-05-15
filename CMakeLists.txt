cmake_minimum_required(VERSION 3.25)
project(GOLDE VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Modernized Compiler Options
include("cmake/CompilerOptions.cmake")

# Restrict to Debug and Release only
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Find OpenGL globally so all subprojects can use OpenGL::GL
find_package(OpenGL REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLX REQUIRED glx)
endif()


# Add dependencies first
add_subdirectory(Dependencies)
add_subdirectory(GOLLoggingLib)
add_subdirectory(GOLAlgoLib)
add_subdirectory(GOLGraphicsLib)
add_subdirectory(GOLGuiLib)
add_subdirectory(GOLCoreLib)
add_subdirectory(GOLExecutable)
add_subdirectory(GOLTest)


# CLANG FORMAT SETUP
include("cmake/ClangFormat.cmake")

# --- Basic CPack packaging configuration ---
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_NAME}")
set(CPACK_PACKAGE_CONTACT "Ryan Keane <keaner@rpi.edu>")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")

# Default generators: ZIP and tar.gz; platform-specific ones appended below
if(WIN32)
    set(CPACK_GENERATOR "ZIP;WIX")

    # Start Menu shortcut
    set(CPACK_PACKAGE_EXECUTABLES "GOLDE" "GOLDE")
    
    # Desktop shortcut
    set(CPACK_CREATE_DESKTOP_LINKS "GOLDE")

    set(CPACK_WIX_SKIP_PROGRAM_FOLDER ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
endif()

set(CPACK_WIX_VERSION "4") 
set(CPACK_COMPONENTS_ALL Unspecified)

include(CPack)
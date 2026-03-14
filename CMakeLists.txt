cmake_minimum_required(VERSION 3.25)
project(GameOfLife VERSION 1.0.0 LANGUAGES CXX)

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
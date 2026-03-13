![GOLDE](docs/logo.gif)

# Game of Life Designer and Engine (GOLDE)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.25+-blue.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform: Windows | Linux](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20-brightgreen.svg)

![Build Status](https://github.com/RyanJK5/GameOfLife/actions/workflows/build.yml/badge.svg)

## Overview

GOLDE is a high-performance Conway's Game of Life simulator with an intuitive GUI and an implementation of [HashLife](https://en.wikipedia.org/wiki/Hashlife) for jumping billions of generations instantly.

## Features

- **Interactive GUI**: Full-featured ImGui-based interface with intuitive controls
- **Simulation Control**: Play, pause, step, and adjust speed in real-time
- **Hyper Speed**: Jump any number of generations into the future using HashLife
- **Pattern Editor**: Create and edit patterns with all the quality of life features of a paint program
- **Customizable Shortcuts**: Edit keyboard shortcuts in real-time through [shortcuts.yml](GOLExecutable/config/shortcuts.yml)
- **Preset Library**: Pre-loaded classic Game of Life patterns
- **Multi-threaded**: Separation of concerns for maximum responsiveness
- **Cross-platform**: Windows and Linux support
- **Production Quality**: Comprehensive test suite with GTest

## Quick Start

### Prerequisites

- **C++23** compiler (MSVC, Clang, or GCC)
- **CMake 3.25+**
- **Ninja** (recommended) or other CMake-compatible build system
- **OpenGL 4.5+**

### Build and Run

```sh
# Configure (Ninja single-config)
cmake -B build -G Ninja -D CMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Launch application
./build/GOLExecutable/GOLExecutable.exe
```

For **multi-config generators** (Visual Studio, Xcode):
```sh
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/GOLExecutable/Release/GOLExecutable.exe
```
## Usage Guide

### Launching the Application
Start GOLDE and you'll be greeted with an interactive grid and control panel.

### Controls
- **Draw** cells to the screen by dragging the cursor
- **Select** regions of the grid by holding shift and dragging.
- **Edit** the selection by pressing copy, cut, rotate, flip, or any other command visible in the sidebar.
- **Shortcuts** can be found by hovering over any button, and can be customized by editing [shortcuts.yml](GOLExecutable/config/shortcuts.yml).
- **Open patterns** from the built-in library by selecting a preset from the bottom window.
- **Select an algorithm** from the dropdown in the sidebar.
- **Start** the simulation, and freely pause, restart, or stop your simulation.
- **Step** billions of generations by inputting any value (<2^63) into the "Step Count" field.
- **Save and Load** your universe for future use.
- **Customize** your window setup using ImGUI's docking features

## Project Structure

```
GameOfLife/
├── GOLAlgoLib/          # Core algorithms (HashLife, SparseLife)
├── GOLCoreLib/          # Game state & orchestration
├── GOLExecutable/       # Application entry point
├── GOLGraphicsLib/      # OpenGL rendering engine
├── GOLGuiLib/           # ImGui-based UI
├── GOLLoggingLib/       # Logging system
├── GOLTest/             # Unit and integration tests
├── Dependencies/        # Third-party libraries (ImGui, FontAwesome)
├── cmake/               # CMake utilities and compiler options
└── docs/                # Documentation and assets
```

### Dependencies
- [OpenGL 4.5+](https://www.opengl.org/)
- [ImGui](https://github.com/ocornut/imgui)
- [GLFW](https://www.glfw.org/)
- [GLEW](https://glew.sourceforge.net/)
- [GLM](https://github.com/g-truc/glm)
- [GoogleTest](https://github.com/google/googletest)
- [unordered_dense](https://github.com/martinus/unordered_dense/)
- [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended)

## Development

### Code Standards
- **Language**: C++23 with modern idioms
- **Code Format**: Clang-format (run with `cmake --build build --target Format`)
- **Testing**: Comprehensive GTest suite
- **Compilation**: Warnings enabled and treated as errors

### Running Tests Locally

```sh
# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
ctest --test-dir build -R TestName --output-on-failure

# Run with verbose output
ctest --test-dir build --verbose
```

## Gallery

*Add GIF demonstrations here:*
- ***HashLife Performance Demo*** — millions of generations advancing in seconds
- ***Pattern Editor in Action*** — creating and modifying patterns
- ***Interactive Navigation*** — zooming and panning across the grid

## License

This project is licensed under the terms of the [MIT license](LICENSE).
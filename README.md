![GOLDE](docs/logo.gif)

# Game of Life Designer and Engine (GOLDE)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.25+-blue.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform: Windows | Linux | macOS](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20-brightgreen.svg)

![Build Status](https://github.com/RyanJK5/GameOfLife/actions/workflows/build.yml/badge.svg)

## Overview

GOLDE is a high-performance Conway's Game of Life simulator with an intuitive GUI and an implementation of [HashLife](https://en.wikipedia.org/wiki/Hashlife) for jumping billions of generations instantly.

## Features

- **Interactive GUI**: Full-featured interface with intuitive controls
- **Simulation Control**: Play, pause, step, and adjust speed in real-time
- **Hyper Speed**: Jump any number of generations into the future using HashLife
- **Customizable Rules**: Experiment with outer-totalistic rules and toroidal topologies
- **Pattern Editor**: Create and edit patterns with all the quality of life features of a paint program
- **Customizable Shortcuts**: Edit keyboard shortcuts in real-time through [shortcuts.yml](GOLExecutable/config/shortcuts.yml)
- **Preset Library**: Pre-loaded classic Game of Life patterns
- **Multi-threaded**: Separation of concerns for maximum responsiveness
- **Cross-platform**: Windows, Linux, and macOS support
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
./build/GOLExecutable/GOLDE.exe
```

For **multi-config generators** (Visual Studio, Xcode):
```sh
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/GOLExecutable/Release/GOLDE.exe
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
- **Start** the simulation, and freely pause, restart, or stop your simulation.
- **Step** billions of generations by inputting any value into the "Step Count" field.
- **Save and Load** your universe for future use.
- **Customize** your window setup using ImGUI's docking features

## Gallery

![](docs/breeder.gif)


A simple breeder pattern running for over 10^1000 generations using HashLife.

![](docs/turing_machine.png)


A turing machine in GOLDE.

![](docs/editor.gif)


A demonstration of basic GOLDE editing features.

![](docs/life_in_life.gif)


Game of Life simulating itself in GOLDE.

## What's Next?

GOLDE will continue receiving updates and is moving towards the goal of supporting modular extensions to the editor. In order of priority, the next major features that GOLDE is targeting are:
1. Support for multi-state rules
2. Support for Python and Lua scripting
3. Robust extension support for adding rules, algorithms, topologies, and data structures (both in C++ and Lua)
4. Support for making additions and modifications to the UI through extensions, such as additional widgets

## License

This project is licensed under the terms of the [MIT license](LICENSE).
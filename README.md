![GOLDE](docs/logo.gif)

# Game of Life Designer and Engine (GOLDE)

## Build, Test, and Run

**Requirements:**
   - C++23 compiler
   - CMake 3.25
   - Build system (Ninja is recommended)

### Configure

For Ninja, choose the build type at configure time:

```sh
cmake -B build -G Ninja -D CMAKE_BUILD_TYPE=Release
```

If you are using a multi-config generator instead of Ninja, do not set `CMAKE_BUILD_TYPE`. Configure with the generator you want and choose the configuration at build time with `--config`.

Example:

```sh
cmake -B build -G "Ninja Multi-Config"
```

### Build

Build the project with:

```sh
cmake --build build
```

For multi-config generators, specify the configuration explicitly:

```sh
cmake --build build --config Release
```

### Run Tests

After building, run the test suite with CTest:

```sh
ctest --test-dir build --output-on-failure
```

For multi-config generators, specify the configuration:

```sh
ctest --test-dir build -C Release --output-on-failure
```

### Run The Application

With a single-config generator such as Ninja, the executable is produced at:

```sh
build/GOLExecutable/GOLExecutable.exe
```

With a multi-config generator, the executable is produced under the selected configuration directory, for example:

```sh
build/GOLExecutable/Release/GOLExecutable.exe
```
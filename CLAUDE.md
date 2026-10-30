# GOLDE

Editor and simulator for Conway's Game of Life and other Life-like cellular automata, built on HashLife. C++23, CMake, OpenGL/ImGui front end.

**Current plan:** `docs/roadmap.md`. Read it before starting any phase work; check the "Order of work" table for what is next.

## Build and test

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel
```

- Headless (algorithm, IO and tests only; no OpenGL/X11/ImGui needed): add `-DGOLDE_BUILD_GUI=OFF`.
- Compilers: GCC 14 (Linux; CI pins it, older GCC may lack needed C++23 features), LLVM Clang with `-stdlib=libc++` (macOS), MSVC (Windows).
- Dependencies come from FetchContent (`Dependencies/CMakeLists.txt`); the first configure needs network access.
- Format before committing: `cmake -DPROJECT_SOURCE_DIR="$(pwd)" -P cmake/RunFormat.cmake`. CI fails on any formatting diff.
- Tests are GoogleTest in `GOLTest/`, run from the binary directory; test patterns live in `GOLTest/universes/`.
- Shipped patterns are in `GOLExecutable/Patterns/` (RLE and Macrocell).

## Library layout

| Library | Role | Notes |
| --- | --- | --- |
| `GOLAlgoLib` | HashLife, `HashQuadtree`, `LifeNode`, rules, topologies, `GameGrid`, geometry types (`Graphics2D.hpp`) | Headless; no GUI headers |
| `GOLIOLib` | `PatternIO` (RLE and Macrocell read/write) | Headless; depends only on GOLAlgoLib |
| `GOLGraphicsLib` | OpenGL renderer, camera, shaders, `GL_DEBUG` (`GLDebug.hpp`), ImGui/glm conversions (`GraphicsInterop.hpp`) | GUI only |
| `GOLGuiLib` | ImGui widgets, `VersionManager`, `SelectionManager` | GUI only |
| `GOLCoreLib` | App layer: `EditorModel`, `SimulationWorker`, `SimulationEditor`, presets | GUI only |
| `GOLExecutable` | `main`, resources, patterns | GUI only |

Key algorithm files: `HashLife.hpp/.cpp` (`AdvanceFast`, `AdvanceSlow`, base cases), `HashLifeCache.hpp/.cpp` (node table, arena, slow cache, GC), `LifeNode.hpp/.cpp`, `HashQuadtree.hpp/.cpp`, `Torus.cpp`, `Plane.cpp`.

## Code style

- Follow `.clang-format` (LLVM base, 4-space indent, left-aligned `*`/`&`).
- PascalCase for types, methods, public fields and constants; camelCase for locals and parameters; `m_` members, `s_` statics. Lowercase STL-style names only where required (`begin`, `end`, `emplace`).
- Header guards `Name_hpp_`, everything in `namespace Golde`.
- Prefer modern C++23: `std::expected` for errors, `std::optional` for absence, designated initializers, ranges, `std::print`/`std::format`, `std::jthread` + `std::stop_token`, `constexpr` where practical.

## Verification against Golly

Golly's command-line `bgolly` is the correctness oracle. Build it **outside this repo** (Golly is GPL-2; GOLDE is MIT, so never copy, translate line by line, or vendor Golly code):

```sh
# in an unpacked Golly source tree
g++ -O2 -w -DZLIB -Igollybase cmdline/bgolly.cpp gollybase/*.cpp -lz -o bgolly
./bgolly -h -m 2048 -o out.mc path/to/breeder.rle   # prints "gen: pop" lines
```

Known-good reference: breeder.rle reaches population 11,260 at generation 2,048 (also 3,963 at gen 1, 6,297 at gen 1,024).

Golden test values must come from bgolly or published sources. **Never change an expected value to make a test pass**; if GOLDE disagrees with bgolly, GOLDE is wrong until proven otherwise.

## Traps

- **Loading a file gives a bounded grid.** `PatternIO::ReadRegion` returns a `GameGrid` sized to the RLE's `x`/`y`, with a bounded `Plane` topology, because the app pastes it as a selection. Stepping it directly gives plausible but wrong populations. To simulate, wrap the data in an unbounded grid: `GameGrid grid{res->Grid.Data(), Size2{0, 0}};` then `grid.SetRule(...)`.
- **The garbage collector is dead code today.** `GOLDE_GARBAGE_COLLECTION` is never defined, the code does not compile with it (`MarkAndSweep(m_Root)` references a removed member), and `HashQuadtree.hpp` misspells the macro twice. Phase 1 replaces it; don't patch it in place.
- **One `HashLifeCache` is shared across threads** (editor, undo history, simulation worker, preset loader) with no synchronization. Don't add new cross-thread uses.
- **`s_PopulationCache` is `static thread_local`** and keyed by node pointer; it goes stale if nodes are ever freed.
- **Rule lives in the cache, the rule string in the grid**; they can disagree after undo.

## Working agreements

- Small, focused commits; one logical change each. Every bug fix comes with a regression test.
- Run the build, tests and formatter before saying a change is done. Report what was verified and how.
- Each roadmap phase has a **Gate**; a phase is done only when its gate passes.
- Ask before changing public interfaces outside the phase being worked on.
- Keep replies short. Give the answer or recommendation and leave out detail the user can ask for.
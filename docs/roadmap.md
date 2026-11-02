# GOLDE Roadmap: GC, CLI, Tests, Rokicki Optimizations, Renderer

Last updated 2026-10-29 (population cache redesign added to Phase 1). Baseline: GOLDE `master` at `12bb980`, Golly source `dcbc0873`.

## Order of work

| Step | Phase | Size | Status |
| --- | --- | --- | --- |
| 1 | 0 · Headless algorithm layer | ~1 day | **next** |
| 2 | 2a · Thin `golde-cli` + bgolly oracle + ~12 golden tests | 2–3 days | |
| 3 | `CLAUDE.md` refresh (build/oracle commands become real) | <1 hour | |
| 4 | 1 · `NodeStore` + garbage collection | 1–2 weeks | |
| 5 | 2b · Full CLI (`script`, `compare`, `bench`, `.lif`/`.cells`) | 2–3 days | |
| 6 | 3 · Test suite expansion | ~1 week, then ongoing | |
| 7 | 4 · Torus as a tiled universe | 1–2 weeks | |
| 8 | 5 · Exact steps in one jump, `(node, advance)` cache | 1–2 weeks | |
| 9 | 6 · Lockstep HashLife, single-threaded | 2–3 weeks | |
| 10 | 7 · Parallel lockstep | 2–4 weeks | |
| — | 8a/8b · Renderer (parallel track once Phase 1 adds the traversal lock) | ~2 weeks | |
| — | 1b · `uint64_t` 8x8 leaves (optional) | | |

Phases 4 and 5 are independent of each other; 6 needs 5; 7 needs 6. The CLI was split so the oracle loop exists *before* the riskiest change (GC). The thin CLI talks to a small `Universe` facade (load, step, population, digest) so Phase 1's API changes stay contained.

## Findings that motivated the plan

Measured by building GOLAlgoLib headless (GCC 14.2, `-DGOLDE_GARBAGE_COLLECTION` with minimal compile fixes) and comparing with `bgolly` built from Golly source.

| Run (breeder.rle, unbounded) | Without GC | With GC (threshold 50k nodes) | Golly |
| --- | --- | --- | --- |
| Populations gen 1 to 2,048 | Match Golly exactly (11,260 at 2,048) | — | 11,260 |
| Hyperspeed, 117 jumps | 24 ms total, 92k nodes | 11.2 s, 944 collections (thrashing) | — |
| Fixed step 1,000 | 353 ms for 400 jumps | Segfault on jump 0 in `CenteredSubNode` under `AdvanceSlow` | — |
| Fixed step 1,024 | 22 ms per 200 jumps | Correct, 1 collection | — |

At the real 1M threshold the breeder never collects (it peaks below 300k nodes). The GC as committed does not compile: `HashLifeCache::FindOrCreate` calls `MarkAndSweep(m_Root)`, but `m_Root` moved out of the cache in #33, and `HashQuadtree.hpp` spells the macro `GOLDE_GARBAGE_COLLCETION` and `GOLDE_GARBGE_COLLECTION`.

Root causes:

1. **Unrooted intermediates.** `AdvanceSlow` builds four windows, then recurses on `window00`; a collection inside that recursion frees `window01..11` (the segfault). Golly avoids this class entirely: every `find_node` pushes onto a root stack (`save`) and `getres` pops back to its saved stack pointer (`hlifealgo.cpp:198-327, 1398-1421`).
2. **Weak tables not swept.** `SlowCache` keys/values, `EmptyNodeCache` below the top slot, and the `thread_local s_PopulationCache` keep pointers to freed, then recycled, nodes.
3. **Fixed node-count trigger.** Once the live set nears 1M, every insert collects. Golly triggers on a memory budget (`maxmem`) only when the free list is empty, and collects before each hash resize.
4. **Free-list corruption (latent).** `LifeNodeArena::SweepGarbage` re-appends nodes already on the free list (free nodes are never marked); a second collection before reuse builds a cycle.
5. **No root set.** One cache is shared by the editor grid, undo/redo history, selection, async command copies and the simulation worker, but GC sees one root.

**Licensing:** GOLDE is MIT, Golly is GPL-2. Use Golly to understand algorithms and as a test oracle; write GOLDE's code fresh. Never copy or line-by-line translate Golly code, and never vendor Golly source into this repo.

## Architecture: what to refactor and when

The central problem: `HashLifeCache` is a shared, unsynchronized bag of nodes with no owner, no root set and no rule about which thread may write it. GC, lockstep, parallelism and the renderer all need that owner.

| Problem | Evidence | Refactor | Phase |
| --- | --- | --- | --- |
| Algorithm layer can't build headless | `GOLAlgoLib` links `GOLGraphicsLib` (cycle) only for glm/ImGui conversions in `Graphics2D.hpp:134-180`; `Logging.hpp:10` includes GLEW; RLE/MC parsing lives in `GOLGuiLib/misc/FileFormatHandler` | Split `Graphics2D.hpp`; move `GL_DEBUG` to GraphicsLib; new `GOLIOLib`; `GOLDE_BUILD_GUI` option | 0 |
| Shared cache has no owner | Preset loader jthread inserts into `PresetSelection::m_Cache` while the main thread encodes from it; selection edits run during Stepping on the worker's cache (`SimulationEditor.cpp:156-216`) | `NodeStore` with a single-writer rule (`WriteToken`, asserted in debug); readers take a shared traversal lock | 1 |
| GC has no root set | Roots in `EditorModel`, executor, `VersionManager` stacks, `SelectionManager`, async futures, `SimulationWorker` grids | `RootHandle` RAII registration in every `HashQuadtree`, plus a root stack for in-flight recursion | 1 |
| Weak tables outlive nodes | `SlowCache`, `EmptyNodeCache`, `thread_local s_PopulationCache` (`HashQuadtree.cpp:25-27`; never cleared, returns references a later insert can invalidate) | All caches owned by `NodeStore`, swept or cleared by GC; population table redesigned (Phase 1 item 6) | 1 |
| Node representation leaks everywhere | Raw `const LifeNode*` in editor, renderer, IO | `NodeRef` handle (a pointer for now) so indices can be benchmarked later | 1 |
| Step state lives on the algorithm object | `m_StepData`, `m_StepStopToken`, `m_StepAdvanceDepth` (`HashLife.hpp:68-70`); `GameGrid` copies clone the algorithm ~3x per frame | Stateless `HashLife` + per-call `StepContext` | 5 |
| Rule per cache, rule string per grid | Undo across a rule change restores the old string but steps with the new rule (`SelectionManager.cpp:329`) | Rule stored with the universe; result cache keyed by a rule epoch | 1 |
| Topology is ghost-cell hooks over `dynamic_cast` | `Plane.cpp:38`, `Torus.cpp:34,99`; bounded grids step 1 generation per jump | `BuildRoot` / `ExtractUniverse` | 4 |
| 32-bit public coordinates | `FindBoundingBox` returns zeros past depth 31; `Get`/`begin()` see only the central 2^31 window | BigInt-first queries | 2 |
| Cell-level leaves | Levels 0–2 are pointer nodes; `EncodeLevel3` chases ~21 pointers per base case | 8x8 leaves as one `uint64_t` at level 3 | 1b |

Keep the `LifeAlgorithm` / `LifeDataStructure` / `Topology` split for extensibility, but give HashLife-specific topologies a typed `HashQuadtree&` interface. Optional low-priority rename: `GOLCoreLib` → `GOLAppLib`.

## Phase 0: headless algorithm layer (~1 day)

A scratch build only needed an empty `GL/glew.h` stub, so this is small.

1. Split `Graphics2D.hpp`: keep `GenericVec`, `GenericRect`, `Vec2`, `Vec2L`, `Rect`, `RectL`, `Color` in GOLAlgoLib; move `Vec2F`/`Vec2D`/`Size2F`/`RectF` and their ImGui/glm conversions to `GOLGraphicsLib/include/GraphicsInterop.hpp`.
2. Move `GL_DEBUG` and `LogGLErrors` out of `Logging.hpp` into GraphicsLib; drop `glew` from `GOLLoggingLib`.
3. Remove the `GOLGraphicsLib` link from `GOLAlgoLib/CMakeLists.txt`; fix the copy-paste `target_include_directories(GOLAlgoLib ...)` at `GOLGraphicsLib/CMakeLists.txt:16`; add `LifeNode.hpp` and `BigInt.hpp` to `HEADERS`.
4. New `GOLIOLib` holding `FileFormatHandler` (renamed `PatternIO`), depending only on GOLAlgoLib.
5. Root `CMakeLists.txt`: `option(GOLDE_BUILD_GUI ON)`; wrap `find_package(OpenGL/X11/GLX)` and the GUI subdirectories in it. glm/ImGui/GLFW FetchContent should also be skipped when the GUI is off.
6. CI: add a headless Linux job (`-DGOLDE_BUILD_GUI=OFF`) building GOLAlgoLib, GOLIOLib, the CLI and algorithm tests.

**Gate:** `cmake -DGOLDE_BUILD_GUI=OFF` configures and builds on a machine with no GL headers; the GUI build and existing tests are unchanged.

## Phase 1: garbage collection that works (1–2 weeks)

Adopt Golly's GC model, expressed in GOLDE's types:

1. **`HashLifeCache` becomes `NodeStore`** (`GOLAlgoLib/include/NodeStore.hpp`), owning the arena, unique table, empty nodes, root stack, root registry, all weak caches and statistics.
2. **Root stack replaces `ProtectNodeFromGC` guards.** `FindOrCreate` pushes every node it returns. Recursive functions use a `RootScope` RAII helper: record the mark on entry, pop on exit, save the result. Covers `AdvanceSlow`'s windows, `Centered*` results and `DecodeLevel2` temporaries.
3. **`RootHandle`** in every `HashQuadtree`: registered on construction/copy, removed on destruction. GC marks every registered root.
4. **Memory-budget trigger.** Collect when the free list is empty and the next block would exceed `MaxMemory`, and before the unique table rehashes. After a collection the next trigger is max(budget, 2 x live bytes). Collection only while a `Step` holds the write token; editor operations never collect.
5. **Sweep rebuilds, never appends.** Rebuild the unique table and the free list from scratch. Mark every non-null `EmptyNodeCache` level. Erase weak-cache entries with unmarked keys (first version: clear partial-result caches entirely).
6. **Replace `s_PopulationCache`.** Today it is `static thread_local`, never cleared (memory grows with every new root counted), and `Population()` returns a reference into a vector-backed `ankerl::unordered_dense::map`, which the next insertion can invalidate (`SelectionManager::SelectedPopulation()` and `EditorModel::SelectedPopulation()` pass that reference on). Replacement:
    - `NodeStore` owns one population table, swept by GC, so unchanged subtrees are reused across frames.
    - Levels below 32 store a `uint64_t` (a level-31 node holds at most 2^62 cells); only levels 32 and up use a small BigInt side map.
    - `HashQuadtree` caches its root's population with a valid flag cleared when the root changes, and `Population()` returns `BigInt` by value. No references into the table escape.
    - The simulation worker computes the population of each snapshot it publishes and sends it with `{root, level, origin, generation, population}` (the 8a snapshot struct), so the main thread never writes the store while a simulation runs.
    - Benchmark later: a `uint64_t` count on `LifeNode` itself (56 → 64 bytes, one cache line, nothing to sweep below level 32), weighed against the smaller-node index experiment.
    - For reference, Golly's `calcpop` (`hlifealgo.cpp:1311`) hangs BigInts on nodes temporarily and keeps only the total, recounting whenever the root changes; too slow for GOLDE's per-frame display.
7. **Threading.** GC only on the writer thread, holding the traversal lock exclusively; renderer and editor take it shared. Never call `SimulationWorker::Stop()` while holding it.
8. **Delete the `#ifdef`.** GC always compiled; `MaxMemory = 0` means unlimited.

Debug tooling: `GOLDE_GC_STRESS=N` (collect every N allocations during `Step`), `GOLDE_GC_POISON` (overwrite freed nodes so stale pointers fault immediately), `NodeStore::Verify()`.

**Gate:** with `GOLDE_GC_STRESS=64` + poison, the whole suite passes; breeder, spacefiller and `metapixel-galaxy.mc` run 10^6 generations at steps of 1,000 and 1,024 (10^4 at step 1) within a 64 MB budget, populations matching `bgolly -M 64`; hyperspeed breeder at 64 MB within 2x of no-GC time.

## Phase 2: `golde-cli` (2a thin: 2–3 days; 2b full: 2–3 days)

New `GOLCli` executable linking only GOLAlgoLib and GOLIOLib, modeled on Golly's `bgolly` but built for automated checking. CLI11 via FetchContent.

**2a (thin, before Phase 1):** `run <pattern> --rule --step N --hyper --until GEN --jumps K --json --expect-pop P`, exit 1 on mismatch; plain output prints bgolly-style `gen: pop` lines. `tools/oracle.py` runs both and diffs. ~12 golden tests (breeder, R-pentomino, acorn, one torus, HighLife, Day & Night). **Patterns are loaded into an unbounded universe by default** (see CLAUDE.md trap).

**2b (full):** `script <file|->` (`load`, `rule`, `step N`, `hyper K`, `set x y`, `get x y`, `bbox`, `expect-pop`, `expect-gen`, `expect-digest`, `save`, `gc`, `stats`); `compare <a> <b>`; `bench` (breeder hyperspeed, metapixel-galaxy step 1,024, spacefiller step 1,000, acorn to 5,206) reporting wall time percentiles, peak nodes, collections; `--max-memory`, `--engine recursive|lockstep`, `--threads`, `--output .rle|.mc`, `--stats`. `.lif` and `.cells` readers.

JSON: one object per line, BigInts as strings, e.g. `{"gen":"2048","pop":"11260","nodes":67320,"liveBytes":5308416,"collections":0,"ms":14.2}`. `expect-digest` hashes the canonical macrocell serialization so it survives node-layout changes.

**Gate:** `tools/oracle.py breeder.rle --step 1000 --until 100000` reports zero mismatches; `golde-cli bench` runs headless in CI and uploads its JSON.

## Phase 3: test suite (~1 week, then ongoing)

Split `GOLTest` into `GOLAlgoTest` (headless) and `GOLAppTest`. ctest labels `fast` (every push) and `slow` (nightly). Golden values in `GOLTest/golden/*.json`, recording the bgolly version and command.

| Area | Tests | Oracle |
| --- | --- | --- |
| Methuselahs | R-pentomino gen 1,103 pop 116; acorn 5,206 / 633; blom 23,314 / 2,740; iwona 28,786 / 3,091; rabbits to 17,331 | Golly Lexicon/pattern comments, confirmed with bgolly |
| HashLife stress | breeder, spacefiller, `metapixel-galaxy.mc`, `hashlife-oddity1/2.mc`, `totalperiodic.mc`, `ruler.mc`, `unlimited-novelty.mc`, `wedge-grow.mc` at 5–10 checkpoints | bgolly |
| Step-schedule equivalence | Random 64x64 soups: one step of N = N steps of 1 = random splits of N; hyperspeed checkpoints = fixed-step results | digest |
| Other rules | HighLife replicator, Day & Night gun, LongLife, B1357/S1357, Morley, Seeds | bgolly; Day & Night inversion symmetry |
| Bounded and torus | glider on WxW torus returns after 4W gens; `agar-p3`, `herringbone-agar-p14`, `pulsars-in-tube`, `ice-nine` | bgolly |
| Garbage collection | Suite under stress + poison; undo history survives 100 collections; rule change after collection | digest |
| File formats | RLE and Macrocell round-trip for all shipped patterns; `#CXRLE Pos`; malformed input returns `DecodeError` | round-trip |
| Concurrency | TSan: worker start/stop/step with concurrent snapshot reads; stop latency < 50 ms during a 2^40 jump | TSan |

Also: ASan + UBSan jobs; delete the no-op `CopyingBreeder`; use or remove `tt.rle`, `r_pentomino.rle`, `squiggles3/4.rle`; B0 rules explicitly rejected until supported.

**Gate:** table green in CI; every bug fixed from Phase 1 on lands with a regression test.

## Phase 4: torus as a tiled universe (1–2 weeks)

Rokicki's trick: a W x H torus is the infinite plane tiled with copies of one tile; canonicalization makes the copies nearly free, so HashLife jumps at full speed. Today `Torus::Log2MaxIncrement` returns 0 and `PrepareBorderCells` does per-cell `Get`/`Set` each generation.

- **Power of 2 (W = H = 2^p):** tile T at level p; tower `L(p+1) = FindOrCreate(T,T,T,T)`, `L(k+1) = FindOrCreate(L(k) x4)`. Advancing a level-k tower node (k ≥ p+2) yields a uniform result; its NW level-p descendant is the new tile.
- **General W x H:** a node at corner (x, y) depends only on (x mod W, y mod H). Build lazily with a memo keyed on (level, x mod W, y mod H). At level l there are at most (W / gcd(W, 2^l)) x (H / gcd(H, 2^l)) distinct nodes (9 per level for 1536). Choose the root so the result contains an aligned period, then extract the tile.
- **Interface:** replace `PrepareBorderCells` / `CleanupBorderCells` / `Log2MaxIncrement` with `BuildRoot(NodeStore&, const HashQuadtree&, const BigInt& advance) -> PreparedRoot`, `ExtractUniverse(result, PreparedRoot)`, `std::optional<BigInt> MaxAdvancePerJump()`.
- **Order:** 4a power-of-2 square; 4b arbitrary W x H; 4c one-axis wrap (`T0,68`). Shifted torus, Klein bottle, cross-surface out of scope.

**Gate:** glider on a 1024 x 1024 torus returns after 2^40 generations; `agar-p3`, `herringbone-agar-p14`, `ice-nine` match bgolly at 10^5; 1000 x 1000 soup torus runs 10^6 generations ≥100x faster than ghost cells.

## Phase 5: exact steps in one jump (1–2 weeks)

Any step N becomes one jump landing exactly on N; each level sees at most two advance values, so the cache stays hot. Replaces `AdvanceSlow`, `SlowKey`/`SlowCache`, `m_StepAdvanceDepth`, `Log2MaxIncrement` and the power-of-2 loop in `HashLife::Step`.

`AdvanceBy(node, k, t)` for 0 ≤ t ≤ F(k) = 2^(k-2), half = F(k-1):

1. t = F(k): full advance, cached in `node->AdvanceResult`.
2. t = 0: centered subnode.
3. Otherwise t1 = half if t ≥ half else 0, t2 = t - t1. Stage 1 advances the 9 overlapping level-(k-1) nodes by t1 (t1 = 0 → centered subnodes, like Golly's `dorecurs_half`). Stage 2 assembles 4 nodes and calls `AdvanceBy(..., k-1, t2)`.
4. Level 3: t = 2 `AdvanceBase`, t = 1 `AdvanceBaseOneGen`, t = 0 center.

Expand the root until it holds the pattern and F(k) ≥ N, then one call. For N = 1,000: levels ≤5 advance 8; level 6 sees 16 and 8; level 7, 32 and 8; level 8, 64 and 40.

Partial results go in `NodeStore::PartialResults` (`ankerl::unordered_dense::map<PartialKey, NodeRef>`, `PartialKey{NodeRef Node; uint64_t Advance;}`); steps ≥ 2^63 run the high part as full jumps. GC v1 clears it. Benchmark Rokicki's alternative (second result pointer + tag on the node). Move step state into a `StepContext`; `HashLife` keeps only rule and topology.

**Gate:** breeder and `metapixel-galaxy.mc` match bgolly for 200 random N in [1, 10^7]; step 1,000 on the breeder is one jump per step and ≥5x faster than 353 ms / 400 jumps; ≤2 distinct advances per level per N.

## Phase 6: lockstep HashLife, single-threaded (2–3 weeks)

`LockstepHashLife` computes the same results but processes every pending node at one (time, level) together. A `Batch` holds level, advance, deduplicated inputs, results and per-miss intermediates. Explicit batch stack:

1. Look up all inputs; hits done.
2. For misses, build 9 x misses stage-1 nodes, dedupe, push child batch (level k-1, advance t1).
3. On return, assemble 4 stage-2 nodes per miss, dedupe, push child batch (advance t2).
4. On return, assemble results, write cache, pop.

Batch lookups can prefetch hash buckets (Golly's `USEPREFETCH`, `hlifealgo.cpp:323-340`); try per-level unique tables behind a `NodeStore` option. GC happens only between batches; batch vectors are the roots. Record batch-width histograms per level (`--stats`) — they decide whether GPU work is ever worth it.

**Gate:** `golde-cli run --engine lockstep --verify-with recursive` gives identical roots on the Phase 3 suite; bench comparison published (within 1.5x acceptable).

## Phase 7: parallel lockstep (2–4 weeks)

Start with an in-house `std::jthread` + `std::barrier` pool behind a `BatchExecutor` interface (serial backend too). Lookup phase: read-only, parallel. Insert phase: dedupe first, then partition new nodes by hash bits into shards, each thread owning whole shards and its own arena. Compute phase: data-parallel loops. Kernels index-addressed, no allocation, no virtual calls.

| Backend | Notes |
| --- | --- |
| `std::jthread` + `std::barrier` | Recommended first; no dependency |
| oneTBB | Work stealing if batches are unbalanced |
| OpenMP | MSVC defaults to 2.0; Apple Clang lacks `libomp` |
| Kokkos | Only if GPUs become a goal; no Apple GPU backend; nodes must become indices |

**Gate:** speedup curve at 1/2/4/8 threads for `metapixel-galaxy.mc` (step 1,024) and breeder (step 1,000); digests identical to serial; TSan clean.

**GPU note:** before any GPU work, check Phase 6 batch widths for target patterns. If most batches are under ~10k entries, a GPU is unlikely to win. GPU requires pointers → indices, a fixed-capacity CAS hash table rehashed between batches, GC on device, and a host/device split for editing and rendering.

**Indices note:** benchmark per-level arrays with 32-bit indices (node ~24 bytes vs 56 today) behind `NodeRef` once Phase 6 exists; keep only if >~20% faster.

## Phase 8: renderer (~2 weeks, parallel track)

**8a quick wins:** reallocate the FBO/state texture only on resize (`RescaleFrameBuffer` runs every frame, `SimulationEditor.cpp:115`); pass `{root, level, origin, generation, population}` snapshots instead of `GameGrid` copies; `GL_LINEAR` or max-reduction when minifying (cells currently flicker/vanish zoomed out); keep partly visible edge blocks (`GraphicsHandler.hpp:205`).

**8b Golly-style traversal** (`hlifedraw.cpp:140-376`), written fresh:

1. `Viewport` with BigInt center, integer `Mag`, screen size.
2. `DrawQuadtree(root, level, origin, viewport, renderer)`: descend while a node is wider than 2x the screen, then switch to `int32` coordinates.
3. Empty-node checks only; at 128-px nodes draw children into a 256 x 256 one-bit tile and blit; 1-px nodes set one pixel; level-3 leaves use 4x4 tables like `draw4x4_*`/`compress4x4`.
4. `LifeRenderer::BlitTile(Rect, std::span<const uint8_t>)`; GL writes tiles with `glTexSubImage2D` into a persistent viewport-sized texture.

Zoom: keep continuous zoom by rendering at `Mag = floor(log2(px per cell))` and scaling the quad by the 1–2x residual. `Camera.Center` becomes BigInt cell + sub-cell offset. Grid lines, selection, floating selection and bounded clipping move to `Viewport`. Renderer holds the traversal lock shared during a draw.

**Gate:** `metapixel-galaxy.mc` at 1:1 and fit-to-screen, and a 4096 x 4096 soup at 1:1, each < 4 ms/frame via `golde-cli render --bench` into a null renderer.

## Risks

- **GC across threads** — single-writer assertion, GC only on writer thread, stress/poison modes, TSan before Phase 1 is done.
- **Root-stack discipline** — `RootScope` in every function calling `FindOrCreate` more than once.
- **Lockstep memory** — cap batch size, split large batches.
- **Torus extraction off-by-one** — glider-return test, bgolly at odd and non-square sizes.
- **Licensing** — see above.

## Open questions

- [ ] Default GUI memory budget: fixed or fraction of RAM?
- [ ] Is 1b (`uint64_t` leaves) in scope now?
- [ ] Zoom: continuous with residual scale (recommended) or power-of-2?
- [ ] Is GPU support a real goal?
- [ ] Does the CLI ship in release packages?

## Side goal: low-latency practice

Measure like a latency team: pinned threads, turbo off, warmup, percentiles, `perf stat` counters. Concrete exercises: lock-free SPSC snapshot handoff from worker to renderer (seqlock or triple buffer) with p50/p99 pickup latency; stop latency during huge jumps; huge pages for the node arena (`madvise(MADV_HUGEPAGE)`, compare dTLB misses); zero allocations during `Step` verified with a counting `operator new`; own open-addressing table vs `ankerl`; false sharing with/without `alignas(64)`.
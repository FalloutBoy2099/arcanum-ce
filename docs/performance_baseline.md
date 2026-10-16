# Performance baselines

The first refactoring block adds measurements without changing game rules or
rendering policy. Profiling is off by default. In `arcanum.ini`, set:

```ini
[global]
performance_logging=1
```

Restart the game. `performance.csv` is appended in the current game directory.
Each enabled session has an identifier and numbered measurement intervals.
Summaries are written approximately every five seconds when the gameplay loop
is running, and on normal shutdown. Menus may defer the next periodic write.
An unwritable directory leaves profiling disabled and emits a debug diagnostic.
Set the option back to `0` and restart after testing. No new menu control is
added for this developer setting. Existing developer profiler commands now
enable, disable and flush these measurements.

The existing zoom-specific counters and their toggle are retained. The new CSV
uses a high-resolution SDL performance clock, measures both normal and zoomed
world drawing, and covers operations outside gameplay such as journal building.
Its fixed buffers do not grow with session length. Sorting and file writes occur
when flushing, outside measured gameplay frame work.

## Reading the CSV

Timing rows contain milliseconds, call count, sample count, mean, median (p50),
p95, p99 and maximum. The mean and maximum cover every call in the interval;
percentiles cover its most recent **2,048 calls** if there are more. Percentiles
use the nearest-rank definition. This cap is explicit in the `sampled` column.
They describe individual calls, not a distribution of per-frame totals.

| Metric | Scope |
| --- | --- |
| `frame_work` | Gameplay tick through presentation; excludes subsequent input dispatch and profiler flushing. Not a complete input-to-input frame duration. |
| `simulation` | `gamelib_ping`, including module ticks and combat information updates. |
| `world_render` | The world draw callback, excluding the subsequent zoom blit. |
| `zoom_blit` | Scaling the world buffer into the viewport when zoom is active. |
| `present_with_vsync` | Window composition and presentation, including any VSync wait. |
| `path` | Calls through `path_make`, including early failures and straight-path fast paths. |
| `text_measure` | Modern font measurement. Block 1 included rasterisation; block 2 uses SDL_ttf sizing without creating a text surface. |
| `text_raster` | SDL_ttf rasterisation calls, including shadows and measurements; excludes encoding conversion and later blits. |
| `journal_build` | Index construction or conversation fragment construction; excludes the later draw and latest-spread traversal. |
| `journal_paginate` | Initial conversation pagination after fragment construction; reused on subsequent unchanged history openings. |
| `crafting_scan` | Collection of eligible inventory objects; excludes the temporary array allocation and later matching. |

Timings are inclusive: a path calculation may be inside simulation; text
rasterisation may be inside text measurement and journal construction. **Do not
add these rows together.** These are elapsed times, not OS CPU-time counters.
VSync may dominate presentation time even when CPU work improves.

Counters cover requested dirty pixels after clipping but before merging
(overlaps are counted repeatedly), local path search iterations and scanned
cells. They represent workload, not the unique number of pixels actually drawn.
Path expansion counters currently cover the 4,096-cell local search, not every
path variant or the world-map search.

Block 3 adds `search_cache_builds`, `search_cache_hits`, `history_cache_hits`
and `index_cache_hits`. Search hits avoid matching schematic and component
names again. History hits avoid fragment construction and pagination. Index
hits still gather candidates and fingerprint visible journal texts, which is
included in `journal_build`; they avoid the candidates-by-text mention scan,
sorting and name copies. These counters count calls, not frames or distinct
screens, and are emitted only while profiling is enabled.

Memory rows are emitted only when the build defines `TIG_DEBUG_MEMORY`.
They cover `tig_memory_*` allocation/reallocation requests and requested bytes
per interval, plus current tracked bytes and blocks. They do not cover SDL,
driver allocations or the full process. Normal Release macros call the C
runtime directly, so these rows are deliberately absent rather than reporting
misleading zero allocations. Dedicated memory profiling remains a separate
diagnostic run: the tracked allocator changes allocation costs.

The profiler API and timing buffers are used from the main game thread only.
The memory snapshot reads the existing allocator under its mutex. No threads
are added to the engine.

## Reproducible comparison protocol

Keep the executable revision, machine, game data, language, save, resolution,
VSync, zoom, HUD stage, fonts and transparency settings with each capture.
Keep a copy of `arcanum.ini`. Use a separate CSV for each version/scenario by
renaming the previous file while the game is closed. Do not compare runs with
different settings or machines as if they measured the same workload.

For each version, load the same save, wait ten seconds for initial resource
loading, then repeat each scenario three times. Use 30-second captures for
continuous actions. For short UI actions, repeat the same open/close cycle and
compare the relevant operation counts and timings rather than frame FPS.

| Scenario | Procedure | Main measurements |
| --- | --- | --- |
| Idle | Remain still in the same city location; repeat for full, medium, mini and hidden HUD, with transparency off/on. | World rendering, dirty pixels, presentation. |
| Movement | Walk the same route through the city with the same party; preserve starting positions. | Paths, scanned cells, simulation, world rendering. |
| Combat | Repeat a fixed turn-based encounter, hover the same targets and tiles with combat information enabled. | Paths and simulation; match action and hover sequence. |
| Zoom | Repeat idle and movement at zoom 1.0 and a fixed zoom-out value. | World rendering, zoom blit, dirty pixels. |
| Journal | From a fixed save with a full conversation history, open Rumeurs, enter history, turn pages, close and repeat; separately repeat the quest index. | Journal build, text measure and rasterisation. |
| Crafting | Use the same loaded party inventories; open the same schematic, change search and quantity, then repeat. | Inventory scans; use a disposable save for actual crafting. |

Record cold first opens separately from subsequent warm opens. Do not claim a
performance improvement from unit tests: they verify counters and aggregation,
not real game workload. The fixed scenario in `automatic_benchmark.md` has a
reference capture at `out/performance-baselines/automatic-reference-20261016-072717`.
Its version 1 sequence opens each view once, so it does not directly quantify
warm journal/index cache benefits. Repeated manual openings are required for
those comparisons; keep cold and warm timings separate.

## Automated validation

`performance_test` checks disabled operation, failed file opening, exact
percentiles, rolling sample capacity, counters, reset/append behaviour, real
clock sampling and allocation snapshots. Existing feature and quality-of-life
tests remain enabled. Windows CI runs Release builds and all CTest suites on
x86 and x64 for pull requests, main/master pushes and manual dispatch.

No strict duration threshold is imposed on shared CI runners. Their execution
times are unsuitable as stable game performance baselines. Actual GitHub runs
require pushing the workflow; local validation does not confirm remote results.

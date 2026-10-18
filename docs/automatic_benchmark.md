# Performance scenario: button or automatic startup

## Start with the button

Load the save, then click **Benchmark**, at the upper right of the game view,
below the top strip. Keep `[global] benchmark_autorun=0` for manual startup.
The button appears in the normal game view; it is hidden while the scenario
runs and while other panels or menus are open. It follows resolution changes.

A persistent onscreen notice and a message in the HUD report completion or
the reason for stopping. Click the notice to dismiss it. A failed preflight
does not start automatic retries in manual mode: move to an open area and
click the button again. Escape cancels; losing focus interrupts the run.
Reload the original save before another measurement or normal play, because
simulation continues during a run. Manual starts can be repeated without
restarting the executable. Only a full run ending with `finished` is valid.

Button and notice strings are in `mes/benchmark_ui.mes` (French/English,
Windows-1252), with the English fallback in `mes/ce_fallback/benchmark_ui.mes`.

## Optional automatic startup

Close the game and set these options in `arcanum.ini`:

```ini
[global]
benchmark_autorun=1
performance_logging=0
```

Launch the game and load the same save for each comparison. Choose a conscious
character outside combat in an open outdoor location. The scenario starts
automatically once per process after a save load. Leave the game focused until
the journal/schematic panels close, normally after about 40 seconds at 60 Hz.
Escape cancels. Losing focus or quitting interrupts it. Moving the pointer
outside the game window is allowed and does not stop the scenario.
Check `benchmark-status.txt`: only `finished` means complete success.

After testing, close the game, set `benchmark_autorun=0` and reload the original
save before normal play. The script does not save, craft items or choose dialogue
responses. It temporarily disables travel/sleep autosaves, border scrolling and
camera follow. Prior settings and their dirty flag, HUD, transparency, zoom
target, camera origin and RNG state are restored. Movement and simulation occur
in memory; the game state is not automatically rewound at completion.

## Fixed scenario version 2

Version 2 keeps the original 13 phases and appends three repeated consultations.
The metadata records `scenario=2`; do not treat a full v1/v2 comparison as an
identical scenario. The original phase names remain useful for examining old
captures, but missing save fingerprints in old captures prevent verified
before/after comparisons.

| Phase | Action | Minimum frames |
| --- | --- | ---: |
| `warmup` | Center on player, opaque full HUD, zoom 1.0. | 120 |
| `idle_opaque_full` | Stable scene with opaque full HUD. | 180 |
| `walk_1`–`walk_4` | Four destinations in a reachable square or corridor, returning to start. | 60 per leg |
| `idle_opaque_hidden` | Stable scene with hidden HUD. | 180 |
| `idle_translucent_full` | Full HUD with translucency. | 180 |
| `zoom_075` | Same scene at zoom 0.75. | 180 |
| `rumors` | Reset zoom and open Rumors. | 60 |
| `conversations` | Open history and navigate pages at fixed frame counts. | 180 |
| `quest_index` | Open known-place/person index and navigate pages. | 180 |
| `schematics` | Open schematics and navigate forward/back. | 180 |
| `quest_index_repeat` | Reopen the journal and its known-place/person index. | 180 |
| `conversations_repeat` | Reopen the journal and its recorded conversations. | 180 |
| `schematics_repeat` | Reopen schematics and navigate forward/back. | 180 |

These are repeated consultations, not a guarantee that every cache is warm:
some UI caches intentionally clear on close. The counters report actual hits.

The first reachable four-tile square is selected in a fixed direction order
and recorded. If none fits, two-tile squares are tested, then four two-tile
legs out and back along a corridor. The recorded waypoints must match for a
comparison. When no route is available, normal input stays enabled and startup
is retried every three seconds: move to a more open area to allow it to start.
Each movement leg waits for the exact destination; after 600 frames an unmet
checkpoint is a failure. Rejected actions, changed map/player, unconsciousness,
death or combat abort the run. An already started run is never silently retried.

The RNG seed is fixed to `0xCE2026`. Normal simulation clocks remain active:
this repeats actions, **not bit-for-bit deterministic simulation**. AI scheduling
and pathfinding time budgets may vary with machine speed. Compare three runs per
build and keep failed runs separate. Empty journal/schematic collections make
those phases lighter; the script does not add entries or unlock schematics.

## Output and comparison

- `benchmark-performance.csv`: existing timing/counter format, separate from
  manual captures.
- `benchmark-phases.csv`: run identifier, phase, status, inclusive first/last
  interval, frame count, map and final player coordinates.
- `benchmark-runs.txt`: scenario version, loaded slot, settings, seed, platform,
  route, conversation count and CRC32/length fingerprints of loaded data.sav,
  and optional data2.sav/conversation/journal memory files. Files are read through
  TIG's virtual filesystem, including the mounted module's Save/Current directory.
  Fingerprints detect
  changed inputs; they are not security hashes.
- `benchmark-status.txt`: current phase or final outcome (`finished`, `timeout`,
  `action_failed`, `aborted`, `cancelled`, `interrupted_focus_lost`, `interrupted_quit`,
  `interrupted_shutdown`, `requires_safe_save`,
  `route_unavailable`, `output_unavailable`).

CSV and metadata files append runs; the status file holds the latest state.
Join timing rows to phase ranges by run and interval. Only `ok` phases from runs
ending in `finished` are complete comparisons; exclude `warmup` for steady-state
results. Periodic flushes may produce multiple intervals per phase. Match save
fingerprints, route, scenario version, resolution, VSync, fonts, game data and
machine. Keep executable hashes with captures; installation verifies them.

Generate a report with Python (no third-party dependencies):

```powershell
python tools/benchmark_report.py C:\Games\Arcanum --output out/benchmark-report.md
```

The tool selects the latest finished run, joins intervals by its exact session,
and rejects incomplete, duplicated, reordered or overlapping phases and missing
intervals. It produces Markdown and JSON. `--run` selects a specific finished run.
Means are weighted by call counts; interval percentiles are never combined into
an invented global percentile. Warmup is omitted from the displayed tables.
New metrics such as texture uploads, text-cache hits, collision queries, sorting
and ART evictions appear when present.

For compatible captures, add `--baseline <capture-directory>` and optionally
`--baseline-run <id>`. Different scenario versions, save fingerprints, routes,
settings or platforms prevent automatic deltas. A missing data.sav fingerprint
also prevents verified comparison. The report cannot verify machine, executable
or all external game assets from these metadata alone: preserve their identity
with the capture. One run is insufficient to establish repeatable gains.

The driver uses normal movement and navigation APIs. Physical and queued input
are consumed before UI dispatch, including held mouse repeats; system redraw,
quit and ping continue. No desktop automation is used. Save formats are unchanged:
the driver has no save/load payload.

The benchmark plan test checks phase order, frame counts, single entry, delayed
arrival, timeout boundaries and terminal states. The input test checks isolation,
system messages and restored normal delivery. Existing tests also run. Headless
tests do not execute an entire save; the first interactive run must confirm the
route and installed UI data.

## Manual button regression (2026-10-17)

The widget must retain an event-based press across frame updates until release.
`tig_mouse_get_state` copies only the cursor-hidden flag into the public state;
using its flags to check a held left button canceled every ordinary press on
an initialized mouse. The widget no longer polls that API for button state.
Losing application focus hides the widget and cancels its pending press.

The benchmark widget test now initializes the real mouse, message queue, button
manager and window filters. It sends a mouse press, dispatches queued messages,
updates the widget on the next frame, then dispatches the release. This regression
failed with the previous implementation and passes with the correction.
A test-only ART fixture supplies the headless cursor; no game installation is used.

## ART spike diagnostics

The profiler additionally measures `art_load` (file read/decompression and cache
entry setup), `art_prepare` (cached frame video buffer preparation), and `art_blit`
(software ART drawing). These measurements are nested/inclusive, not additive.
ART operations taking at least 5 ms also record their resource path in
`benchmark-performance.csv.details.csv`, joined by session and interval.
The report tool includes only details belonging to the selected completed run
and its measured phase intervals. Preserve this sidecar with archived captures.

No clock reads or diagnostic output occur when profiling is disabled. Sidecar
writes are buffered and flushed alongside normal profiling intervals, outside
the per-operation duration. Optional sidecar creation failure does not prevent
ordinary capture. The rendering/caching algorithm is unchanged in this build;
these diagnostics are intended to attribute the remaining observed 77 ms spike
before choosing a rendering optimization.

ART load sub-stages are now recorded as `art_file_open` (virtual/native open),
`art_metadata` (header, palettes and frame tables) and `art_pixels` (allocation,
frame reads and RLE decoding). They help distinguish raw-file stalls from RLE
work; the outer `art_load` remains the total cache-entry load measurement.

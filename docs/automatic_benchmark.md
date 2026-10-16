# Automatic performance scenario

Close the game and set these options in `arcanum.ini`:

```ini
[global]
benchmark_autorun=1
performance_logging=0
```

Launch the game and load the same save for each comparison. Choose a conscious
character outside combat in an open outdoor location. The scenario starts
automatically once per process after a save load. Leave the game focused until
the journal/schematic panels close, normally after about half a minute at 60 Hz.
Escape cancels. Losing focus or quitting interrupts it. Moving the pointer
outside the game window is allowed and does not stop the scenario.
Check `benchmark-status.txt`: only `finished` means complete success.

After testing, close the game, set `benchmark_autorun=0` and reload the original
save before normal play. The script does not save, craft items or choose dialogue
responses. It temporarily disables travel/sleep autosaves, border scrolling and
camera follow. Prior settings and their dirty flag, HUD, transparency, zoom
target, camera origin and RNG state are restored. Movement and simulation occur
in memory; the game state is not automatically rewound at completion.

## Fixed scenario version 1

| Phase | Action | Minimum frames |
| --- | --- | ---: |
| `warmup` | Center on player, opaque full HUD, zoom 1.0. | 120 |
| `idle_opaque_full` | Stable scene with opaque full HUD. | 180 |
| `walk_1`–`walk_4` | Four destinations in a four-tile square, returning to start. | 60 per leg |
| `idle_opaque_hidden` | Stable scene with hidden HUD. | 180 |
| `idle_translucent_full` | Full HUD with translucency. | 180 |
| `zoom_075` | Same scene at zoom 0.75. | 180 |
| `rumors` | Reset zoom and open Rumors. | 60 |
| `conversations` | Open history and navigate pages at fixed frame counts. | 180 |
| `quest_index` | Open known-place/person index and navigate pages. | 180 |
| `schematics` | Open schematics and navigate forward/back. | 180 |

The first reachable square is selected in a fixed direction order and recorded.
Each movement leg waits for the exact destination; after 600 frames an unmet
checkpoint is a failure. Rejected actions, changed map/player, unconsciousness,
death or combat abort the run. Unavailable routes require a more open save.

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
  data2.sav and optional conversation/journal memory files. Fingerprints detect
  changed inputs; they are not security hashes.
- `benchmark-status.txt`: current phase or final outcome (`finished`, `timeout`,
  `action_failed`, `aborted`, `interrupted_focus_lost`, `interrupted_quit`,
  `interrupted_shutdown`, `requires_safe_save`,
  `route_unavailable`, `output_unavailable`).

CSV and metadata files append runs; the status file holds the latest state.
Join timing rows to phase ranges by run and interval. Only `ok` phases from runs
ending in `finished` are complete comparisons; exclude `warmup` for steady-state
results. Periodic flushes may produce multiple intervals per phase. Match save
fingerprints, route, scenario version, resolution, VSync, fonts, game data and
machine. Keep executable hashes with captures; installation verifies them.

The driver uses normal movement and navigation APIs. Physical and queued input
are consumed before UI dispatch, including held mouse repeats; system redraw,
quit and ping continue. No desktop automation is used. Save formats are unchanged:
the driver has no save/load payload.

The benchmark plan test checks phase order, frame counts, single entry, delayed
arrival, timeout boundaries and terminal states. The input test checks isolation,
system messages and restored normal delivery. Existing tests also run. Headless
tests do not execute an entire save; the first interactive run must confirm the
route and installed UI data.

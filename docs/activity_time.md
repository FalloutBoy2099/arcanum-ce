# Configurable activity durations

`[time]` contains nine hour-valued keys, all disabled by default. Parsing accepts
finite decimal durations from 0 through 720 hours and rounds to milliseconds.
Four CP categories charge actual successful point expenditure and subtract
refunds while the sheet is open. Character creation is excluded.

Training is charged at `dialog_perform_training` and `DIALOG_ACTION_TR`, after
checking the returned rank against its previous rank. The raw skill setters
remain free of time side effects: dialog eligibility checks temporarily call
those setters, as do object initialization and follower propagation. Educator
records once per propagation only when a follower's rank actually improves.
Written schematic learning records only after adding a previously unknown entry.

The pending accumulator applies at the end of `gamelib_ping` only in the MAIN
mode, with dialogue, character sheet, main menu, save/load/reset and combat
excluded. Nine category totals permit point refunds without cancelling unrelated
training time. A single transition uses native `gfade_run`, advances game and
animation clocks with a normalized days/milliseconds `DateTime`, updates ambient
lighting and redraws under the fade. Normal event dispatch resumes next tick,
as with the script fade/advance action. It does not simulate sleeping/healing.

`SAT_FADE` in `script.c` already fades out, advances time by its seconds argument,
then fades in (optionally with sound/movie/delay). `SAT_FADE_AND_TELEPORT` adds
the same operation to teleportation. The new feature reuses their engine helpers;
it does not edit existing SCR/DLG assets or guess which scenario actions should
be suppressed. Explicit script time passage therefore remains cumulative.

The native fade renderer uses a fixed step count paced by presentation; its
duration argument is currently ignored by TIG. The new effect uses 20 steps
each way rather than the scripts' 48. Visual duration depends on VSync; no global
fade-renderer behavior was changed for this feature.

Pending durations are saved to `Save/Current/activity_time.bin`, version 1,
with category count and bounded uint32 durations. Missing/invalid old sidecars
load as zero; no existing data.sav layout or script opcode changes are needed.

Headless regressions exercise defaults, rank no-op/removal, decimal hours,
invalid/huge values, category refunds, party ownership, safe UI boundaries,
creation/load/reset exclusions, saturation, fade/clock/redraw order and pending
save/load including missing/truncated files. Settings regressions verify the
new section and fractional value round-trip. Gameplay appearance remains to
be validated with at least one nonzero setting.

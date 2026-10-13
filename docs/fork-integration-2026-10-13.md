# Fork Integration Notes - 2026-10-13

This branch integrates selected work from active Arcanum CE forks on top of the
local `Audio + Dialog` work.

## Integrated Forks

### God-damnit-all/main

Merged in full.

Main areas covered:

- UI and input fixes: ghost child buttons, cursor state, modal/overlay dismiss,
  Escape handling, save/load shortcuts, and mouse wheel scrolling.
- Dialogue/high resolution work: configurable dialogue scaling and related text
  bubble spacing.
- Gameplay/script fixes: inverted charisma wait gate, `SAT_SET_REACTION`, quest
  dumb text, script cache cleanup, and high-byte localized text parsing.

Local resolution:

- `src/game/tc.c` was merged manually to keep the local 10-option dynamic
  dialogue list while adding the fork's `DialogScale` support.

### Corrosion667/main

Merged after God-damnit-all.

Main additional areas covered:

- Optional manual follower leveling through `follower_level_scheme`.
- Turn-based poison and scripted spell processing fixes.
- Scripted weapon spell damage attribution.
- Additional stat/combat/magic fixes from the fork's gameplay branches.

### gzenz/main

Partially integrated.

The following non-video changes were already present after the
God-damnit-all/Corrosion667 merges and were verified as empty cherry-picks:

- Misc UI fixes from upstream PR #140.
- Mouse wheel scrolling from upstream PR #144.
- Script cache leak fix from upstream PR #147.
- Quest log `dumb_description` from upstream PR #142.

Explicitly not integrated yet:

- Optional FFmpeg-based Bink playback.
- Upscaled movie framebuffer changes.
- macOS-specific packaging/runtime changes.

Those video/macOS changes should be handled in a separate pass.

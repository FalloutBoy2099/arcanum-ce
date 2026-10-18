# Benchmark completion, 2026-10-17

Run 900699437339 completed all 16 phases. Reference: 792206787157.
Save CRCs, route, scenario and measured settings pass the comparison checks.
The comparator ignores startup splash and barter comparison colors because neither
is exercised by this scenario. Raw captures and metadata remain unchanged.

| Phase / metric | Before (ms) | After (ms) | Reduction |
| --- | ---: | ---: | ---: |
| quest_index / journal_build | 67.0915 | 12.2536 | 81.7% |
| quest_index_repeat / journal_build | 56.8582 | 1.2974 | 97.7% |
| conversations_repeat / journal_build | 56.7610 | 1.4255 | 97.5% |
| zoom_075 / zoom_blit | 1.9158 | 0.4413 | 77.0% |

This is one completed before/after capture, not a guarantee for every map.
VSync keeps average frame times near 16.6 ms; these are CPU subcomponent gains,
not equivalent FPS gains. The zoom phase still has a 76.791 ms maximum frame
(object_render maximum 69.8319 ms), so isolated hitches remain to investigate.
The headless report validator has 9 passing tests, including rejection of changed
lighting, font size, zoom filter and VSync settings.

## Diagnostic follow-up: run 1072040107236

All 16 phases completed and the comparison inputs match run 900699437339.
The resource sidecar identifies cold ART loads during journal opening:
Tab_Keys 169.2304 ms, Tab_BackGrnd 169.2277 ms and Tab_Rep 168.0410 ms.
These happen inside UI phase setup, which is outside the `frame_work` timer;
they identify interface-opening stalls, not three separate measured world frames.

In `zoom_075`, maximum frame time is 45.7655 ms, object rendering 39.1198 ms,
ART loading 8.5393 ms, frame preparation 0.0477 ms and software ART blit 0.0484 ms.
These maxima need not belong to the same call/frame and must not be subtracted
or added to claim attribution. The prior 76.791 ms maximum did not recur exactly.
Cold resource loading is now evidenced; it does not fully explain the remaining
world-render spike. Further profiling of object enumeration/path resolution
would be needed before claiming that issue fixed.

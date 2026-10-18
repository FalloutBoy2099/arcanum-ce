# Local-map automatic routes: 30 waypoints

The local-map route editor now accepts the same 30-waypoint capacity already
allocated for map routes. The old six-point restriction also reflected the
eight-entry animation goal stack: queued destinations consumed entries needed
by movement/door subgoals. Raising the editor limit alone would silently drop
later destinations or exhaust that stack.

`MovementRoute` keeps the local player's itinerary outside `AnimRunInfo`.
Only one destination is submitted to the normal walking/running machinery at
a time. The next destination starts after the bound animation completes at
the exact current waypoint. Each segment binds its own slot/unique identity
and retains camera following. Companion and eye-candy animation completions
cannot advance the itinerary. Existing speed, collision, pathfinding and
companion passage handling are reused.

Manual movement commands and successful interruption of the bound animation
clear the itinerary. Failed arrival never skips a point. Combat, spotted traps
and map clearing prevent continuation. Initialization/reset/load clear runtime
itineraries. Remaining waypoints are not serialized: after reloading a save,
the saved current movement may finish, but the route tail must be replotted.
Neither animation save layouts nor network packet layouts are enlarged.
CE networking remains disabled by `net_compat.h`.

The world-map travel algorithm and its existing 5,000-direction path buffer
are unchanged. The existing localized world-map message 611 is reused when
the 30-point editor limit is reached; no new INI setting or text is required.

`movement_route_test` checks all 30 destinations in order, animation identity
reuse, unrelated completions, snapshot/interrupt/rebind, blocked arrival,
explicit cancellation, duplicate destinations, one-point completion, invalid
inputs and a canary beyond storage. All 23 CTest suites pass.

Gameplay checks still needed: plot 7–30 points, edit/remove points past the
sixth, start the full route, cancel with a new click, and cross a doorway with
companions. These are not claimed as visually validated.

# Party movement tile-entry validation

The smoother follower movement retains paths and gait across animation ticks.
A path planned while a tile is free is not a reservation: another actor can
occupy the tile before the animated walk crosses its boundary. The previous
`sub_4305D0` committed normalized locations without a final occupancy check.
This audit establishes the missing check; it does not prove when the original
behavior first appeared or attribute it to the A* heap implementation.

`companion_movement_blocker` revalidates only a local player/companion tile
change, using the direct dynamic-critter query. The predicate is read-only and
runs no portal callbacks, path searches or scripts. Live critters block even
when unconscious. Dead, off, destroyed, inventory and explicitly nonblocking
objects do not block; explicit nonblocking movers retain their existing behavior.
Unrelated NPC movement and teleport/knockback relocation routines are unchanged.

On a blocked crossing, animation restores the last valid AID and pixel offset,
keeps the source location and current path index, marks the route invalid for
replanning, cancels a pending pivot and asks an eligible companion to make room.
No arrival effects or action-point charge for the rejected step are executed.
The original movement target remains available to the parent animation goal.

Headless regressions cover player/follower and follower/follower races in both
update orders, departing occupants, outside NPC blockers, unconscious/dead and
nonblocking exceptions, and scheduler flags/path-index preservation. Existing
A* oracle and pivot-scheduler regressions continue to pass. Actual appearance
and follower recovery in narrow passages still require gameplay validation.

## Recovery from transient occupants (17 October)

Gameplay feedback identified premature cancellation when the next tile belongs
to a companion. The `sub_427110` obstruction result leads to state 10 of
`AG_ATTEMPT_MOVE`, which terminates the whole order. Outside combat, a dynamic
critter occupant now defers to the tile-entry guard rather than taking that
termination branch. Non-critter obstacles and spotted traps retain their checks.

At a rejected crossing, the guard rolls back the animated frame/offset as before,
but retains the path and scheduler movement bit during a short wait. The wait
has a 1,200 ms deadline and a 100 ms animation pause. A cleared tile restores
the previous animation cadence. At the deadline, the attempt is popped and its
parent can rebuild a route to the original destination, first without the
ignore-critters fallback. No rejected step consumes an action point.

After 300 ms, a local player may interrupt a blocking companion's automatic
follow movement and ask it to make room through `AG_PLEASE_MOVE`. Manual orders,
combat, door animations and other interactions are excluded from this yield.
Runtime wait state is keyed by animation identity, goal and source/destination,
and cleared on initialization, reset and load; save layouts are unchanged.

Headless tests cover a departing occupant, preserved destination/path, deadline
replanning, restored cadence, recycled animation identity, goal/tile changes and
timer wraparound. The full 22-suite test run passes. Gameplay confirmation of
crossing and doorway behavior remains necessary.

## Anticipatory lateral passage (follow-up)

Feedback on that build showed two remaining problems: the companion could move
forward along the same corridor, and the player held a running frame while
waiting. Passage requests now inspect the next three planned player tiles at
movement startup and during movement, accounting for a tile boundary already
crossed by the animation. An idle or automatically following companion receives
a precomputed `AG_PLEASE_MOVE` destination immediately. Manual actions and combat
remain excluded from interruption.

The destination search prefers a one-tile lateral step, tries wider exits only
when needed, and rejects routes whose intermediate or final tiles belong to
the player's short corridor. Precomputing the destination also avoids the
random-neighbor fallback and resets the shared chained-NPC delay to zero in
the existing `AG_PLEASE_MOVE` startup handler. If no safe exit exists, the
companion's order is left intact and the player's bounded wait/repath guard
remains necessary to prevent overlap.

During this last-resort wait, the player uses a standing pose. The saved moving
AID and cadence are restored when the tile clears; waiting ticks do not replay
movement frames or footsteps. Tests cover lateral exits in all eight directions,
a wall on one side, a bent corridor and a corridor with no off-route exit.
The appearance/timing still needs gameplay confirmation.

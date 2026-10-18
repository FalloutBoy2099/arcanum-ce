#ifndef ARCANUM_GAME_MOVEMENT_ROUTE_H_
#define ARCANUM_GAME_MOVEMENT_ROUTE_H_

#include <stdbool.h>
#include <stdint.h>

#define MOVEMENT_ROUTE_CAPACITY 30

// Runtime itinerary, separate from the eight-entry serialized animation stack.
typedef struct MovementRoute {
    int64_t actor;
    int64_t points[MOVEMENT_ROUTE_CAPACITY];
    int count;
    int current;
    int slot;
    int unique_id;
} MovementRoute;

void movement_route_clear(MovementRoute* route);
bool movement_route_begin(MovementRoute* route, int64_t actor, const int64_t* points, int count);
void movement_route_bind(MovementRoute* route, int slot, int unique_id);
bool movement_route_matches(const MovementRoute* route, int slot, int unique_id);
bool movement_route_advance(MovementRoute* route, int slot, int unique_id, int64_t reached);

#endif

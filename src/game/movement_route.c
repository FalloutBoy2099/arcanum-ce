#include "game/movement_route.h"

#include <string.h>

void movement_route_clear(MovementRoute* route)
{
    memset(route, 0, sizeof(*route));
    route->slot = -1;
}

bool movement_route_begin(MovementRoute* route, int64_t actor, const int64_t* points, int count)
{
    if (actor == 0 || points == NULL || count <= 0 || count > MOVEMENT_ROUTE_CAPACITY) return false;
    for (int i = 0; i < count; i++) {
        if (points[i] == 0 || points[i] == -1) return false;
    }
    movement_route_clear(route);
    route->actor = actor;
    route->count = count;
    memcpy(route->points, points, count * sizeof(*points));
    return true;
}

void movement_route_bind(MovementRoute* route, int slot, int unique_id)
{
    route->slot = slot;
    route->unique_id = unique_id;
}

bool movement_route_matches(const MovementRoute* route, int slot, int unique_id)
{
    return route->count > 0 && route->slot >= 0
        && route->slot == slot && route->unique_id == unique_id;
}

bool movement_route_advance(MovementRoute* route, int slot, int unique_id, int64_t reached)
{
    if (!movement_route_matches(route, slot, unique_id)) return false;
    if (reached != route->points[route->current] || ++route->current == route->count) {
        movement_route_clear(route);
        return false;
    }
    route->slot = -1; // The next segment must bind its new animation identity.
    return true;
}

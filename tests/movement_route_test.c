#include "game/movement_route.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static void test_full_itinerary(void)
{
    struct { MovementRoute route; unsigned int guard; } fixture = { 0 };
    fixture.guard = 0x12345678;
    int64_t points[MOVEMENT_ROUTE_CAPACITY];
    for (int i = 0; i < MOVEMENT_ROUTE_CAPACITY; i++) points[i] = 1000 + i;
    CHECK(movement_route_begin(&fixture.route, 42, points, MOVEMENT_ROUTE_CAPACITY));
    for (int i = 0; i < MOVEMENT_ROUTE_CAPACITY; i++) {
        MovementRoute* route = &fixture.route;
        CHECK(route->current == i && route->points[i] == points[i]);
        movement_route_bind(route, i % 4, 100 + i);
        // Completion of a companion/eye-candy animation must not advance us.
        CHECK(!movement_route_advance(route, 10, 100 + i, points[i]));
        CHECK(!movement_route_advance(route, i % 4, 99 + i, points[i]));
        CHECK(route->current == i);
        // Production takes a snapshot, interrupts/clears the old goal, then
        // binds the continuation to the next movement animation.
        MovementRoute continuation = *route;
        bool next = movement_route_advance(&continuation, i % 4, 100 + i, points[i]);
        movement_route_clear(route);
        CHECK(!movement_route_matches(route, i % 4, 100 + i));
        CHECK(next == (i + 1 < MOVEMENT_ROUTE_CAPACITY));
        if (next) {
            CHECK(!movement_route_matches(&continuation, i % 4, 100 + i));
            *route = continuation;
        }
        CHECK(fixture.guard == 0x12345678);
    }
    CHECK(fixture.route.count == 0 && fixture.route.actor == 0);
}

static void test_cancellation_and_validation(void)
{
    MovementRoute route;
    int64_t points[] = { 100, 200, 200, 300 }; // Repeated points are legal.
    CHECK(movement_route_begin(&route, 42, points, 4));
    movement_route_bind(&route, 3, 99);
    CHECK(!movement_route_advance(&route, 3, 99, 150)); // Blocked before arrival.
    CHECK(route.count == 0);
    CHECK(movement_route_begin(&route, 42, points, 4));
    for (int i = 0; i < 4; i++) {
        movement_route_bind(&route, 3, 100 + i);
        CHECK(movement_route_advance(&route, 3, 100 + i, points[i]) == (i < 3));
    }
    CHECK(route.count == 0);
    CHECK(movement_route_begin(&route, 42, points, 4));
    movement_route_bind(&route, 3, 99);
    movement_route_clear(&route); // Manual command, interrupt, reset or load.
    CHECK(!movement_route_advance(&route, 3, 99, 100));
    CHECK(movement_route_begin(&route, 42, points, 1));
    movement_route_bind(&route, 3, 99);
    CHECK(!movement_route_advance(&route, 3, 99, 100) && route.count == 0);
    CHECK(!movement_route_begin(&route, 0, points, 4));
    CHECK(!movement_route_begin(&route, 42, NULL, 4));
    CHECK(!movement_route_begin(&route, 42, points, 0));
    CHECK(!movement_route_begin(&route, 42, points, -1));
    CHECK(!movement_route_begin(&route, 42, points, MOVEMENT_ROUTE_CAPACITY + 1));
    points[1] = 0;
    CHECK(!movement_route_begin(&route, 42, points, 4));
    points[1] = -1;
    CHECK(!movement_route_begin(&route, 42, points, 4));
}

int main(void)
{
    test_full_itinerary();
    test_cancellation_and_validation();
    printf("Movement itineraries: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

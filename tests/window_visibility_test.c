#include <stdio.h>

#include <tig/memory.h>
#include <tig/rect.h>
#include <tig/window.h>
#include "ui/search_bar.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_rect_init(NULL) == TIG_OK);
    TigRect rect;
    CHECK(!tig_window_translucent_underlay_rect(0, 1, &rect));
    TigInitInfo init = { 0 };
    init.width = 640;
    init.height = 480;
    CHECK(tig_window_init(&init) == TIG_OK);
    TigWindowData data = { 0 };
    data.rect = (TigRect){ 0, 0, 640, 480 };
    tig_window_handle_t world = TIG_WINDOW_HANDLE_INVALID;
    tig_window_handle_t hud = TIG_WINDOW_HANDLE_INVALID;
    CHECK(tig_window_create(&data, &world) == TIG_OK);
    data.rect = (TigRect){ 50, 300, 500, 160 };
    CHECK(tig_window_create(&data, &hud) == TIG_OK);
    if (world == TIG_WINDOW_HANDLE_INVALID || hud == TIG_WINDOW_HANDLE_INVALID) return 1;
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_near_black_alpha_set(hud, true, world, 8, 128) == TIG_OK);
    CHECK(tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(rect.x == 50 && rect.y == 300 && rect.width == 500 && rect.height == 160);
    TigRect clip = { 50, 410, 500, 25 };
    CHECK(tig_window_clip_rect_set(hud, &clip) == TIG_OK);
    CHECK(tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(rect.x == 50 && rect.y == 410 && rect.width == 500 && rect.height == 25);
    clip.y = 999;
    CHECK(tig_window_clip_rect_set(hud, &clip) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(rect.width == 0 && rect.height == 0);
    CHECK(tig_window_clip_rect_set(hud, NULL) == TIG_OK);
    CHECK(tig_window_hide(hud) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_show(hud) == TIG_OK);
    CHECK(tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_near_black_alpha_set(hud, true, world, 8, 255) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_near_black_alpha_set(hud, true, world, 8, 0) == TIG_OK);
    CHECK(tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_move(hud, -20, 450) == TIG_OK);
    CHECK(tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(rect.x == 0 && rect.y == 450 && rect.width == 480 && rect.height == 30);
    CHECK(tig_window_near_black_alpha_set(hud, false, world, 8, 128) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(tig_window_near_black_alpha_set(hud, true, TIG_WINDOW_HANDLE_INVALID, 8, 128) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    CHECK(!tig_window_translucent_underlay_rect(TIG_WINDOW_HANDLE_INVALID, world, &rect));
    CHECK(!tig_window_translucent_underlay_rect(hud, TIG_WINDOW_HANDLE_INVALID, &rect));
    CHECK(!tig_window_translucent_underlay_rect(hud, world, NULL));
    // Attached toolbars may extend below a panel without being clipped to its
    // buffer. Their separate window follows the parent's screen position.
    CHECK(tig_window_move(hud, 50, 20) == TIG_OK);
    tig_window_handle_t toolbar = TIG_WINDOW_HANDLE_INVALID;
    CHECK(ui_search_bar_open(&toolbar, hud, (TigRect){ 10, 180, 473, 47 }, NULL));
    TigWindowData toolbar_data;
    CHECK(tig_window_data(toolbar, &toolbar_data) == TIG_OK);
    CHECK(toolbar_data.rect.x == 60 && toolbar_data.rect.y == 200);
    CHECK(toolbar_data.rect.width == 473 && toolbar_data.rect.height == 47);
    tig_window_handle_t original_toolbar = toolbar;
    CHECK(tig_window_hide(toolbar) == TIG_OK);
    CHECK(tig_window_move(hud, 60, 30) == TIG_OK);
    CHECK(ui_search_bar_open(&toolbar, hud, (TigRect){ 10, 180, 473, 47 }, NULL));
    CHECK(toolbar == original_toolbar);
    CHECK(tig_window_data(toolbar, &toolbar_data) == TIG_OK);
    CHECK(toolbar_data.rect.x == 70 && toolbar_data.rect.y == 210);
    CHECK((toolbar_data.flags & TIG_WINDOW_HIDDEN) == 0);
    TigRect cell = { 0, 25, 348, 22 };
    CHECK(ui_search_bar_contains(&cell, 0, 25));
    CHECK(ui_search_bar_contains(&cell, 347, 46));
    CHECK(!ui_search_bar_contains(&cell, 348, 25));
    CHECK(!ui_search_bar_contains(&cell, 0, 47));
    ui_search_bar_close(&toolbar);
    CHECK(toolbar == TIG_WINDOW_HANDLE_INVALID);
    ui_search_bar_close(&toolbar);
    // XL panels grow downward, retaining their top-left. Returning to a normal
    // screen restores 400 pixels without recentering or moving its controls.
    CHECK(tig_window_hide(hud) == TIG_OK);
    CHECK(ui_panel_window_height(&hud, 450));
    CHECK(tig_window_data(hud, &toolbar_data) == TIG_OK);
    CHECK(toolbar_data.rect.x == 60 && toolbar_data.rect.y == 30);
    CHECK(toolbar_data.rect.width == 500 && toolbar_data.rect.height == 450);
    CHECK((toolbar_data.flags & TIG_WINDOW_HIDDEN) != 0);
    CHECK(ui_panel_window_height(&hud, 400));
    CHECK(tig_window_data(hud, &toolbar_data) == TIG_OK);
    CHECK(toolbar_data.rect.x == 60 && toolbar_data.rect.y == 30 && toolbar_data.rect.height == 400);
    original_toolbar = hud;
    CHECK(ui_panel_window_height(&hud, 400) && hud == original_toolbar);
    CHECK(!ui_panel_window_height(&hud, 0) && hud == original_toolbar);
    CHECK(tig_window_destroy(hud) == TIG_OK);
    CHECK(!tig_window_translucent_underlay_rect(hud, world, &rect));
    tig_window_exit();
    tig_rect_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Window visibility regressions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}

#include "ui/benchmark_widget.h"
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static bool mouse(int x, int y, TigMessageMouseEvent event, bool repeat)
{
    TigMessage message = { 0 };
    message.type = TIG_MESSAGE_MOUSE;
    message.data.mouse.x = x; message.data.mouse.y = y;
    message.data.mouse.event = event; message.data.mouse.repeat = repeat;
    return benchmark_widget_mouse(&message);
}

static const char* cursor_art;
static int cursor_path(tig_art_id_t id, char* path, size_t size)
{
    (void)id;
    SDL_strlcpy(path, cursor_art, size);
    return TIG_OK;
}
static tig_art_id_t cursor_id(tig_art_id_t id) { return tig_art_id_frame_set(id, 0); }
static void drain_messages(void)
{
    TigMessage message;
    while (tig_message_dequeue(&message) == TIG_OK) { }
}

int main(int argc, char** argv)
{
    CHECK(argc > 2);
    if (argc <= 2) return 1;
    CHECK(SDL_Init(0));
    tig_color_init(NULL);
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    tig_rect_init(NULL);
    TigInitInfo init = { 0 }; init.width = 640; init.height = 480;
    CHECK(tig_window_init(&init) == TIG_OK);
    cursor_art = argv[2];
    init.bpp = 32;
    init.art_file_path_resolver = cursor_path;
    init.art_id_reset_func = cursor_id;
    CHECK(tig_palette_init(&init) == TIG_OK);
    CHECK(tig_file_repository_add("."));
    CHECK(tig_art_init(&init) == TIG_OK);
    CHECK(tig_message_init(&init) == TIG_OK);
    CHECK(tig_button_init(&init) == TIG_OK);
    CHECK(tig_mouse_init(&init) == TIG_OK);
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN | TIG_FONT_CENTERED;
    font.modern_font_path = argv[1]; font.modern_font_size = 14;
    font.color = tig_color_make(245, 235, 205);
    tig_font_handle_t handle;
    tig_font_create(&font, &handle);
    TigRect screen = { 0, 0, 640, 480 };
    benchmark_widget_update(screen, true, handle, "Benchmark", "");
    CHECK(!benchmark_widget_take_request());
    CHECK(mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false));
    CHECK(!benchmark_widget_take_request()); // Release without press cannot start.
    CHECK(mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, false));
    CHECK(mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false));
    CHECK(benchmark_widget_take_request());
    CHECK(!benchmark_widget_take_request()); // Request is consumed once.
    mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, false);
    CHECK(!mouse(100, 100, TIG_MESSAGE_MOUSE_MOVE, false));
    mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false);
    CHECK(!benchmark_widget_take_request()); // Drag outside cancels the click.
    mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, true);
    mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false);
    CHECK(!benchmark_widget_take_request()); // Held repeats cannot start a run.
    mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, false);
    benchmark_widget_update(screen, false, handle, "Benchmark", "");
    CHECK(!mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false));
    CHECK(!benchmark_widget_take_request());
    benchmark_widget_update(screen, true, handle, "Benchmark", "Benchmark complete.");
    CHECK(mouse(300, 110, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false)); // Dismiss notice.
    CHECK(!benchmark_widget_take_request());
    screen = (TigRect){ 40, 20, 800, 600 };
    benchmark_widget_update(screen, true, handle, "Benchmark", "");
    CHECK(!mouse(520, 70, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, false));
    mouse(720, 90, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN, false);
    mouse(720, 90, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP, false);
    CHECK(benchmark_widget_take_request()); // Resize/offset use current bounds.
    // Real mouse -> queued messages -> button processing -> window filters.
    // A held click must survive rendering between down and up messages.
    tig_mouse_set_position(720, 90);
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT, true);
    drain_messages();
    CHECK(!benchmark_widget_take_request());
    benchmark_widget_update(screen, true, handle, "Benchmark", "");
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT, false);
    drain_messages();
    CHECK(benchmark_widget_take_request());
    CHECK(!benchmark_widget_take_request());
    benchmark_widget_close(); benchmark_widget_close();
    CHECK(!benchmark_widget_take_request());
    tig_font_destroy(handle);
    tig_mouse_exit();
    tig_button_exit();
    tig_message_exit();
    tig_window_exit();
    tig_art_exit();
    tig_file_repository_remove_all();
    tig_palette_exit();
    tig_rect_exit(); tig_color_exit();
    SDL_Quit();
    printf("Benchmark button and notice: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

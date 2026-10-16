#include <stdio.h>
#include <string.h>

#include <tig/tig.h>
#include "ui/benchmark_input.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    CHECK(benchmark_input_interrupt_reason(SDL_EVENT_WINDOW_MOUSE_LEAVE) == NULL);
    CHECK(benchmark_input_interrupt_reason(SDL_EVENT_WINDOW_MOUSE_ENTER) == NULL);
    CHECK(benchmark_input_interrupt_reason(SDL_EVENT_WINDOW_FOCUS_GAINED) == NULL);
    CHECK(benchmark_input_interrupt_reason(SDL_EVENT_MOUSE_MOTION) == NULL);
    CHECK(strcmp(benchmark_input_interrupt_reason(SDL_EVENT_WINDOW_FOCUS_LOST), "interrupted_focus_lost") == 0);
    CHECK(strcmp(benchmark_input_interrupt_reason(SDL_EVENT_QUIT), "interrupted_quit") == 0);
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_message_init(NULL) == TIG_OK);
    tig_message_input_block_set(true);
    TigMessage message = { 0 };
    message.type = TIG_MESSAGE_KEYBOARD;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    message.type = TIG_MESSAGE_MOUSE;
    message.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    message.type = TIG_MESSAGE_TEXT_INPUT;
    message.data.text.text = "ignored";
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    message.type = TIG_MESSAGE_BUTTON;
    message.data.button.button_handle = TIG_BUTTON_HANDLE_INVALID;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    message.type = TIG_MESSAGE_REDRAW;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    message.type = TIG_MESSAGE_PING;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    CHECK(tig_message_post_quit(0) == TIG_OK);
    CHECK(tig_message_dequeue(&message) == TIG_OK && message.type == TIG_MESSAGE_REDRAW);
    CHECK(tig_message_dequeue(&message) == TIG_OK && message.type == TIG_MESSAGE_PING);
    CHECK(tig_message_dequeue(&message) == TIG_OK && message.type == TIG_MESSAGE_QUIT);
    CHECK(tig_message_dequeue(&message) == TIG_ERR_MESSAGE_QUEUE_EMPTY);
    tig_message_input_block_set(false);
    message = (TigMessage){ 0 };
    message.type = TIG_MESSAGE_KEYBOARD;
    message.data.keyboard.scancode = SDL_SCANCODE_A;
    CHECK(tig_message_enqueue(&message) == TIG_OK);
    CHECK(tig_message_dequeue(&message) == TIG_OK && message.type == TIG_MESSAGE_KEYBOARD
        && message.data.keyboard.scancode == SDL_SCANCODE_A);
    tig_message_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Benchmark input isolation: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}

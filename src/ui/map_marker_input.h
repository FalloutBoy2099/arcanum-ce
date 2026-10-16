#ifndef ARCANUM_UI_MAP_MARKER_INPUT_H_
#define ARCANUM_UI_MAP_MARKER_INPUT_H_

#include <tig/message.h>

static inline bool map_marker_edit_gesture(const TigMessage* message, bool control)
{
    return control && message->type == TIG_MESSAGE_MOUSE
        && message->data.mouse.event == TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN
        && !message->data.mouse.repeat;
}

static inline bool map_marker_release_gesture(const TigMessage* message, int pending_release)
{
    return pending_release >= 0 && message->type == TIG_MESSAGE_MOUSE
        && message->data.mouse.event == pending_release;
}

static inline bool map_marker_delete_gesture(const TigMessage* message, bool alt)
{
    return alt && message->type == TIG_MESSAGE_MOUSE
        && message->data.mouse.event == TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN
        && !message->data.mouse.repeat;
}

#endif

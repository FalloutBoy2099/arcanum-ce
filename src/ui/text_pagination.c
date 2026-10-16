#include "ui/text_pagination.h"

int ui_text_paginate_spread(int start, int last, const int heights[2],
    UiPageEntryMeasure measure, void* context)
{
    int step = start > last ? -1 : 1;
    int entry = start;
    int page = 0;
    int remaining = heights[0];
    bool first = true;
    while (entry != last + step) {
        int height = measure(entry, page, remaining, step < 0, first, context);
        if (height < 0) {
            entry += step;
            continue;
        }
        first = false;
        if (height > 0 && height <= remaining) {
            remaining -= height;
        } else {
            if (page == 1) {
                if (height > 0) entry += step;
                break;
            }
            page = 1;
            remaining = heights[1];
            first = true;
            if (height == 0) entry -= step;
        }
        entry += step;
    }
    return entry - step;
}

#ifndef ARCANUM_UI_TEXT_PAGINATION_H_
#define ARCANUM_UI_TEXT_PAGINATION_H_

#include <stdbool.h>

// Negative height skips an entry (empty or filtered); zero means it must be
// retried on a fresh page. first permits the existing oversized-entry policy.
typedef int (*UiPageEntryMeasure)(int entry, int page, int remaining,
    bool dry_run, bool first, void* context);
int ui_text_paginate_spread(int start, int last, const int heights[2],
    UiPageEntryMeasure measure, void* context);

#endif

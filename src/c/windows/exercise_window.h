#pragma once

#include <pebble.h>

// The sets of one exercise. Select completes a set (or edits a done one),
// long Select opens set options. The last rows switch exercises.
void exercise_window_push(int entry_index);
void exercise_window_refresh(void);
// Repaints without reloading, e.g. for the rest countdown in the header.
void exercise_window_redraw(void);

// Moves the selection to the next set to do, or to "Next exercise".
void exercise_window_select_next(void);

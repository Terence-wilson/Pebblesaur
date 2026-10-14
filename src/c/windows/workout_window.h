#pragma once

#include <pebble.h>

// The app's main window: the workout's exercises, plus start/finish.
void workout_window_push(void);
void workout_window_refresh(void);
void workout_window_redraw(void);

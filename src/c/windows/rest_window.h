#pragma once

#include <pebble.h>

// Full-screen rest countdown. Up/Down add/remove 15s, Select ends rest,
// Back leaves it running in the background.
void rest_window_push(void);
void rest_window_refresh(void);

#pragma once

#include <pebble.h>

// A full-screen notice. Any button dismisses it; it also closes on its own.
void message_window_push(const char *title, const char *text);

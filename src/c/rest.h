#pragma once

#include <pebble.h>

// The rest timer between sets. It survives the app being closed: the end time
// is persisted and a wakeup brings the app back when rest is over.

typedef void (*RestTickHandler)(void);

// woke_up: the app was launched by the rest wakeup, so it should buzz now.
void rest_init(RestTickHandler on_tick, bool woke_up);
void rest_deinit(void);

void rest_start(int seconds, const char *next_label);
void rest_stop(void);
void rest_adjust(int delta_seconds);

bool rest_active(void);
// Seconds left; negative once rest is over (time since it ended).
int rest_remaining(void);
int rest_total(void);
const char *rest_next_label(void);

// "1:30", or "+0:12" when overdue.
void rest_format(int seconds, char *buf, size_t size);

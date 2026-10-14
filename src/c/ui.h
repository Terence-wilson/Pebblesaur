#pragma once

#include <pebble.h>
#include "model.h"

#define UI_HIGHLIGHT PBL_IF_COLOR_ELSE(GColorIndigo, GColorBlack)
#define UI_ACCENT PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define UI_DONE PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack)
#define UI_OVERDUE PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)
#define UI_HEADER_HEIGHT PBL_IF_ROUND_ELSE(36, 22)

// "5 x 100kg", "3-5 x 100kg", "5+ x 100kg", or the logged reps once done.
void ui_format_set(const WorkoutSet *set, char *buf, size_t size);

// Hint shown under a set: rest time, AMRAP, RPE.
void ui_format_set_details(const WorkoutSet *set, char *buf, size_t size);

// A circle, ticked when the set is done.
void ui_draw_check(GContext *ctx, GPoint center, bool done, bool pending, bool highlighted);

// Menu header with a title and, while resting, the rest countdown.
void ui_draw_header(GContext *ctx, const Layer *cell_layer, const char *title);

void ui_style_menu(MenuLayer *menu, Window *window);

// Called when the model or the rest timer changes; refreshes every open window.
void ui_refresh_all(void);
// Called every second while resting; repaints the countdowns.
void ui_redraw_all(void);

#include "ui.h"
#include "rest.h"
#include "windows/exercise_window.h"
#include "windows/rest_window.h"
#include "windows/workout_window.h"

void ui_format_set(const WorkoutSet *set, char *buf, size_t size) {
  bool done = set->flags & SET_FLAG_COMPLETED;
  char reps[12];
  if (done && set->done_reps >= 0) {
    snprintf(reps, sizeof(reps), "%d", set->done_reps);
  } else if (set->min_reps >= 0 && set->min_reps != set->reps) {
    snprintf(reps, sizeof(reps), "%d-%d", set->min_reps, set->reps);
  } else {
    snprintf(reps, sizeof(reps), "%d%s", set->reps, (set->flags & SET_FLAG_AMRAP) ? "+" : "");
  }
  if (set->weight[0]) {
    snprintf(buf, size, "%s x %s", reps, set->weight);
  } else {
    snprintf(buf, size, "%s reps", reps);
  }
}

static void prv_append(char *buf, size_t size, const char *text) {
  size_t len = strlen(buf);
  if (len > 0 && len + 2 < size) {
    strncat(buf, ", ", size - len - 1);
  }
  strncat(buf, text, size - strlen(buf) - 1);
}

void ui_format_set_details(const WorkoutSet *set, char *buf, size_t size) {
  buf[0] = '\0';
  char part[20];
  if (set->flags & SET_FLAG_COMPLETED) {
    prv_append(buf, size, (set->flags & SET_FLAG_PENDING) ? "Saving..." : "Done");
    if (set->done_rpe10 >= 0) {
      snprintf(part, sizeof(part), "@%d.%d", set->done_rpe10 / 10, set->done_rpe10 % 10);
      prv_append(buf, size, part);
    }
    return;
  }
  if (set->flags & SET_FLAG_WARMUP) {
    prv_append(buf, size, "Warmup");
  }
  if (set->flags & SET_FLAG_AMRAP) {
    prv_append(buf, size, "AMRAP");
  }
  if (set->rpe10 >= 0) {
    snprintf(part, sizeof(part), "RPE %d.%d", set->rpe10 / 10, set->rpe10 % 10);
    prv_append(buf, size, part);
  } else if (set->flags & SET_FLAG_LOG_RPE) {
    prv_append(buf, size, "Log RPE");
  }
  if (set->timer > 0) {
    char time[8];
    rest_format(set->timer, time, sizeof(time));
    snprintf(part, sizeof(part), "Rest %s", time);
    prv_append(buf, size, part);
  }
}

void ui_draw_check(GContext *ctx, GPoint center, bool done, bool pending, bool highlighted) {
  GColor fg = highlighted ? GColorWhite : (done ? UI_DONE : GColorBlack);
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  const int radius = 8;
  if (done && !pending) {
    graphics_fill_circle(ctx, center, radius);
    GColor tick = highlighted ? UI_HIGHLIGHT : GColorWhite;
    graphics_context_set_stroke_color(ctx, tick);
    graphics_draw_line(ctx, GPoint(center.x - 4, center.y), GPoint(center.x - 1, center.y + 3));
    graphics_draw_line(ctx, GPoint(center.x - 1, center.y + 3), GPoint(center.x + 4, center.y - 3));
  } else {
    graphics_draw_circle(ctx, center, radius);
    if (done) {
      // Pending: a dot until the phone confirms.
      graphics_fill_circle(ctx, center, 3);
    }
  }
}

void ui_draw_header(GContext *ctx, const Layer *cell_layer, const char *title) {
  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_fill_color(ctx, UI_HIGHLIGHT);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  char rest[12] = "";
  if (rest_active()) {
    rest_format(rest_remaining(), rest, sizeof(rest));
  }
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#if defined(PBL_ROUND)
  // Round screens: centered, with the rest timer on a line of its own above.
  GRect title_rect = GRect(bounds.origin.x + 24, bounds.origin.y + 14, bounds.size.w - 48, 20);
  graphics_draw_text(ctx, title, font, title_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  if (rest[0]) {
    graphics_context_set_text_color(ctx, UI_ACCENT);
    graphics_draw_text(ctx, rest, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, 16),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
#else
  int rest_w = rest[0] ? 44 : 0;
  GRect title_rect = GRect(bounds.origin.x + 4, bounds.origin.y - 2, bounds.size.w - 8 - rest_w, 22);
  graphics_draw_text(ctx, title, font, title_rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  if (rest[0]) {
    graphics_context_set_text_color(ctx, UI_ACCENT);
    GRect rest_rect = GRect(bounds.origin.x + bounds.size.w - rest_w - 4, bounds.origin.y - 2, rest_w, 22);
    graphics_draw_text(ctx, rest, font, rest_rect, GTextOverflowModeFill, GTextAlignmentRight, NULL);
  }
#endif
}

void ui_style_menu(MenuLayer *menu, Window *window) {
  menu_layer_set_normal_colors(menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(menu, UI_HIGHLIGHT, GColorWhite);
  menu_layer_set_click_config_onto_window(menu, window);
}

void ui_refresh_all(void) {
  workout_window_refresh();
  exercise_window_refresh();
  rest_window_refresh();
}

void ui_redraw_all(void) {
  workout_window_redraw();
  exercise_window_redraw();
  rest_window_refresh();
}

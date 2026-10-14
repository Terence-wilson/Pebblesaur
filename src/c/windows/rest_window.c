#include "rest_window.h"
#include "../rest.h"
#include "../ui.h"

#define ADJUST_SECONDS 15

// Per-window state: a popped window is unloaded after its transition, so
// it may still exist when the next one is pushed.
typedef struct {
  Window *window;
  Layer *canvas;
} RestWindow;

// The newest rest window, until it unloads.
static RestWindow *s_current;

static void prv_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int remaining = rest_remaining();
  int total = rest_total();
  bool overdue = rest_active() && remaining <= 0;

  // Progress ring: shrinks as rest runs out.
  int ring_size = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) - PBL_IF_ROUND_ELSE(16, 20);
  GRect ring = GRect((bounds.size.w - ring_size) / 2, (bounds.size.h - ring_size) / 2, ring_size, ring_size);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, 4, 0, TRIG_MAX_ANGLE);
  if (rest_active() && total > 0) {
    int32_t angle = overdue ? TRIG_MAX_ANGLE : (int32_t)((int64_t)TRIG_MAX_ANGLE * remaining / total);
    graphics_context_set_fill_color(ctx, overdue ? UI_OVERDUE : UI_HIGHLIGHT);
    graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, 10, 0, angle);
  }

  int cy = bounds.size.h / 2;
  graphics_context_set_text_color(ctx, overdue ? UI_OVERDUE : GColorBlack);
  graphics_draw_text(ctx, overdue ? "GO!" : "REST", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, cy - 44, bounds.size.w, 22), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  char time_text[12];
  rest_format(rest_active() ? remaining : 0, time_text, sizeof(time_text));
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, time_text, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
                     GRect(0, cy - 24, bounds.size.w, 40), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  const char *next = rest_next_label();
  if (next[0]) {
    int inset = ring.origin.x + 18;
    graphics_draw_text(ctx, next, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(inset, cy + 16, bounds.size.w - 2 * inset, 32),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

#if !defined(PBL_ROUND)
  // Button hints in the corners, outside the ring.
  GFont hint = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  graphics_context_set_text_color(ctx, UI_HIGHLIGHT);
  graphics_draw_text(ctx, "+15", hint, GRect(bounds.size.w - 28, 0, 28, 18), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
  graphics_draw_text(ctx, "-15", hint, GRect(bounds.size.w - 28, bounds.size.h - 20, 28, 18),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
#endif
}

static void prv_up(ClickRecognizerRef recognizer, void *context) {
  rest_adjust(ADJUST_SECONDS);
}

static void prv_down(ClickRecognizerRef recognizer, void *context) {
  rest_adjust(-ADJUST_SECONDS);
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  RestWindow *rw = context;
  rest_stop();
  window_stack_remove(rw->window, true);
}

static void prv_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 300, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 300, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
}

static void prv_load(Window *window) {
  RestWindow *rw = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  rw->canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(rw->canvas, prv_draw);
  layer_add_child(root, rw->canvas);
}

static void prv_unload(Window *window) {
  RestWindow *rw = window_get_user_data(window);
  layer_destroy(rw->canvas);
  if (s_current == rw) {
    s_current = NULL;
  }
  free(rw);
  window_destroy(window);
}

void rest_window_push(void) {
  if (s_current && window_stack_contains_window(s_current->window)) {
    if (window_stack_get_top_window() == s_current->window) {
      rest_window_refresh();
      return;
    }
    // Somewhere down the stack: replace it with one on top.
    window_stack_remove(s_current->window, false);
  }
  RestWindow *rw = malloc(sizeof(RestWindow));
  if (!rw) {
    return;
  }
  memset(rw, 0, sizeof(*rw));
  s_current = rw;
  rw->window = window_create();
  window_set_user_data(rw->window, rw);
  window_set_background_color(rw->window, GColorWhite);
  window_set_click_config_provider_with_context(rw->window, prv_click_config, rw);
  window_set_window_handlers(rw->window, (WindowHandlers){
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(rw->window, true);
}

void rest_window_refresh(void) {
  if (s_current && s_current->canvas) {
    layer_mark_dirty(s_current->canvas);
  }
}

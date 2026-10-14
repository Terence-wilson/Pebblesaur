#include "number_window.h"
#include "../ui.h"

#define NUMBER_TITLE_LEN 24
#define NUMBER_SUBTITLE_LEN 32

// Per-window state. A popped window is only unloaded after its transition,
// possibly after the next picker was pushed, so nothing here may be global.
typedef struct {
  Window *window;
  Layer *canvas;
  NumberWindowConfig config;
  char title[NUMBER_TITLE_LEN];
  char subtitle[NUMBER_SUBTITLE_LEN];
} NumberWindow;

static void prv_draw(Layer *layer, GContext *ctx) {
  NumberWindow *nw = *(NumberWindow **)layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  int w = bounds.size.w;
  int h = bounds.size.h;

  graphics_context_set_fill_color(ctx, UI_HIGHLIGHT);
  graphics_fill_rect(ctx, GRect(0, 0, w, PBL_IF_ROUND_ELSE(50, 30)), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, nw->title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, PBL_IF_ROUND_ELSE(16, 0), w, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  char value[12];
  if (nw->config.format == NUMBER_FORMAT_TENTHS) {
    snprintf(value, sizeof(value), "%d.%d", nw->config.value / 10, nw->config.value % 10);
  } else {
    snprintf(value, sizeof(value), "%d", nw->config.value);
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, value, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, h / 2 - 30, w, 50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, nw->subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(0, h / 2 + 20, w, 24), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Button hints along the right edge.
  int hint_x = w - PBL_IF_ROUND_ELSE(30, 16);
  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  graphics_context_set_text_color(ctx, UI_HIGHLIGHT);
  graphics_draw_text(ctx, "+", hint_font, GRect(hint_x, PBL_IF_ROUND_ELSE(50, 30), 14, 26),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "-", hint_font, GRect(hint_x, h - PBL_IF_ROUND_ELSE(76, 40), 14, 26),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void prv_change(NumberWindow *nw, int direction) {
  int value = nw->config.value + direction * nw->config.step;
  if (value > nw->config.max) {
    value = nw->config.max;
  }
  if (value < nw->config.min) {
    value = nw->config.min;
  }
  nw->config.value = value;
  layer_mark_dirty(nw->canvas);
}

static void prv_up(ClickRecognizerRef recognizer, void *context) {
  prv_change(context, 1);
}

static void prv_down(ClickRecognizerRef recognizer, void *context) {
  prv_change(context, -1);
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  NumberWindow *nw = context;
  // Copy out first: removing the window may free nw.
  NumberWindowCallback callback = nw->config.on_select;
  int value = nw->config.value;
  void *cb_context = nw->config.context;
  // Pop first so the callback can push the next window.
  window_stack_remove(nw->window, false);
  if (callback) {
    callback(value, cb_context);
  }
}

static void prv_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
}

static void prv_load(Window *window) {
  NumberWindow *nw = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  nw->canvas = layer_create_with_data(layer_get_bounds(root), sizeof(NumberWindow *));
  *(NumberWindow **)layer_get_data(nw->canvas) = nw;
  layer_set_update_proc(nw->canvas, prv_draw);
  layer_add_child(root, nw->canvas);
}

static void prv_unload(Window *window) {
  NumberWindow *nw = window_get_user_data(window);
  layer_destroy(nw->canvas);
  free(nw);
  window_destroy(window);
}

void number_window_push(const NumberWindowConfig *config) {
  NumberWindow *nw = malloc(sizeof(NumberWindow));
  if (!nw) {
    return;
  }
  memset(nw, 0, sizeof(*nw));
  nw->config = *config;
  // The caller's strings may not outlive this call.
  strncpy(nw->title, config->title ? config->title : "", sizeof(nw->title) - 1);
  strncpy(nw->subtitle, config->subtitle ? config->subtitle : "", sizeof(nw->subtitle) - 1);
  nw->config.title = nw->title;
  nw->config.subtitle = nw->subtitle;

  nw->window = window_create();
  window_set_user_data(nw->window, nw);
  window_set_background_color(nw->window, GColorWhite);
  window_set_click_config_provider_with_context(nw->window, prv_click_config, nw);
  window_set_window_handlers(nw->window, (WindowHandlers){
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(nw->window, true);
}

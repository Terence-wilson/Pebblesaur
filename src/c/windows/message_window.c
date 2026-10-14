#include "message_window.h"
#include "../ui.h"

#define AUTO_CLOSE_MS 6000

// Per-window state: a popped window is unloaded after its transition, so
// a new message may arrive while the old window still exists.
typedef struct {
  Window *window;
  TextLayer *title_layer;
  TextLayer *text_layer;
  AppTimer *close_timer;
  char title[24];
  char text[128];
} MessageWindow;

// The window currently on the stack, if any.
static MessageWindow *s_current;

static void prv_close(MessageWindow *mw) {
  window_stack_remove(mw->window, true);
}

static void prv_close_cb(void *context) {
  MessageWindow *mw = context;
  mw->close_timer = NULL;
  prv_close(mw);
}

static void prv_click(ClickRecognizerRef recognizer, void *context) {
  prv_close(context);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click);
}

static TextLayer *prv_text_layer(Layer *root, GRect frame, const char *text, const char *font) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_text(layer, text);
  text_layer_set_font(layer, fonts_get_system_font(font));
  text_layer_set_text_alignment(layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(layer));
  return layer;
}

static void prv_load(Window *window) {
  MessageWindow *mw = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int inset = PBL_IF_ROUND_ELSE(24, 8);
  int width = bounds.size.w - 2 * inset;
  int top = PBL_IF_ROUND_ELSE(62, 46);
  mw->title_layer = prv_text_layer(root, GRect(inset, PBL_IF_ROUND_ELSE(30, 14), width, 30), mw->title,
                                   FONT_KEY_GOTHIC_24_BOLD);
  mw->text_layer = prv_text_layer(root, GRect(inset, top, width, bounds.size.h - top - 8), mw->text,
                                  FONT_KEY_GOTHIC_18);
}

static void prv_unload(Window *window) {
  MessageWindow *mw = window_get_user_data(window);
  if (mw->close_timer) {
    app_timer_cancel(mw->close_timer);
  }
  text_layer_destroy(mw->title_layer);
  text_layer_destroy(mw->text_layer);
  if (s_current == mw) {
    s_current = NULL;
  }
  free(mw);
  window_destroy(window);
}

void message_window_push(const char *title, const char *text) {
  MessageWindow *mw = s_current;
  bool reuse = mw && window_stack_contains_window(mw->window);
  if (!reuse) {
    mw = malloc(sizeof(MessageWindow));
    if (!mw) {
      return;
    }
    memset(mw, 0, sizeof(*mw));
  }
  strncpy(mw->title, title ? title : "", sizeof(mw->title) - 1);
  mw->title[sizeof(mw->title) - 1] = '\0';
  strncpy(mw->text, text ? text : "", sizeof(mw->text) - 1);
  mw->text[sizeof(mw->text) - 1] = '\0';

  if (reuse) {
    // Already showing: just swap the text.
    text_layer_set_text(mw->title_layer, mw->title);
    text_layer_set_text(mw->text_layer, mw->text);
    if (!mw->close_timer || !app_timer_reschedule(mw->close_timer, AUTO_CLOSE_MS)) {
      mw->close_timer = app_timer_register(AUTO_CLOSE_MS, prv_close_cb, mw);
    }
    return;
  }
  s_current = mw;
  mw->window = window_create();
  window_set_user_data(mw->window, mw);
  window_set_background_color(mw->window, UI_HIGHLIGHT);
  window_set_click_config_provider_with_context(mw->window, prv_click_config, mw);
  window_set_window_handlers(mw->window, (WindowHandlers){
    .load = prv_load,
    .unload = prv_unload,
  });
  mw->close_timer = app_timer_register(AUTO_CLOSE_MS, prv_close_cb, mw);
  window_stack_push(mw->window, true);
}

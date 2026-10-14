#include "options_window.h"
#include "../ui.h"

#define OPTIONS_TITLE_LEN 40

// Per-window state: a popped window is unloaded after its transition, so
// nothing here may be global.
typedef struct {
  Window *window;
  MenuLayer *menu;
  OptionsWindowConfig config;
  char title[OPTIONS_TITLE_LEN];
} OptionsWindow;

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  OptionsWindow *ow = context;
  return ow->config.num_options;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *context) {
  return UI_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *context) {
  OptionsWindow *ow = context;
  ui_draw_header(ctx, cell, ow->title);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  OptionsWindow *ow = context;
  menu_cell_basic_draw(ctx, cell, ow->config.labels[index->row], NULL, NULL);
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *context) {
  OptionsWindow *ow = context;
  // Copy out first: removing the window may free ow.
  OptionsWindowCallback callback = ow->config.on_select;
  int id = ow->config.ids[index->row];
  void *cb_context = ow->config.context;
  window_stack_remove(ow->window, false);
  if (callback) {
    callback(id, cb_context);
  }
}

static void prv_load(Window *window) {
  OptionsWindow *ow = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  ow->menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(ow->menu, ow, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  ui_style_menu(ow->menu, window);
  layer_add_child(root, menu_layer_get_layer(ow->menu));
}

static void prv_unload(Window *window) {
  OptionsWindow *ow = window_get_user_data(window);
  menu_layer_destroy(ow->menu);
  free(ow);
  window_destroy(window);
}

void options_window_push(const OptionsWindowConfig *config) {
  OptionsWindow *ow = malloc(sizeof(OptionsWindow));
  if (!ow) {
    return;
  }
  memset(ow, 0, sizeof(*ow));
  ow->config = *config;
  if (ow->config.num_options > OPTIONS_MAX) {
    ow->config.num_options = OPTIONS_MAX;
  }
  strncpy(ow->title, config->title ? config->title : "", sizeof(ow->title) - 1);
  ow->config.title = ow->title;

  ow->window = window_create();
  window_set_user_data(ow->window, ow);
  window_set_window_handlers(ow->window, (WindowHandlers){
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(ow->window, true);
}

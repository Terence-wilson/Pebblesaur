#include "workout_window.h"
#include "../actions.h"
#include "../comm.h"
#include "../model.h"
#include "../ui.h"
#include "exercise_window.h"

#define ROW_HEIGHT PBL_IF_ROUND_ELSE(56, 46)
#define TEXT_X PBL_IF_ROUND_ELSE(46, 30)
#define CHECK_X PBL_IF_ROUND_ELSE(30, 14)

static Window *s_window;
static MenuLayer *s_menu;
static TextLayer *s_message_layer;

typedef enum {
  ROW_START,
  ROW_ENTRY,
  ROW_FINISH,
  ROW_REFRESH,
  ROW_NONE,
} RowKind;

// Rows: [Start workout] exercises... [Finish workout] Refresh
static RowKind prv_row_kind(int row, int *entry_index) {
  if (!model_started()) {
    if (row == 0) {
      return ROW_START;
    }
    row--;
  }
  int count = model_num_entries();
  if (row < count) {
    if (entry_index) {
      *entry_index = row;
    }
    return ROW_ENTRY;
  }
  row -= count;
  if (model_started()) {
    if (row == 0) {
      return ROW_FINISH;
    }
    row--;
  }
  return row == 0 ? ROW_REFRESH : ROW_NONE;
}

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  return model_num_entries() + 2;
}

static int16_t prv_row_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return ROW_HEIGHT;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *context) {
  return UI_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *context) {
  const char *day = model_day_name();
  ui_draw_header(ctx, cell, day[0] ? day : "Pebblesaur");
}

static void prv_draw_entry(GContext *ctx, const Layer *cell, int entry_index) {
  WorkoutEntry *entry = model_entry(entry_index);
  GRect bounds = layer_get_bounds(cell);
  bool highlighted = menu_cell_layer_is_highlighted(cell);
  bool done = model_entry_done(entry_index);
  ui_draw_check(ctx, GPoint(CHECK_X, bounds.size.h / 2), done, false, highlighted);

  char subtitle[48];
  int completed = model_completed_sets(entry_index);
  int total = model_working_sets(entry_index);
  int next = model_first_incomplete_set(entry_index);
  if (next >= 0) {
    char summary[28];
    ui_format_set(&entry->sets[next], summary, sizeof(summary));
    snprintf(subtitle, sizeof(subtitle), "%d/%d, %s", completed, total, summary);
  } else {
    snprintf(subtitle, sizeof(subtitle), "%d/%d sets done", completed, total);
  }

  int text_w = bounds.size.w - TEXT_X - PBL_IF_ROUND_ELSE(20, 2);
  graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, entry->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(TEXT_X, 0, text_w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(TEXT_X, 22, text_w, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  int entry_index = 0;
  switch (prv_row_kind(index->row, &entry_index)) {
    case ROW_START:
      menu_cell_basic_draw(ctx, cell, "Start workout", model_program_name(), NULL);
      break;
    case ROW_ENTRY:
      prv_draw_entry(ctx, cell, entry_index);
      break;
    case ROW_FINISH:
      menu_cell_basic_draw(ctx, cell, "Finish workout", "Save & progress", NULL);
      break;
    case ROW_REFRESH:
      menu_cell_basic_draw(ctx, cell, "Refresh", comm_busy() ? "Syncing..." : "Reload from phone", NULL);
      break;
    case ROW_NONE:
      break;
  }
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *context) {
  int entry_index = 0;
  switch (prv_row_kind(index->row, &entry_index)) {
    case ROW_START:
      comm_start_workout();
      break;
    case ROW_ENTRY:
      exercise_window_push(entry_index);
      break;
    case ROW_FINISH:
      actions_finish_workout();
      break;
    case ROW_REFRESH:
      comm_refresh();
      menu_layer_reload_data(s_menu);
      break;
    case ROW_NONE:
      break;
  }
}

static void prv_message_click(ClickRecognizerRef recognizer, void *context) {
  comm_refresh();
}

static void prv_message_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_message_click);
}

static void prv_show_state(void) {
  bool ready = model_state() == MODEL_READY;
  layer_set_hidden(menu_layer_get_layer(s_menu), !ready);
  layer_set_hidden(text_layer_get_layer(s_message_layer), ready);
  if (ready) {
    ui_style_menu(s_menu, s_window);
  } else {
    text_layer_set_text(s_message_layer, model_state() == MODEL_LOADING ? "Loading workout..." : model_message());
    window_set_click_config_provider(s_window, prv_message_click_config);
  }
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .get_cell_height = prv_row_height,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
  });
  layer_add_child(root, menu_layer_get_layer(s_menu));

  int inset = PBL_IF_ROUND_ELSE(24, 8);
  s_message_layer = text_layer_create(GRect(inset, PBL_IF_ROUND_ELSE(40, 20), bounds.size.w - 2 * inset,
                                            bounds.size.h - PBL_IF_ROUND_ELSE(60, 28)));
  text_layer_set_font(s_message_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_message_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_overflow_mode(s_message_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_message_layer));

  prv_show_state();
}

static void prv_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  text_layer_destroy(s_message_layer);
  s_message_layer = NULL;
}

void workout_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load,
      .unload = prv_unload,
    });
  }
  window_stack_push(s_window, true);
}

void workout_window_refresh(void) {
  if (!s_menu) {
    return;
  }
  menu_layer_reload_data(s_menu);
  uint16_t rows = prv_num_rows(s_menu, 0, NULL);
  MenuIndex selected = menu_layer_get_selected_index(s_menu);
  if (selected.row >= rows) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, rows - 1), MenuRowAlignCenter, false);
  }
  prv_show_state();
}

void workout_window_redraw(void) {
  if (s_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

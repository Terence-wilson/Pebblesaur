#include "exercise_window.h"
#include "../actions.h"
#include "../model.h"
#include "../ui.h"

#define ROW_HEIGHT PBL_IF_ROUND_ELSE(56, 46)
#define TEXT_X PBL_IF_ROUND_ELSE(46, 30)
#define CHECK_X PBL_IF_ROUND_ELSE(30, 14)

// Per-window state: a popped window is unloaded after its transition, so it
// may still exist when the next one is pushed.
typedef struct {
  Window *window;
  MenuLayer *menu;
  int entry_index;
  char header[NAME_LEN];
} ExerciseWindow;

// The newest exercise window, until it unloads.
static ExerciseWindow *s_current;

typedef enum {
  ROW_SET,
  ROW_NEXT,
  ROW_PREV,
  ROW_NONE,
} RowKind;

static bool prv_has_next(ExerciseWindow *ew) {
  return ew->entry_index + 1 < model_num_entries();
}

static bool prv_has_prev(ExerciseWindow *ew) {
  return ew->entry_index > 0;
}

static int prv_num_sets(ExerciseWindow *ew) {
  WorkoutEntry *entry = model_entry(ew->entry_index);
  return entry ? entry->num_sets : 0;
}

static RowKind prv_row_kind(ExerciseWindow *ew, int row) {
  int sets = prv_num_sets(ew);
  if (row < sets) {
    return ROW_SET;
  }
  row -= sets;
  if (prv_has_next(ew)) {
    if (row == 0) {
      return ROW_NEXT;
    }
    row--;
  }
  if (prv_has_prev(ew) && row == 0) {
    return ROW_PREV;
  }
  return ROW_NONE;
}

static void prv_update_header(ExerciseWindow *ew) {
  WorkoutEntry *entry = model_entry(ew->entry_index);
  snprintf(ew->header, sizeof(ew->header), "%s", entry ? entry->name : "");
}

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *context) {
  ExerciseWindow *ew = context;
  return prv_num_sets(ew) + (prv_has_next(ew) ? 1 : 0) + (prv_has_prev(ew) ? 1 : 0);
}

static int16_t prv_row_height(MenuLayer *menu, MenuIndex *index, void *context) {
  return ROW_HEIGHT;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *context) {
  return UI_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *context) {
  ExerciseWindow *ew = context;
  ui_draw_header(ctx, cell, ew->header);
}

static void prv_draw_set(ExerciseWindow *ew, GContext *ctx, const Layer *cell, int set_index) {
  WorkoutSet *set = model_set(ew->entry_index, set_index);
  if (!set) {
    return;
  }
  GRect bounds = layer_get_bounds(cell);
  bool highlighted = menu_cell_layer_is_highlighted(cell);
  bool done = set->flags & SET_FLAG_COMPLETED;

  ui_draw_check(ctx, GPoint(CHECK_X, bounds.size.h / 2), done, set->flags & SET_FLAG_PENDING, highlighted);

  char label[4];
  char summary[28];
  char title[36];
  char details[40];
  model_set_label(ew->entry_index, set_index, label, sizeof(label));
  ui_format_set(set, summary, sizeof(summary));
  snprintf(title, sizeof(title), "%s  %s", label, summary);
  ui_format_set_details(set, details, sizeof(details));

  int text_w = bounds.size.w - TEXT_X - PBL_IF_ROUND_ELSE(20, 2);
  graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(TEXT_X, -2, text_w, 28), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, details, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(TEXT_X, 25, text_w, 18), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  ExerciseWindow *ew = context;
  switch (prv_row_kind(ew, index->row)) {
    case ROW_SET:
      prv_draw_set(ew, ctx, cell, index->row);
      break;
    case ROW_NEXT:
      menu_cell_basic_draw(ctx, cell, "Next exercise", model_entry(ew->entry_index + 1)->name, NULL);
      break;
    case ROW_PREV:
      menu_cell_basic_draw(ctx, cell, "Previous exercise", model_entry(ew->entry_index - 1)->name, NULL);
      break;
    case ROW_NONE:
      break;
  }
}

static void prv_select_next(ExerciseWindow *ew, bool animated) {
  int row = model_first_incomplete_set(ew->entry_index);
  if (row < 0) {
    // Every set is done: point at "Next exercise", or the last set.
    row = prv_num_sets(ew) - (prv_has_next(ew) ? 0 : 1);
  }
  if (row < 0) {
    row = 0;
  }
  menu_layer_set_selected_index(ew->menu, MenuIndex(0, row), MenuRowAlignCenter, animated);
}

static void prv_show_entry(ExerciseWindow *ew, int entry_index) {
  ew->entry_index = entry_index;
  prv_update_header(ew);
  menu_layer_reload_data(ew->menu);
  prv_select_next(ew, false);
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *context) {
  ExerciseWindow *ew = context;
  switch (prv_row_kind(ew, index->row)) {
    case ROW_SET: {
      WorkoutSet *set = model_set(ew->entry_index, index->row);
      if (!set) {
        break;
      }
      if (set->flags & SET_FLAG_COMPLETED) {
        actions_edit_set(ew->entry_index, index->row);
      } else {
        actions_complete_set(ew->entry_index, index->row);
      }
      break;
    }
    case ROW_NEXT:
      prv_show_entry(ew, ew->entry_index + 1);
      break;
    case ROW_PREV:
      prv_show_entry(ew, ew->entry_index - 1);
      break;
    case ROW_NONE:
      break;
  }
}

static void prv_long_select(MenuLayer *menu, MenuIndex *index, void *context) {
  ExerciseWindow *ew = context;
  if (prv_row_kind(ew, index->row) == ROW_SET) {
    actions_set_options(ew->entry_index, index->row);
  }
}

static void prv_load(Window *window) {
  ExerciseWindow *ew = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  ew->menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(ew->menu, ew, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .get_cell_height = prv_row_height,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
    .select_long_click = prv_long_select,
  });
  ui_style_menu(ew->menu, window);
  layer_add_child(root, menu_layer_get_layer(ew->menu));
  prv_update_header(ew);
  prv_select_next(ew, false);
}

static void prv_unload(Window *window) {
  ExerciseWindow *ew = window_get_user_data(window);
  menu_layer_destroy(ew->menu);
  if (s_current == ew) {
    s_current = NULL;
  }
  free(ew);
  window_destroy(window);
}

// The current window, if it is on the stack and loaded.
static ExerciseWindow *prv_live(void) {
  if (s_current && s_current->menu && window_stack_contains_window(s_current->window)) {
    return s_current;
  }
  return NULL;
}

void exercise_window_push(int entry_index) {
  if (!model_entry(entry_index)) {
    return;
  }
  ExerciseWindow *old = prv_live();
  if (old) {
    window_stack_remove(old->window, false);
  }
  ExerciseWindow *ew = malloc(sizeof(ExerciseWindow));
  if (!ew) {
    return;
  }
  memset(ew, 0, sizeof(*ew));
  ew->entry_index = entry_index;
  s_current = ew;
  ew->window = window_create();
  window_set_user_data(ew->window, ew);
  window_set_window_handlers(ew->window, (WindowHandlers){
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(ew->window, true);
}

void exercise_window_refresh(void) {
  ExerciseWindow *ew = prv_live();
  if (!ew) {
    return;
  }
  if (model_state() != MODEL_READY || !model_entry(ew->entry_index)) {
    // The workout went away (finished, or changed on the phone).
    window_stack_remove(ew->window, true);
    return;
  }
  prv_update_header(ew);
  MenuIndex selected = menu_layer_get_selected_index(ew->menu);
  menu_layer_reload_data(ew->menu);
  uint16_t rows = prv_num_rows(ew->menu, 0, ew);
  if (rows > 0 && selected.row >= rows) {
    menu_layer_set_selected_index(ew->menu, MenuIndex(0, rows - 1), MenuRowAlignCenter, false);
  }
}

void exercise_window_redraw(void) {
  ExerciseWindow *ew = prv_live();
  if (ew) {
    layer_mark_dirty(menu_layer_get_layer(ew->menu));
  }
}

void exercise_window_select_next(void) {
  ExerciseWindow *ew = prv_live();
  if (ew) {
    prv_select_next(ew, true);
  }
}

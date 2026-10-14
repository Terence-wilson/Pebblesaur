#include "actions.h"
#include "comm.h"
#include "model.h"
#include "rest.h"
#include "ui.h"
#include "windows/exercise_window.h"
#include "windows/number_window.h"
#include "windows/options_window.h"
#include "windows/rest_window.h"

#define DEFAULT_RPE10 80

typedef struct {
  int entry_index;
  int set_index;
  int reps;
  int rpe10;
  bool start_rest;
} PendingSet;

static PendingSet s_pending;

static void prv_next_label(int entry_index, int set_index, char *buf, size_t size) {
  WorkoutEntry *entry = model_entry(entry_index);
  for (int i = set_index + 1; entry && i < entry->num_sets; i++) {
    WorkoutSet *set = &entry->sets[i];
    if (!(set->flags & SET_FLAG_COMPLETED)) {
      char label[4];
      char summary[24];
      model_set_label(entry_index, i, label, sizeof(label));
      ui_format_set(set, summary, sizeof(summary));
      snprintf(buf, size, "Next: set %s, %s", label, summary);
      return;
    }
  }
  int count = model_num_entries();
  for (int n = 1; n < count; n++) {
    int e = (entry_index + n) % count;
    if (!model_entry_done(e)) {
      snprintf(buf, size, "Next: %s", model_entry(e)->name);
      return;
    }
  }
  snprintf(buf, size, "All sets done!");
}

static void prv_log(void) {
  WorkoutSet *set = model_set(s_pending.entry_index, s_pending.set_index);
  if (!set) {
    return;
  }
  int timer = set->timer;
  model_mark_completed(s_pending.entry_index, s_pending.set_index, s_pending.reps, s_pending.rpe10);
  comm_complete_set(s_pending.entry_index, s_pending.set_index, s_pending.reps, s_pending.rpe10);
  exercise_window_select_next();
  if (s_pending.start_rest && timer > 0) {
    char label[40];
    prv_next_label(s_pending.entry_index, s_pending.set_index, label, sizeof(label));
    rest_start(timer, label);
    rest_window_push();
  }
}

static void prv_rpe_selected(int value, void *context) {
  s_pending.rpe10 = value;
  prv_log();
}

static void prv_ask_rpe(void) {
  WorkoutSet *set = model_set(s_pending.entry_index, s_pending.set_index);
  int initial = set->done_rpe10 >= 0 ? set->done_rpe10 : (set->rpe10 >= 0 ? set->rpe10 : DEFAULT_RPE10);
  char subtitle[24] = "How hard was it?";
  if (set->rpe10 >= 0) {
    snprintf(subtitle, sizeof(subtitle), "Target %d.%d", set->rpe10 / 10, set->rpe10 % 10);
  }
  NumberWindowConfig config = {
    .title = "RPE",
    .subtitle = subtitle,
    .value = initial,
    .min = 10,
    .max = 100,
    .step = 5,
    .format = NUMBER_FORMAT_TENTHS,
    .on_select = prv_rpe_selected,
  };
  number_window_push(&config);
}

static void prv_reps_selected(int value, void *context) {
  s_pending.reps = value;
  WorkoutSet *set = model_set(s_pending.entry_index, s_pending.set_index);
  if (set && (set->flags & SET_FLAG_LOG_RPE)) {
    prv_ask_rpe();
  } else {
    prv_log();
  }
}

static void prv_ask_reps(void) {
  WorkoutSet *set = model_set(s_pending.entry_index, s_pending.set_index);
  char subtitle[24];
  if (set->min_reps >= 0 && set->min_reps != set->reps) {
    snprintf(subtitle, sizeof(subtitle), "Target %d-%d", set->min_reps, set->reps);
  } else {
    snprintf(subtitle, sizeof(subtitle), "Target %d%s", set->reps, (set->flags & SET_FLAG_AMRAP) ? "+" : "");
  }
  if (set->weight[0]) {
    size_t len = strlen(subtitle);
    snprintf(subtitle + len, sizeof(subtitle) - len, " @ %s", set->weight);
  }
  NumberWindowConfig config = {
    .title = "Reps",
    .subtitle = subtitle,
    .value = s_pending.reps,
    .min = 0,
    .max = 99,
    .step = 1,
    .format = NUMBER_FORMAT_INT,
    .on_select = prv_reps_selected,
  };
  number_window_push(&config);
}

static bool prv_begin(int entry_index, int set_index) {
  WorkoutSet *set = model_set(entry_index, set_index);
  if (!set) {
    return false;
  }
  s_pending = (PendingSet){
    .entry_index = entry_index,
    .set_index = set_index,
    .reps = set->done_reps >= 0 ? set->done_reps : (set->reps >= 0 ? set->reps : 0),
    .rpe10 = set->done_rpe10,
    .start_rest = !(set->flags & SET_FLAG_COMPLETED),
  };
  return true;
}

void actions_complete_set(int entry_index, int set_index) {
  if (!prv_begin(entry_index, set_index)) {
    return;
  }
  WorkoutSet *set = model_set(entry_index, set_index);
  if (set->flags & SET_FLAG_AMRAP) {
    prv_ask_reps();
  } else if (set->flags & SET_FLAG_LOG_RPE) {
    prv_ask_rpe();
  } else {
    prv_log();
  }
}

void actions_edit_set(int entry_index, int set_index) {
  if (prv_begin(entry_index, set_index)) {
    prv_ask_reps();
  }
}

enum {
  OPTION_EDIT_REPS,
  OPTION_COMPLETE,
  OPTION_UNDO,
  OPTION_REST,
};

static void prv_option_selected(int option, void *context) {
  int entry_index = s_pending.entry_index;
  int set_index = s_pending.set_index;
  WorkoutSet *set = model_set(entry_index, set_index);
  if (!set) {
    return;
  }
  switch (option) {
    case OPTION_EDIT_REPS:
      actions_edit_set(entry_index, set_index);
      break;
    case OPTION_COMPLETE:
      actions_complete_set(entry_index, set_index);
      break;
    case OPTION_UNDO:
      model_mark_uncompleted(entry_index, set_index);
      comm_uncomplete_set(entry_index, set_index);
      break;
    case OPTION_REST: {
      char label[40];
      prv_next_label(entry_index, set_index, label, sizeof(label));
      rest_start(set->timer > 0 ? set->timer : 90, label);
      rest_window_push();
      break;
    }
  }
}

void actions_set_options(int entry_index, int set_index) {
  WorkoutSet *set = model_set(entry_index, set_index);
  if (!set) {
    return;
  }
  s_pending.entry_index = entry_index;
  s_pending.set_index = set_index;

  char label[4];
  char title[16];
  model_set_label(entry_index, set_index, label, sizeof(label));
  snprintf(title, sizeof(title), "Set %s", label);

  OptionsWindowConfig config = {
    .title = title,
    .on_select = prv_option_selected,
  };
  bool done = set->flags & SET_FLAG_COMPLETED;
  int n = 0;
  config.labels[n] = done ? "Change reps" : "Log with different reps";
  config.ids[n++] = OPTION_EDIT_REPS;
  if (done) {
    config.labels[n] = "Mark not done";
    config.ids[n++] = OPTION_UNDO;
  } else {
    config.labels[n] = "Complete set";
    config.ids[n++] = OPTION_COMPLETE;
  }
  config.labels[n] = "Start rest timer";
  config.ids[n++] = OPTION_REST;
  config.num_options = n;
  options_window_push(&config);
}

enum {
  FINISH_SAVE,
  FINISH_CANCEL,
};

static void prv_finish_selected(int option, void *context) {
  if (option == FINISH_SAVE) {
    rest_stop();
    comm_finish_workout();
  }
}

void actions_finish_workout(void) {
  OptionsWindowConfig config = {
    .title = "Finish workout?",
    .num_options = 2,
    .labels = {"Save to Liftosaur", "Keep going"},
    .ids = {FINISH_SAVE, FINISH_CANCEL},
    .on_select = prv_finish_selected,
  };
  options_window_push(&config);
}

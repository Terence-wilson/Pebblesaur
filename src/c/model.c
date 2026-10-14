#include "model.h"

static ModelChangedHandler s_on_change;
static ModelState s_state = MODEL_LOADING;
static char s_message[96];

static bool s_started;
static char s_program[TITLE_LEN];
static char s_day[TITLE_LEN];
static int s_num_entries;
static WorkoutEntry s_entries[MAX_ENTRIES];

static void prv_changed(void) {
  if (s_on_change) {
    s_on_change();
  }
}

void model_init(ModelChangedHandler on_change) {
  s_on_change = on_change;
  s_state = MODEL_LOADING;
  s_num_entries = 0;
}

ModelState model_state(void) {
  return s_state;
}

const char *model_message(void) {
  return s_message;
}

void model_set_message(const char *text) {
  strncpy(s_message, text, sizeof(s_message) - 1);
  s_message[sizeof(s_message) - 1] = '\0';
  s_state = MODEL_MESSAGE;
  s_num_entries = 0;
  prv_changed();
}

bool model_started(void) {
  return s_started;
}

const char *model_program_name(void) {
  return s_program;
}

const char *model_day_name(void) {
  return s_day;
}

int model_num_entries(void) {
  return s_num_entries;
}

WorkoutEntry *model_entry(int index) {
  if (index < 0 || index >= s_num_entries) {
    return NULL;
  }
  return &s_entries[index];
}

WorkoutSet *model_set(int entry_index, int set_index) {
  WorkoutEntry *entry = model_entry(entry_index);
  if (!entry || set_index < 0 || set_index >= entry->num_sets) {
    return NULL;
  }
  return &entry->sets[set_index];
}

int model_first_incomplete_set(int entry_index) {
  WorkoutEntry *entry = model_entry(entry_index);
  if (!entry) {
    return -1;
  }
  // Prefer working sets: most lifters don't log warmups.
  for (int i = 0; i < entry->num_sets; i++) {
    uint8_t flags = entry->sets[i].flags;
    if (!(flags & SET_FLAG_WARMUP) && !(flags & SET_FLAG_COMPLETED)) {
      return i;
    }
  }
  return -1;
}

int model_completed_sets(int entry_index) {
  WorkoutEntry *entry = model_entry(entry_index);
  int count = 0;
  for (int i = 0; entry && i < entry->num_sets; i++) {
    uint8_t flags = entry->sets[i].flags;
    if (!(flags & SET_FLAG_WARMUP) && (flags & SET_FLAG_COMPLETED)) {
      count++;
    }
  }
  return count;
}

int model_working_sets(int entry_index) {
  WorkoutEntry *entry = model_entry(entry_index);
  int count = 0;
  for (int i = 0; entry && i < entry->num_sets; i++) {
    if (!(entry->sets[i].flags & SET_FLAG_WARMUP)) {
      count++;
    }
  }
  return count;
}

bool model_entry_done(int entry_index) {
  return model_working_sets(entry_index) > 0 && model_first_incomplete_set(entry_index) < 0;
}

void model_set_label(int entry_index, int set_index, char *buf, size_t size) {
  WorkoutEntry *entry = model_entry(entry_index);
  if (!entry) {
    buf[0] = '\0';
    return;
  }
  int warmups = 0;
  int working = 0;
  for (int i = 0; i <= set_index && i < entry->num_sets; i++) {
    if (entry->sets[i].flags & SET_FLAG_WARMUP) {
      warmups++;
    } else {
      working++;
    }
  }
  if (entry->sets[set_index].flags & SET_FLAG_WARMUP) {
    snprintf(buf, size, "W%d", warmups);
  } else {
    snprintf(buf, size, "%d", working);
  }
}

void model_begin_workout(bool started, int count, const char *program, const char *day) {
  s_started = started;
  s_num_entries = count > MAX_ENTRIES ? MAX_ENTRIES : (count < 0 ? 0 : count);
  strncpy(s_program, program ? program : "", sizeof(s_program) - 1);
  s_program[sizeof(s_program) - 1] = '\0';
  strncpy(s_day, day ? day : "", sizeof(s_day) - 1);
  s_day[sizeof(s_day) - 1] = '\0';
  memset(s_entries, 0, sizeof(s_entries));
}

// Reads a (possibly negative) integer and advances *p past it and one trailing
// separator.
static int prv_read_int(const char **p) {
  const char *s = *p;
  bool negative = false;
  int value = 0;
  if (*s == '-') {
    negative = true;
    s++;
  }
  while (*s >= '0' && *s <= '9') {
    value = value * 10 + (*s - '0');
    s++;
  }
  if (*s == ',') {
    s++;
  }
  *p = s;
  return negative ? -value : value;
}

static int8_t prv_clamp8(int value) {
  if (value > 127) {
    return 127;
  }
  if (value < -1) {
    return NO_VALUE;
  }
  return (int8_t)value;
}

// Sets arrive as "flags,reps,minReps,timer,doneReps,rpe10,doneRpe10,weight"
// joined with ';'. The weight goes last because it is free text.
static int prv_parse_sets(const char *text, WorkoutSet *sets, int max) {
  int count = 0;
  const char *p = text;
  while (p && *p && count < max) {
    WorkoutSet *set = &sets[count];
    set->flags = (uint8_t)prv_read_int(&p);
    set->reps = prv_clamp8(prv_read_int(&p));
    set->min_reps = prv_clamp8(prv_read_int(&p));
    set->timer = (int16_t)prv_read_int(&p);
    set->done_reps = prv_clamp8(prv_read_int(&p));
    set->rpe10 = prv_clamp8(prv_read_int(&p));
    set->done_rpe10 = prv_clamp8(prv_read_int(&p));
    int len = 0;
    while (*p && *p != ';') {
      if (len < WEIGHT_LEN - 1) {
        set->weight[len++] = *p;
      }
      p++;
    }
    set->weight[len] = '\0';
    if (*p == ';') {
      p++;
    }
    count++;
  }
  return count;
}

void model_set_entry(int index, const char *name, const char *info, const char *sets) {
  if (index < 0 || index >= s_num_entries) {
    return;
  }
  WorkoutEntry *entry = &s_entries[index];
  strncpy(entry->name, name ? name : "", NAME_LEN - 1);
  entry->name[NAME_LEN - 1] = '\0';
  strncpy(entry->info, info ? info : "", INFO_LEN - 1);
  entry->info[INFO_LEN - 1] = '\0';
  entry->num_sets = prv_parse_sets(sets, entry->sets, MAX_SETS);
}

void model_end_workout(void) {
  s_state = MODEL_READY;
  prv_changed();
}

void model_mark_completed(int entry_index, int set_index, int reps, int rpe10) {
  WorkoutSet *set = model_set(entry_index, set_index);
  if (!set) {
    return;
  }
  set->flags |= SET_FLAG_COMPLETED | SET_FLAG_PENDING;
  set->done_reps = prv_clamp8(reps);
  set->done_rpe10 = prv_clamp8(rpe10);
  prv_changed();
}

void model_mark_uncompleted(int entry_index, int set_index) {
  WorkoutSet *set = model_set(entry_index, set_index);
  if (!set) {
    return;
  }
  set->flags &= ~SET_FLAG_COMPLETED;
  set->flags |= SET_FLAG_PENDING;
  set->done_reps = NO_VALUE;
  set->done_rpe10 = NO_VALUE;
  prv_changed();
}

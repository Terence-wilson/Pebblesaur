#pragma once

#include <pebble.h>

// The watch keeps a compact copy of the workout. The phone (PebbleKit JS) owns
// the real Liftosaur workout and re-sends this copy after every change.

#define MAX_ENTRIES 16
#define MAX_SETS 12
#define NAME_LEN 32
#define INFO_LEN 32
#define WEIGHT_LEN 12
#define TITLE_LEN 40

#define NO_VALUE -1

// Set flags, mirrored in src/pkjs/protocol.js.
enum {
  SET_FLAG_WARMUP = 1 << 0,
  SET_FLAG_AMRAP = 1 << 1,
  SET_FLAG_LOG_RPE = 1 << 2,
  SET_FLAG_ASK_WEIGHT = 1 << 3,
  SET_FLAG_COMPLETED = 1 << 4,
  SET_FLAG_UNILATERAL = 1 << 5,
  SET_FLAG_TIMED = 1 << 6,
  // Watch-only: completed locally, waiting for the phone to confirm.
  SET_FLAG_PENDING = 1 << 7,
};

typedef struct {
  uint8_t flags;
  int8_t reps;       // Target reps
  int8_t min_reps;   // Lower bound of a rep range, or NO_VALUE
  int8_t done_reps;  // Logged reps, or NO_VALUE
  int8_t rpe10;      // Target RPE x10, or NO_VALUE
  int8_t done_rpe10; // Logged RPE x10, or NO_VALUE
  int16_t timer;     // Rest after this set, in seconds
  char weight[WEIGHT_LEN];
} WorkoutSet;

typedef struct {
  char name[NAME_LEN];
  char info[INFO_LEN];
  uint8_t num_sets;
  WorkoutSet sets[MAX_SETS];
} WorkoutEntry;

typedef enum {
  MODEL_LOADING,
  MODEL_MESSAGE, // No workout to show, see model_message()
  MODEL_READY,
} ModelState;

typedef void (*ModelChangedHandler)(void);

void model_init(ModelChangedHandler on_change);

ModelState model_state(void);
const char *model_message(void);
void model_set_message(const char *text);

bool model_started(void);
const char *model_program_name(void);
const char *model_day_name(void);
int model_num_entries(void);
WorkoutEntry *model_entry(int index);
WorkoutSet *model_set(int entry_index, int set_index);

// Index of the first set in the entry that isn't completed, or -1.
int model_first_incomplete_set(int entry_index);
int model_completed_sets(int entry_index);
// Number of working (non-warmup) sets.
int model_working_sets(int entry_index);
bool model_entry_done(int entry_index);

// Display label for a set: "W1", "W2" for warmups, then "1", "2", ...
void model_set_label(int entry_index, int set_index, char *buf, size_t size);

// Updates from the phone.
void model_begin_workout(bool started, int count, const char *program, const char *day);
void model_set_entry(int index, const char *name, const char *info, const char *sets);
void model_end_workout(void);

// Optimistic local changes while the phone talks to Liftosaur.
void model_mark_completed(int entry_index, int set_index, int reps, int rpe10);
void model_mark_uncompleted(int entry_index, int set_index);

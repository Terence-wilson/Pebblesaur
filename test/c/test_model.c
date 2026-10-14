// Native tests for the watch model: run with `make -C test/c`.
#include <assert.h>
#include "../../src/c/model.h"

// Same strings as test/protocol.test.js produces from the fixture.
#define SQUAT_SETS "1,5,-1,90,-1,-1,-1,60kg;16,5,-1,180,6,-1,85,102.5kg;6,5,3,180,-1,90,-1,100kg"
#define PLANK_SETS "96,1,-1,60,-1,-1,-1,"

static int s_changes;
static void on_change(void) { s_changes++; }

static void load_fixture(void) {
  model_begin_workout(true, 2, "GZCLP", "A1");
  model_set_entry(0, "Squat", "T1 - work up to a 5RM", SQUAT_SETS);
  model_set_entry(1, "Side Plank", "", PLANK_SETS);
  model_end_workout();
}

static void test_parse(void) {
  load_fixture();
  assert(model_state() == MODEL_READY);
  assert(model_started());
  assert(strcmp(model_day_name(), "A1") == 0);
  assert(model_num_entries() == 2);

  WorkoutEntry *squat = model_entry(0);
  assert(squat->num_sets == 3);
  WorkoutSet *warmup = &squat->sets[0];
  assert(warmup->flags == SET_FLAG_WARMUP && warmup->reps == 5 && warmup->timer == 90);
  assert(strcmp(warmup->weight, "60kg") == 0);

  WorkoutSet *done = &squat->sets[1];
  assert(done->flags & SET_FLAG_COMPLETED);
  assert(done->done_reps == 6 && done->done_rpe10 == 85 && done->rpe10 == NO_VALUE);
  assert(strcmp(done->weight, "102.5kg") == 0);

  WorkoutSet *amrap = &squat->sets[2];
  assert(amrap->flags == (SET_FLAG_AMRAP | SET_FLAG_LOG_RPE));
  assert(amrap->min_reps == 3 && amrap->reps == 5 && amrap->rpe10 == 90 && amrap->timer == 180);

  WorkoutSet *plank = model_set(1, 0);
  assert(plank->flags == (SET_FLAG_UNILATERAL | SET_FLAG_TIMED));
  assert(plank->weight[0] == '\0');
  assert(model_set(1, 1) == NULL && model_set(2, 0) == NULL);
}

static void test_progress(void) {
  load_fixture();
  char label[4];
  model_set_label(0, 0, label, sizeof(label));
  assert(strcmp(label, "W1") == 0);
  model_set_label(0, 2, label, sizeof(label));
  assert(strcmp(label, "2") == 0);

  // Warmups are skipped when finding the next set.
  assert(model_first_incomplete_set(0) == 2);
  assert(model_working_sets(0) == 2 && model_completed_sets(0) == 1);
  assert(!model_entry_done(0));

  s_changes = 0;
  model_mark_completed(0, 2, 8, 95);
  assert(s_changes == 1);
  WorkoutSet *set = model_set(0, 2);
  assert(set->flags & SET_FLAG_COMPLETED && set->flags & SET_FLAG_PENDING);
  assert(set->done_reps == 8 && set->done_rpe10 == 95);
  assert(model_entry_done(0) && model_first_incomplete_set(0) == -1);

  model_mark_uncompleted(0, 2);
  assert(!(set->flags & SET_FLAG_COMPLETED) && set->done_reps == NO_VALUE);
  assert(model_first_incomplete_set(0) == 2);
}

static void test_limits(void) {
  // Long weights are cut, extra sets dropped, counts clamped.
  char sets[1024] = "";
  for (int i = 0; i < MAX_SETS + 4; i++) {
    strcat(sets, "0,5,-1,60,-1,-1,-1,1234567890123456kg;");
  }
  model_begin_workout(false, MAX_ENTRIES + 5, "P", "D");
  assert(model_num_entries() == MAX_ENTRIES);
  model_set_entry(0, "A name that is far too long to fit in the watch buffer", "", sets);
  model_set_entry(MAX_ENTRIES, "ignored", "", sets);
  model_end_workout();
  WorkoutEntry *entry = model_entry(0);
  assert(entry->num_sets == MAX_SETS);
  assert(strlen(entry->sets[0].weight) == WEIGHT_LEN - 1);
  assert(strlen(entry->name) == NAME_LEN - 1);
  assert(entry->sets[MAX_SETS - 1].reps == 5);
}

static void test_message(void) {
  load_fixture();
  model_set_message("Add your API key");
  assert(model_state() == MODEL_MESSAGE);
  assert(model_num_entries() == 0);
  assert(strcmp(model_message(), "Add your API key") == 0);
}

int main(void) {
  model_init(on_change);
  assert(model_state() == MODEL_LOADING);
  test_parse();
  test_progress();
  test_limits();
  test_message();
  printf("model tests passed\n");
  return 0;
}

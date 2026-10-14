#pragma once

#include <pebble.h>

// Completes a set with its target reps. AMRAP sets ask for reps and RPE sets
// ask for RPE first. Starts the rest timer.
void actions_complete_set(int entry_index, int set_index);

// Asks for reps (and RPE if the set logs it), then logs the set. Does not
// start the rest timer when the set was already done.
void actions_edit_set(int entry_index, int set_index);

// Long-press menu for a set: edit reps, mark not done, start rest.
void actions_set_options(int entry_index, int set_index);

// Asks for confirmation, then saves the workout to Liftosaur.
void actions_finish_workout(void);

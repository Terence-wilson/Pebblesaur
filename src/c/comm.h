#pragma once

#include <pebble.h>

// Actions sent to the phone, mirrored in src/pkjs/protocol.js.
typedef enum {
  ACTION_REFRESH = 1,
  ACTION_START = 2,
  ACTION_COMPLETE_SET = 3,
  ACTION_UNCOMPLETE_SET = 4,
  ACTION_FINISH = 5,
} CommAction;

// Message types received from the phone.
typedef enum {
  MSG_STATUS = 1,   // Text: no workout to show (not configured, loading, error)
  MSG_WORKOUT = 2,  // Started, Count, ProgramName, DayName: a workout follows
  MSG_ENTRY = 3,    // EntryIndex, Name, Info, Sets
  MSG_DONE = 4,     // The whole workout has arrived
  MSG_ERROR = 5,    // Text: an action failed, the workout is unchanged
  MSG_FINISHED = 6, // Text: the workout was saved
} CommMessageType;

typedef void (*CommNoticeHandler)(CommMessageType type, const char *text);

void comm_init(CommNoticeHandler on_notice);
void comm_deinit(void);

// True until the phone has answered the last action.
bool comm_busy(void);

void comm_refresh(void);
void comm_start_workout(void);
void comm_complete_set(int entry_index, int set_index, int reps, int rpe10);
void comm_uncomplete_set(int entry_index, int set_index);
void comm_finish_workout(void);

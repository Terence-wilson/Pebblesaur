#include "comm.h"
#include "model.h"

#define QUEUE_SIZE 8
#define MAX_RETRIES 5
#define RETRY_MS 1000

typedef struct {
  CommAction action;
  int16_t entry_index;
  int16_t set_index;
  int16_t reps;
  int16_t rpe10;
} QueuedAction;

static CommNoticeHandler s_on_notice;
static QueuedAction s_queue[QUEUE_SIZE];
static int s_queue_len;
static bool s_sending;
static int s_retries;
static int s_awaiting;
static AppTimer *s_retry_timer;

static void prv_send_next(void);

static void prv_retry_cb(void *context) {
  s_retry_timer = NULL;
  prv_send_next();
}

static void prv_schedule_retry(void) {
  if (!s_retry_timer) {
    s_retry_timer = app_timer_register(RETRY_MS, prv_retry_cb, NULL);
  }
}

static void prv_send_next(void) {
  if (s_sending || s_queue_len == 0) {
    return;
  }
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    prv_schedule_retry();
    return;
  }
  QueuedAction *a = &s_queue[0];
  dict_write_int32(iter, MESSAGE_KEY_Action, a->action);
  dict_write_int32(iter, MESSAGE_KEY_EntryIndex, a->entry_index);
  dict_write_int32(iter, MESSAGE_KEY_SetIndex, a->set_index);
  dict_write_int32(iter, MESSAGE_KEY_Reps, a->reps);
  dict_write_int32(iter, MESSAGE_KEY_Rpe, a->rpe10);
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_sending = true;
  } else {
    prv_schedule_retry();
  }
}

static void prv_pop(void) {
  if (s_queue_len == 0) {
    return;
  }
  memmove(&s_queue[0], &s_queue[1], sizeof(QueuedAction) * (s_queue_len - 1));
  s_queue_len--;
}

static void prv_outbox_sent(DictionaryIterator *iter, void *context) {
  s_sending = false;
  s_retries = 0;
  prv_pop();
  s_awaiting++;
  prv_send_next();
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  s_sending = false;
  if (++s_retries > MAX_RETRIES) {
    s_retries = 0;
    prv_pop();
    if (s_on_notice) {
      s_on_notice(MSG_ERROR, "Can't reach phone");
    }
  }
  prv_schedule_retry();
}

static const char *prv_cstring(DictionaryIterator *iter, uint32_t key) {
  Tuple *t = dict_find(iter, key);
  return (t && t->type == TUPLE_CSTRING) ? t->value->cstring : "";
}

static int prv_int(DictionaryIterator *iter, uint32_t key, int fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) {
    return fallback;
  }
  switch (t->type) {
    case TUPLE_INT:
    case TUPLE_UINT:
      return t->length == 1 ? t->value->int8 : (t->length == 2 ? t->value->int16 : (int)t->value->int32);
    default:
      return fallback;
  }
}

static void prv_answered(void) {
  if (s_awaiting > 0) {
    s_awaiting--;
  }
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  CommMessageType type = (CommMessageType)prv_int(iter, MESSAGE_KEY_MsgType, 0);
  switch (type) {
    case MSG_STATUS:
      s_awaiting = 0;
      model_set_message(prv_cstring(iter, MESSAGE_KEY_Text));
      break;
    case MSG_WORKOUT:
      model_begin_workout(prv_int(iter, MESSAGE_KEY_Started, 0) != 0,
                          prv_int(iter, MESSAGE_KEY_Count, 0),
                          prv_cstring(iter, MESSAGE_KEY_ProgramName),
                          prv_cstring(iter, MESSAGE_KEY_DayName));
      break;
    case MSG_ENTRY:
      model_set_entry(prv_int(iter, MESSAGE_KEY_EntryIndex, -1),
                      prv_cstring(iter, MESSAGE_KEY_Name),
                      prv_cstring(iter, MESSAGE_KEY_Info),
                      prv_cstring(iter, MESSAGE_KEY_Sets));
      break;
    case MSG_DONE:
      prv_answered();
      model_end_workout();
      break;
    case MSG_ERROR:
    case MSG_FINISHED:
      prv_answered();
      if (s_on_notice) {
        s_on_notice(type, prv_cstring(iter, MESSAGE_KEY_Text));
      }
      break;
  }
}

void comm_init(CommNoticeHandler on_notice) {
  s_on_notice = on_notice;
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_outbox_sent(prv_outbox_sent);
  app_message_register_outbox_failed(prv_outbox_failed);
  // Entries carry up to MAX_SETS sets of text, so the inbox needs some room.
  app_message_open(2048, 128);
}

void comm_deinit(void) {
  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
    s_retry_timer = NULL;
  }
  app_message_deregister_callbacks();
}

bool comm_busy(void) {
  return s_awaiting > 0 || s_queue_len > 0;
}

static void prv_enqueue(CommAction action, int entry_index, int set_index, int reps, int rpe10) {
  if (s_queue_len >= QUEUE_SIZE) {
    if (s_on_notice) {
      s_on_notice(MSG_ERROR, "Too many changes waiting for the phone");
    }
    return;
  }
  s_queue[s_queue_len++] = (QueuedAction){
    .action = action,
    .entry_index = entry_index,
    .set_index = set_index,
    .reps = reps,
    .rpe10 = rpe10,
  };
  prv_send_next();
}

void comm_refresh(void) {
  prv_enqueue(ACTION_REFRESH, -1, -1, NO_VALUE, NO_VALUE);
}

void comm_start_workout(void) {
  prv_enqueue(ACTION_START, -1, -1, NO_VALUE, NO_VALUE);
}

void comm_complete_set(int entry_index, int set_index, int reps, int rpe10) {
  prv_enqueue(ACTION_COMPLETE_SET, entry_index, set_index, reps, rpe10);
}

void comm_uncomplete_set(int entry_index, int set_index) {
  prv_enqueue(ACTION_UNCOMPLETE_SET, entry_index, set_index, NO_VALUE, NO_VALUE);
}

void comm_finish_workout(void) {
  prv_enqueue(ACTION_FINISH, -1, -1, NO_VALUE, NO_VALUE);
}

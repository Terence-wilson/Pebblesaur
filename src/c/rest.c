#include "rest.h"

#define PERSIST_REST_END 1
#define PERSIST_REST_TOTAL 2
#define PERSIST_REST_LABEL 3

#define LABEL_LEN 40
// Stop counting up after this long past the end of rest.
#define MAX_OVERDUE_SECONDS (30 * 60)
#define MIN_REST_SECONDS 5

static RestTickHandler s_on_tick;
static time_t s_end;
static int s_total;
static char s_label[LABEL_LEN];
static bool s_alerted;

static void prv_save(void) {
  if (s_end) {
    persist_write_int(PERSIST_REST_END, (int32_t)s_end);
    persist_write_int(PERSIST_REST_TOTAL, s_total);
    persist_write_string(PERSIST_REST_LABEL, s_label);
  } else {
    persist_delete(PERSIST_REST_END);
    persist_delete(PERSIST_REST_TOTAL);
    persist_delete(PERSIST_REST_LABEL);
  }
}

static void prv_alert(void) {
  static const uint32_t segments[] = {300, 150, 300, 150, 600};
  VibePattern pattern = {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  };
  vibes_enqueue_custom_pattern(pattern);
  light_enable_interaction();
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (!s_end) {
    return;
  }
  int remaining = rest_remaining();
  if (remaining <= 0 && !s_alerted) {
    s_alerted = true;
    prv_alert();
  }
  if (remaining < -MAX_OVERDUE_SECONDS) {
    rest_stop();
    return;
  }
  if (s_on_tick) {
    s_on_tick();
  }
}

static void prv_subscribe(bool on) {
  if (on) {
    tick_timer_service_subscribe(SECOND_UNIT, prv_tick);
  } else {
    tick_timer_service_unsubscribe();
  }
}

void rest_init(RestTickHandler on_tick, bool woke_up) {
  s_on_tick = on_tick;
  // While the app is open the tick handler does the alerting.
  wakeup_cancel_all();
  if (persist_exists(PERSIST_REST_END)) {
    s_end = (time_t)persist_read_int(PERSIST_REST_END);
    s_total = persist_read_int(PERSIST_REST_TOTAL);
    persist_read_string(PERSIST_REST_LABEL, s_label, sizeof(s_label));
    int remaining = rest_remaining();
    if (remaining < -MAX_OVERDUE_SECONDS) {
      s_end = 0;
      prv_save();
    } else {
      // Rest that ended long before the app was reopened needs no buzz, but
      // a launch by the rest wakeup does: the wakeup itself is silent.
      s_alerted = remaining <= 0 && !woke_up;
      prv_subscribe(true);
    }
  }
}

void rest_deinit(void) {
  prv_subscribe(false);
  if (s_end && rest_remaining() > 0) {
    // Wakeups need some lead time; very short rests just won't buzz.
    if (wakeup_schedule(s_end, 0, true) < 0) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Could not schedule rest wakeup");
    }
  }
}

void rest_start(int seconds, const char *next_label) {
  if (seconds < MIN_REST_SECONDS) {
    rest_stop();
    return;
  }
  s_end = time(NULL) + seconds;
  s_total = seconds;
  s_alerted = false;
  strncpy(s_label, next_label ? next_label : "", sizeof(s_label) - 1);
  s_label[sizeof(s_label) - 1] = '\0';
  prv_save();
  prv_subscribe(true);
  if (s_on_tick) {
    s_on_tick();
  }
}

void rest_stop(void) {
  bool was_active = s_end != 0;
  s_end = 0;
  s_total = 0;
  s_label[0] = '\0';
  prv_save();
  prv_subscribe(false);
  if (was_active && s_on_tick) {
    s_on_tick();
  }
}

void rest_adjust(int delta_seconds) {
  if (!s_end) {
    return;
  }
  int remaining = rest_remaining();
  if (remaining + delta_seconds <= 0) {
    // Taking time off an expired timer would only push it further overdue.
    delta_seconds = (remaining > 0 ? -remaining : 0);
  }
  s_end += delta_seconds;
  s_total += delta_seconds;
  if (s_total < 1) {
    s_total = 1;
  }
  if (rest_remaining() > 0) {
    s_alerted = false;
  }
  prv_save();
  if (s_on_tick) {
    s_on_tick();
  }
}

bool rest_active(void) {
  return s_end != 0;
}

int rest_remaining(void) {
  return s_end ? (int)(s_end - time(NULL)) : 0;
}

int rest_total(void) {
  return s_total;
}

const char *rest_next_label(void) {
  return s_label;
}

void rest_format(int seconds, char *buf, size_t size) {
  bool overdue = seconds < 0;
  int s = overdue ? -seconds : seconds;
  snprintf(buf, size, "%s%d:%02d", overdue ? "+" : "", s / 60, s % 60);
}

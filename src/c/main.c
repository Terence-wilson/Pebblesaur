#include <pebble.h>
#include "comm.h"
#include "model.h"
#include "rest.h"
#include "ui.h"
#include "windows/message_window.h"
#include "windows/rest_window.h"
#include "windows/workout_window.h"

static void prv_on_notice(CommMessageType type, const char *text) {
  if (type == MSG_FINISHED) {
    vibes_double_pulse();
    message_window_push("Workout saved", text);
  } else {
    vibes_short_pulse();
    message_window_push("Problem", text);
  }
  // A failed action leaves pending sets behind until the phone re-sends.
  ui_refresh_all();
}

static void prv_init(void) {
  bool woke_up = launch_reason() == APP_LAUNCH_WAKEUP;
  model_init(ui_refresh_all);
  rest_init(ui_redraw_all, woke_up);
  comm_init(prv_on_notice);
  workout_window_push();
  if (woke_up && rest_active()) {
    rest_window_push();
  }
}

static void prv_deinit(void) {
  rest_deinit();
  comm_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

#pragma once

#include <pebble.h>

#define OPTIONS_MAX 6

typedef void (*OptionsWindowCallback)(int option_id, void *context);

typedef struct {
  const char *title;
  int num_options;
  const char *labels[OPTIONS_MAX];
  int ids[OPTIONS_MAX];
  OptionsWindowCallback on_select;
  void *context;
} OptionsWindowConfig;

// A short menu of choices. Label strings must be string literals or otherwise
// outlive the window.
void options_window_push(const OptionsWindowConfig *config);

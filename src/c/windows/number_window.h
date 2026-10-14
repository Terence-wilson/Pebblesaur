#pragma once

#include <pebble.h>

typedef enum {
  NUMBER_FORMAT_INT,
  NUMBER_FORMAT_TENTHS, // value is x10, shown as "8.5"
} NumberFormat;

typedef void (*NumberWindowCallback)(int value, void *context);

typedef struct {
  const char *title;
  const char *subtitle;
  int value;
  int min;
  int max;
  int step;
  NumberFormat format;
  NumberWindowCallback on_select;
  void *context;
} NumberWindowConfig;

// Up/Down change the value, Select confirms, Back cancels.
void number_window_push(const NumberWindowConfig *config);

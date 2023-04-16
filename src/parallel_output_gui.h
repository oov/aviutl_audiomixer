#pragma once

#include "ovbase.h"
#include "ovutil/win32.h"

struct parallel_output_gui_options {
  struct wstr filename;
  int bit_format;
  bool export_other;
};

enum {
  parallel_output_progress_complete = -1,
};

struct parallel_output_gui_context {
  struct parallel_output_gui_options options;
  void *userdata;

  void (*progress_func)(struct parallel_output_gui_context const *const ctx, int const progress);
  void (*progress_text_func)(struct parallel_output_gui_context const *const ctx, char const *const text);
};

typedef NODISCARD error (*parallel_output_gui_callback)(struct parallel_output_gui_context *ctx);

NODISCARD error parallel_output_gui_show(struct parallel_output_gui_options const *const options,
                                         parallel_output_gui_callback start_func,
                                         parallel_output_gui_callback finish_func,
                                         void *const userdata);

bool parallel_output_gui_cancel_requested(struct parallel_output_gui_context const *const ctx);

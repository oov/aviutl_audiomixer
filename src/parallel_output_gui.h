#pragma once

#include "ovbase.h"
#include "ovutil/win32.h"

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct parallel_output_gui_options {
  struct wstr filename;
  int bit_format;
  bool export_other;
};

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

enum {
  parallel_output_progress_complete = -1,
};

struct parallel_output_gui_context {
  struct parallel_output_gui_options options;
  void *userdata;

  void (*progress_func)(struct parallel_output_gui_context const *const ctx, int const progress);
  void (*progress_text_func)(struct parallel_output_gui_context const *const ctx, struct wstr const *const text);
};

typedef NODISCARD error (*parallel_output_gui_callback)(struct parallel_output_gui_context *ctx);

NODISCARD error parallel_output_gui_show(struct parallel_output_gui_options const *const options,
                                         parallel_output_gui_callback start_func,
                                         parallel_output_gui_callback finish_func,
                                         void *const userdata);

bool parallel_output_gui_cancel_requested(struct parallel_output_gui_context const *const ctx);

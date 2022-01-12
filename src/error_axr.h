#pragma once

#include "ovbase.h"
#include "ovutil/win32.h"

enum {
  err_type_axr = 100,
};

enum err_axr {
  err_axr_unsupported_aviutl_version = 101,
  err_axr_exedit_not_found = 102,
  err_axr_exedit_not_found_in_same_dir = 103,
  err_axr_unsupported_exedit_version = 106,
  err_axr_project_is_not_open = 107,
  err_axr_project_has_not_yet_been_saved = 108,

  err_axr_wav_size_limit_exceeded = 201,
};

NODISCARD error error_axr_init(void);

void error_message_box(error e, HWND const window, wchar_t const *const msg);

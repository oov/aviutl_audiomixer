#include "parallel_output_gui.h"

#include <commctrl.h>
#include <commdlg.h>
#include <stdatomic.h>

#include "aviutl.h"
#include "ovutil/str.h"

#include "error_axr.h"
#include "i18n.h"
#include "parallel_output.h"

NODISCARD static error show_save_dialog(HWND const owner,
                                        struct wstr const *const title,
                                        struct wstr const *const default_filename,
                                        struct wstr *const dest) {
  enum {
    BUFFER_SIZE = 4096,
  };
  struct wstr tmp = {0};
  struct wstr dir = {0};
  error err = sgrow(&tmp, BUFFER_SIZE);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpy(&tmp, default_filename->ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  OPENFILENAMEW ofn = {
      .lStructSize = sizeof(OPENFILENAMEW),
      .hInstance = get_hinstance(),
      .hwndOwner = owner,
      .lpstrTitle = title->ptr,
      .lpstrFilter = L"Wave File(*.wav)\0*.wav\0",
      .nFilterIndex = 1,
      .lpstrDefExt = L"wav",
      .lpstrFile = tmp.ptr,
      .nMaxFile = BUFFER_SIZE - 1,
      .Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING,
  };
  if (!GetSaveFileNameW(&ofn)) {
    err = errg(err_abort);
    goto cleanup;
  }
  err = scpy(dest, tmp.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  ereport(sfree(&dir));
  ereport(sfree(&tmp));
  return err;
}

struct parallel_output_gui_context_internal {
  struct parallel_output_gui_context pub;

  HWND window;
  atomic_bool cancel_requested;
  bool cancel_after_close;
};

struct parallel_output_dialog {
  struct parallel_output_gui_options const *options;
  struct parallel_output_gui_context_internal *ctxi;
  parallel_output_gui_callback start_func;
  parallel_output_gui_callback finish_func;
  void *userdata;
  error err;
};

static wchar_t const parallel_output_dialog_prop[] = L"parallel_output_dialog";

enum {
  ID_LBL_SAVE_PATH = 1000,
  ID_EDT_SAVE_PATH = 1001,
  ID_BTN_SAVE_PATH = 1002,
  ID_LBL_FORMAT = 1003,
  ID_CMB_FORMAT = 1004,
  ID_CHK_EXPORT_OTHER = 1005,
  ID_PROGRESS = 1006,
  ID_LBL_PROGRESS = 1007,
};

NODISCARD static error get_project_path_fallback(struct wstr *const dest) {
  struct str s = {0};
  struct wstr tmp = {0};
  struct wstr mod = {0};
  error err = get_module_file_name(NULL, &mod);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  size_t fnpos = 0;
  err = extract_file_name(&mod, &fnpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  mod.ptr[fnpos] = L'\0';
  mod.len = fnpos;
  FILE_INFO fi = {0};
  err = aviutl_get_editing_file_info(&fi);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = from_mbcs(&str_unmanaged_const(fi.name), &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  size_t extpos = 0;
  err = extract_file_extension(&tmp, &extpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  tmp.ptr[extpos] = L'\0';
  tmp.len = extpos;
  err = scatm(&mod, tmp.ptr, L".wav");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpy(dest, mod.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  ereport(sfree(&mod));
  ereport(sfree(&tmp));
  ereport(sfree(&s));
  return err;
}

NODISCARD static error get_default_filename(struct wstr *const dest) {
  if (!dest) {
    return errg(err_invalid_arugment);
  }
  struct wstr ws = {0};
  error err = aviutl_get_project_path(&ws);
  if (efailed(err)) {
    if (eis(err, err_type_axr, err_axr_project_has_not_yet_been_saved)) {
      efree(&err);
      err = get_project_path_fallback(&ws);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
    } else {
      err = ethru(err);
      goto cleanup;
    }
  }
  size_t extpos = 0;
  err = extract_file_extension(&ws, &extpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ws.ptr[extpos] = L'\0';
  ws.len = extpos;
  err = scat(&ws, L".wav");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpy(dest, ws.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  ereport(sfree(&ws));
  return err;
}

static void enable_controls(HWND const window, WINBOOL b) {
  EnableWindow(GetDlgItem(window, ID_LBL_SAVE_PATH), b);
  EnableWindow(GetDlgItem(window, ID_EDT_SAVE_PATH), b);
  EnableWindow(GetDlgItem(window, ID_BTN_SAVE_PATH), b);
  EnableWindow(GetDlgItem(window, ID_LBL_FORMAT), b);
  EnableWindow(GetDlgItem(window, ID_CMB_FORMAT), b);
  EnableWindow(GetDlgItem(window, ID_CHK_EXPORT_OTHER), b);
  EnableWindow(GetDlgItem(window, ID_PROGRESS), b);
  EnableWindow(GetDlgItem(window, IDOK), b);
  EnableWindow(GetDlgItem(window, IDABORT), !b);
}

static void send_message_i18n(HWND const window, UINT msg, char const *const text) {
  struct wstr ws = {0};
  error err = to_wstr(&str_unmanaged_const(text), &ws);
  if (efailed(err)) {
    ereport(err);
    return;
  }
  SendMessageW(window, msg, 0, (LPARAM)ws.ptr);
  ereport(sfree(&ws));
}

static BOOL wndproc_init_dialog(HWND const window, struct parallel_output_dialog *const dlg) {
  struct wstr ws = {0};
  error err = eok();
  if (!dlg) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  if (!SetPropW(window, parallel_output_dialog_prop, (HANDLE)dlg)) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  SYS_INFO si = {0};
  err = aviutl_get_sys_info(&si);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  SendMessageW(GetDlgItem(window, IDOK), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, IDABORT), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_LBL_SAVE_PATH), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_EDT_SAVE_PATH), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_BTN_SAVE_PATH), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_LBL_FORMAT), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_CMB_FORMAT), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_CHK_EXPORT_OTHER), WM_SETFONT, (WPARAM)si.hfont, 0);
  SendMessageW(GetDlgItem(window, ID_LBL_PROGRESS), WM_SETFONT, (WPARAM)si.hfont, 0);

  send_message_i18n(window, WM_SETTEXT, gettext("ParallelOutput - ChannelStrip"));
  send_message_i18n(GetDlgItem(window, IDOK), WM_SETTEXT, gettext("Export"));
  send_message_i18n(GetDlgItem(window, IDABORT), WM_SETTEXT, gettext("Abort"));
  send_message_i18n(GetDlgItem(window, ID_LBL_SAVE_PATH), WM_SETTEXT, gettext("Destination"));
  send_message_i18n(GetDlgItem(window, ID_LBL_FORMAT), WM_SETTEXT, gettext("Bit Depth"));
  send_message_i18n(
      GetDlgItem(window, ID_CHK_EXPORT_OTHER), WM_SETTEXT, gettext("Save audio not managed by ChannelStrip"));

  err = scpy(&ws, dlg->options->filename.ptr ? dlg->options->filename.ptr : L"");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!ws.len) {
    err = get_default_filename(&ws);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  SetWindowTextW(GetDlgItem(window, ID_EDT_SAVE_PATH), ws.ptr);

  {
    HWND const h = GetDlgItem(window, ID_CMB_FORMAT);
    send_message_i18n(h, CB_ADDSTRING, gettext("16Bit"));
    send_message_i18n(h, CB_ADDSTRING, gettext("32Bit Float (Recommended)"));
    WPARAM idx = 1;
    switch (dlg->options->bit_format) {
    case parallel_output_bit_format_int16:
      idx = 0;
      break;
    case parallel_output_bit_format_float32:
      idx = 1;
      break;
    }
    SendMessageW(h, CB_SETCURSEL, idx, 0);
  }
  {
    HWND const h = GetDlgItem(window, ID_CHK_EXPORT_OTHER);
    SendMessageW(h, BM_SETCHECK, dlg->options->export_other ? BST_CHECKED : BST_UNCHECKED, 0);
  }
  {
    HWND const h = GetDlgItem(window, ID_PROGRESS);
    SendMessage(h, PBM_SETRANGE, 0, (LPARAM)MAKELONG(0, 100));
    SendMessage(h, PBM_SETPOS, 0, 0);
  }

  enable_controls(window, TRUE);
cleanup:
  ereport(sfree(&ws));
  if (efailed(err)) {
    dlg->err = err;
    err = NULL;
    EndDialog(window, 0);
  }
  return TRUE;
}

enum {
  WM_PROGRESS = WM_APP + 1000,
};

static void send_progress(struct parallel_output_gui_context const *const ctx, int const percent) {
  struct parallel_output_gui_context_internal const *const ctxi = (void const *)ctx;
  PostMessageW(ctxi->window, WM_PROGRESS, (WPARAM)percent, 0);
}

static void send_progress_text(struct parallel_output_gui_context const *const ctx, char const *const text) {
  struct parallel_output_gui_context_internal const *const ctxi = (void const *)ctx;
  struct wstr ws = {0};
  error err = to_wstr(&str_unmanaged_const(text), &ws);
  if (efailed(err)) {
    ereport(err);
  } else {
    SendMessageW(GetDlgItem(ctxi->window, ID_LBL_PROGRESS), WM_SETTEXT, 0, (LPARAM)ws.ptr);
    ereport(sfree(&ws));
  }
}

static BOOL start_export(HWND const window) {
  error err = eok();
  struct parallel_output_dialog *const dlg = (void *)GetPropW(window, parallel_output_dialog_prop);
  if (!dlg || dlg->ctxi) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  err = mem(&dlg->ctxi, 1, sizeof(struct parallel_output_gui_context_internal));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  *dlg->ctxi = (struct parallel_output_gui_context_internal){
      .pub =
          {
              .userdata = dlg->userdata,
              .progress_func = send_progress,
              .progress_text_func = send_progress_text,
          },
      .window = window,
  };
  err = get_window_text(GetDlgItem(window, ID_EDT_SAVE_PATH), &dlg->ctxi->pub.options.filename);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  switch (SendMessageW(GetDlgItem(window, ID_CMB_FORMAT), CB_GETCURSEL, 0, 0)) {
  case 0:
    dlg->ctxi->pub.options.bit_format = parallel_output_bit_format_int16;
    break;
  case 1:
    dlg->ctxi->pub.options.bit_format = parallel_output_bit_format_float32;
    break;
  }
  dlg->ctxi->pub.options.export_other =
      SendMessageW(GetDlgItem(window, ID_CHK_EXPORT_OTHER), BM_GETCHECK, 0, 0) == BST_CHECKED;

  enable_controls(window, FALSE);
  err = dlg->start_func((void *)dlg->ctxi);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  if (efailed(err)) {
    enable_controls(window, TRUE);
    if (dlg->ctxi) {
      ereport(sfree(&dlg->ctxi->pub.options.filename));
      ereport(mem_free(&dlg->ctxi));
    }
    axr_error_message_box(err, window, gettext("Error"), NULL, "%1$s", gettext("Export could not be started."));
  }
  return TRUE;
}

static BOOL abort_export(HWND const window) {
  error err = eok();
  struct parallel_output_dialog *const dlg = (void *)GetPropW(window, parallel_output_dialog_prop);
  if (!dlg || !dlg->ctxi) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  atomic_store_explicit(&dlg->ctxi->cancel_requested, true, memory_order_relaxed);
cleanup:
  if (efailed(err)) {
    ereportmsg_i18n(err, gettext("Failed to abort export."));
  }
  return TRUE;
}

static BOOL finish_export(HWND const window) {
  error err = eok();
  struct parallel_output_dialog *const dlg = (void *)GetPropW(window, parallel_output_dialog_prop);
  if (!dlg || !dlg->ctxi) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  err = dlg->finish_func((void *)dlg->ctxi);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  bool const cancelled = parallel_output_gui_cancel_requested(&dlg->ctxi->pub);
  if (!cancelled) {
    EndDialog(window, IDOK);
  } else if (cancelled && dlg->ctxi->cancel_after_close) {
    EndDialog(window, IDCANCEL);
  }
cleanup:
  ereport(sfree(&dlg->ctxi->pub.options.filename));
  ereport(mem_free(&dlg->ctxi));
  if (efailed(err)) {
    axr_error_message_box(err, window, gettext("Error"), NULL, "%1$s", gettext("Export could not be completed."));
  }
  enable_controls(window, TRUE);
  SendMessageW(GetDlgItem(window, ID_PROGRESS), PBM_SETPOS, 0, 0);
  return TRUE;
}

static BOOL closing(HWND const window) {
  struct parallel_output_dialog *const dlg = (void *)GetPropW(window, parallel_output_dialog_prop);
  if (!dlg || !dlg->ctxi) {
    EndDialog(window, IDCANCEL);
    return TRUE;
  }
  struct wstr title = {0};
  struct wstr msg = {0};
  error err = to_wstr(&str_unmanaged_const(gettext("Confirm")), &title);
  if (efailed(err)) {
    ereport(err);
  }
  err = to_wstr(&str_unmanaged_const(gettext("Export is not yet complete.\nDo you really want to abort?")), &msg);
  if (efailed(err)) {
    ereport(err);
  }
  int const r = message_box(window,
                            msg.ptr ? msg.ptr : L"Export is not yet complete.\nDo you really want to abort?",
                            title.ptr ? title.ptr : L"Confirm",
                            MB_ICONQUESTION | MB_OKCANCEL);
  ereport(sfree(&msg));
  ereport(sfree(&title));
  if (r == IDCANCEL) {
    return TRUE;
  }
  // The export process is running in the background,
  // so it is possible that will be completed before return from the dialog.
  if (dlg->ctxi) {
    atomic_store_explicit(&dlg->ctxi->cancel_requested, true, memory_order_relaxed);
    dlg->ctxi->cancel_after_close = true;
  }
  return TRUE;
}

NODISCARD static BOOL select_parallel_output_file(HWND const window) {
  struct wstr title = {0};
  struct wstr tmp = {0};
  error err = eok();
  struct parallel_output_dialog *const dlg = (void *)GetPropW(window, parallel_output_dialog_prop);
  if (!dlg || dlg->ctxi) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  err = get_window_text(GetDlgItem(window, ID_EDT_SAVE_PATH), &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  HWND *disabled_windows = NULL;
  err = disable_family_windows(window, &disabled_windows);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = to_wstr(&str_unmanaged_const(gettext("Choose where to save file")), &title);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = show_save_dialog(window, &title, &tmp, &tmp);
  restore_disabled_family_windows(disabled_windows);
  if (efailed(err)) {
    if (eis(err, err_type_generic, err_abort)) {
      efree(&err);
      goto cleanup;
    }
    err = ethru(err);
    goto cleanup;
  }
  SetWindowTextW(GetDlgItem(window, ID_EDT_SAVE_PATH), tmp.ptr);
cleanup:
  ereport(sfree(&tmp));
  ereport(sfree(&title));
  if (efailed(err)) {
    axr_error_message_box(
        err, window, gettext("Error"), NULL, "%1$s", gettext("Failed to display file selection dialog."));
  }
  return TRUE;
}

static INT_PTR CALLBACK wndproc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) {
  switch (message) {
  case WM_INITDIALOG:
    return wndproc_init_dialog(window, (void *)lparam);
  case WM_DESTROY:
    RemovePropW(window, parallel_output_dialog_prop);
    return 0;
  case WM_PROGRESS:
    if ((int)wparam == parallel_output_progress_complete) {
      finish_export(window);
    } else {
      SendMessageW(GetDlgItem(window, ID_PROGRESS), PBM_SETPOS, wparam, 0);
    }
    return TRUE;
  case WM_COMMAND:
    switch (LOWORD(wparam)) {
    case IDOK:
      return start_export(window);
    case IDCANCEL:
      return closing(window);
    case IDABORT:
      return abort_export(window);
    case ID_BTN_SAVE_PATH:
      return select_parallel_output_file(window);
    }
    break;
  }
  return FALSE;
}

NODISCARD error parallel_output_gui_show(struct parallel_output_gui_options const *const options,
                                         parallel_output_gui_callback start_func,
                                         parallel_output_gui_callback finish_func,
                                         void *const userdata) {
  struct parallel_output_dialog dlg = {
      .options = options,
      .start_func = start_func,
      .finish_func = finish_func,
      .userdata = userdata,
  };
  HWND parent_window = NULL;
  HWND *disabled_windows = NULL;
  error err = aviutl_get_exedit_window(&parent_window);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = disable_family_windows(parent_window, &disabled_windows);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  INT_PTR r = DialogBoxParamW(get_hinstance(), L"PARALLEL_OUTPUT_DIALOG", parent_window, wndproc, (LPARAM)&dlg);
  if (r == 0 || r == -1) {
    if (efailed(dlg.err)) {
      err = ethru(dlg.err);
      dlg.err = NULL;
    } else {
      err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    }
    goto cleanup;
  }
  if (r == IDCANCEL) {
    goto cleanup;
  }
cleanup:
  if (disabled_windows) {
    restore_disabled_family_windows(disabled_windows);
    disabled_windows = NULL;
  }
  return err;
}

bool parallel_output_gui_cancel_requested(struct parallel_output_gui_context const *const ctx) {
  struct parallel_output_gui_context_internal const *const ctxi = (void const *)ctx;
  return atomic_load_explicit(&ctxi->cancel_requested, memory_order_relaxed);
}

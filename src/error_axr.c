#include "error_axr.h"

#include "i18n.h"
#include "version.h"

static NODISCARD error get_generic_message(int const type, int const code, struct NATIVE_STR *const dest) {
  if (!dest) {
    return errg(err_invalid_arugment);
  }
  if (type != err_type_generic) {
    dest->len = 0;
    return eok();
  }
  switch (code) {
  case err_fail:
    return to_wstr(&str_unmanaged_const(gettext("Failed.")), dest);
  case err_unexpected:
    return to_wstr(&str_unmanaged_const(gettext("Unexpected.")), dest);
  case err_invalid_arugment:
    return to_wstr(&str_unmanaged_const(gettext("Invalid argument.")), dest);
  case err_null_pointer:
    return to_wstr(&str_unmanaged_const(gettext("NULL pointer.")), dest);
  case err_out_of_memory:
    return to_wstr(&str_unmanaged_const(gettext("Out of memory.")), dest);
  case err_not_sufficient_buffer:
    return to_wstr(&str_unmanaged_const(gettext("Not sufficient buffer.")), dest);
  case err_not_found:
    return to_wstr(&str_unmanaged_const(gettext("Not found.")), dest);
  case err_abort:
    return to_wstr(&str_unmanaged_const(gettext("Aborted.")), dest);
  case err_not_implemented_yet:
    return to_wstr(&str_unmanaged_const(gettext("Not implemented yet.")), dest);
  }
  return to_wstr(&str_unmanaged_const(gettext("Unknown error code.")), dest);
}

static NODISCARD error get_axr_message(int const type, int const code, struct NATIVE_STR *const dest) {
  if (!dest) {
    return errg(err_invalid_arugment);
  }
  if (type != err_type_axr) {
    dest->len = 0;
    return eok();
  }
  switch (code) {
  case err_axr_unsupported_aviutl_version:
    return to_wstr(&str_unmanaged_const(gettext("The currently running version of AviUtl is not supported.")), dest);
  case err_axr_exedit_not_found:
    return to_wstr(&str_unmanaged_const(gettext("Advanced Editing plug-in exedit.auf cannot be found.")), dest);
  case err_axr_exedit_not_found_in_same_dir:
    return to_wstr(&str_unmanaged_const(gettext(
                       "Advanced Editing plug-in exedit.auf cannot be found in the same folder as GCMZDrops.auf.")),
                   dest);
  case err_axr_unsupported_exedit_version:
    return to_wstr(&str_unmanaged_const(gettext("The currently using version of exedit.auf is not supported.")), dest);
  case err_axr_project_is_not_open:
    return to_wstr(&str_unmanaged_const(gettext("AviUtl project file (*.aup) has not yet been opened.")), dest);
  case err_axr_project_has_not_yet_been_saved:
    return to_wstr(&str_unmanaged_const(gettext("AviUtl project file (*.aup) has not yet been saved.")), dest);
  case err_axr_wav_size_limit_exceeded:
    return to_wstr(&str_unmanaged_const(gettext("Processing cannot continue because the wave file size is too large.")),
                   dest);
  }
  return to_wstr(&str_unmanaged_const(gettext("Unknown error code.")), dest);
}

NODISCARD error axr_error_message(int const type, int const code, struct NATIVE_STR *const dest) {
  if (type == err_type_generic) {
    return get_generic_message(type, code, dest);
  }
  if (type == err_type_hresult) {
    return error_win32_message_mapper(type, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), dest);
  }
  if (type == err_type_axr) {
    return get_axr_message(type, code, dest);
  }
  if (type == err_type_errno) {
    return error_errno_message_mapper(type, code, dest);
  }
  return to_wstr(&str_unmanaged_const(gettext("Unknown error code.")), dest);
}

NODISCARD error axr_error_vformat(
    error e, struct wstr *const dest, wchar_t const *const reference, char const *const format, va_list valist) {
  if (!dest || !format) {
    return errg(err_invalid_arugment);
  }
  struct wstr mainmsg = {0};
  struct wstr errmsg = {0};
  error err = eok();
  if (esucceeded(e)) {
    err = mo_vsprintf_wstr(dest, reference, format, valist);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    goto cleanup;
  }
  err = mo_vsprintf_wstr(&mainmsg, reference, format, valist);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = error_to_string(e, &errmsg);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scatm(dest, mainmsg.ptr, NSTR("\n\n"), errmsg.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  ereport(sfree(&mainmsg));
  ereport(sfree(&errmsg));
  return err;
}

NODISCARD error
axr_error_format(error e, struct wstr *const dest, wchar_t const *const reference, char const *const format, ...) {
  va_list valist;
  va_start(valist, format);
  error err = axr_error_vformat(e, dest, reference, format, valist);
  if (efailed(err)) {
    err = ethru(err);
  }
  va_end(valist);
  return err;
}

void axr_error_message_box(error e,
                           HWND const window,
                           char const *const title,
                           wchar_t const *const reference,
                           char const *const format,
                           ...) {
  struct wstr wide_title = {0};
  struct wstr msg = {0};
  va_list valist;
  va_start(valist, format);
  error err = axr_error_vformat(e, &msg, reference, format, valist);
  va_end(valist);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = ssprintf(&wide_title, NULL, L"%1$s - %2$s %3$s", title, "ChannelStrip", VERSION);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  message_box(window, msg.ptr, wide_title.ptr, MB_ICONERROR);
cleanup:
  ereport(sfree(&wide_title));
  ereport(sfree(&msg));
  if (efailed(err)) {
    ereportmsg_i18n(err, gettext("Failed to display error dialog."));
  }
  ereport(e);
}

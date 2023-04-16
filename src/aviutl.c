#include "aviutl.h"

#include "ovutil/str.h"
#include "ovutil/win32.h"

#include <string.h>

#include "error_axr.h"
#include "i18n.h"

enum aviutl_patched {
  aviutl_patched_default = 0,
  aviutl_patched_en = 1,
  aviutl_patched_zh_cn = 2,
};

static FILTER const *g_fp = NULL;
static void *g_editp = NULL;

static FILTER *g_exedit_fp = NULL;
static FILTER *g_exedit_audio_fp = NULL;
static enum aviutl_patched g_exedit_patch = aviutl_patched_default;

NODISCARD static error verify_installation(void) {
  struct wstr path = {0};
  error err = get_module_file_name(get_hinstance(), &path);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  size_t fnpos = 0;
  err = extract_file_name(&path, &fnpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  path.ptr[fnpos] = L'\0';
  path.len = fnpos;

  err = scat(&path, L"exedit.auf");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  bool found = false;
  err = file_exists(&path, &found);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!found) {
    err = err(err_type_axr, err_axr_exedit_not_found_in_same_dir);
    goto cleanup;
  }

cleanup:
  ereport(sfree(&path));
  return err;
}

NODISCARD static error find_exedit_filter(FILTER **const exedit_fp, enum aviutl_patched *const patched) {
  static TCHAR const exedit_name_mbcs[] = "\x8a\x67\x92\xa3\x95\xd2\x8f\x57";              // "拡張編集"
  static TCHAR const zhcn_patched_exedit_name_mbcs[] = "\xc0\xa9\xd5\xb9\xb1\xe0\xbc\xad"; // "扩展编辑"
  static TCHAR const en_patched_exedit_name_mbcs[] = "Advanced Editing";

  *exedit_fp = NULL;
  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  for (int i = 0; i < si.filter_n; ++i) {
    FILTER *p = g_fp->exfunc->get_filterp(i);
    if (!p || (p->flag & FILTER_FLAG_AUDIO_FILTER) == FILTER_FLAG_AUDIO_FILTER) {
      continue;
    }
    if (strcmp(p->name, exedit_name_mbcs) == 0) {
      *exedit_fp = p;
      *patched = aviutl_patched_default;
      return eok();
    } else if (strcmp(p->name, zhcn_patched_exedit_name_mbcs) == 0) {
      *exedit_fp = p;
      *patched = aviutl_patched_zh_cn;
      return eok();
    } else if (strcmp(p->name, en_patched_exedit_name_mbcs) == 0) {
      *exedit_fp = p;
      *patched = aviutl_patched_en;
      return eok();
    }
  }
  *exedit_fp = NULL;
  *patched = aviutl_patched_default;
  return err(err_type_axr, err_axr_exedit_not_found);
}

NODISCARD static error find_exedit_audio_filter(FILTER **const exedit_audio_fp) {
  *exedit_audio_fp = NULL;
  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  for (int i = 0; i < si.filter_n; ++i) {
    FILTER *p = g_fp->exfunc->get_filterp(i);
    if (!p || (p->flag & FILTER_FLAG_AUDIO_FILTER) != FILTER_FLAG_AUDIO_FILTER) {
      continue;
    }
    if (p->dll_hinst == g_exedit_fp->dll_hinst) {
      *exedit_audio_fp = p;
      return eok();
    }
  }
  *exedit_audio_fp = NULL;
  return err(err_type_axr, err_axr_exedit_not_found);
}

NODISCARD static error verify_aviutl_version(void) {
  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  if (si.build < 10000) {
    return err(err_type_axr, err_axr_unsupported_aviutl_version);
  }
  return eok();
}

static size_t atou32(TCHAR const *s, uint32_t *const ret) {
  uint64_t r = 0;
  size_t i = 0;
  while (s[i]) {
    if (i >= 10 || '0' > s[i] || s[i] > '9') {
      break;
    }
    r = r * 10 + (uint64_t)(s[i++] - '0');
  }
  if (i == 0 || r > 0xffffffff) {
    return 0;
  }
  *ret = r & 0xffffffff;
  return i;
}

NODISCARD static error verify_exedit_version(FILTER const *const exedit_fp) {
  static TCHAR const version_token[] = " version ";
  TCHAR const *verstr = strstr(exedit_fp->information, version_token);
  if (!verstr) {
    goto failed;
  }
  verstr += strlen(version_token);
  uint32_t major = 0, minor = 0;
  size_t len = atou32(verstr, &major);
  if (!len) {
    goto failed;
  }
  verstr += len + 1; // skip dot
  len = atou32(verstr, &minor);
  if (!len) {
    goto failed;
  }
  if (major == 0 && minor < 92) {
    goto failed;
  }
  return eok();

failed:
  return err(err_type_axr, err_axr_unsupported_exedit_version);
}

void aviutl_set_pointers(FILTER const *fp, void *editp) {
  g_fp = fp;
  g_editp = editp;
}

error aviutl_init(void) {
  FILTER *exedit_fp = NULL;
  enum aviutl_patched patched = aviutl_patched_default;
  error err = eok();

  err = verify_installation();
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = verify_aviutl_version();
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = find_exedit_filter(&exedit_fp, &patched);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = verify_exedit_version(exedit_fp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  g_exedit_fp = exedit_fp;
  g_exedit_patch = patched;
  err = find_exedit_audio_filter(&g_exedit_audio_fp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  return err;
}

bool aviutl_initalized(void) { return g_exedit_fp; }

error aviutl_exit(void) { return eok(); }

error aviutl_exedit_is_enpatched(bool *const enpatched) {
  if (!enpatched) {
    return errg(err_null_pointer);
  }
  if (!aviutl_initalized()) {
    return errg(err_unexpected);
  }
  *enpatched = g_exedit_patch == aviutl_patched_en;
  return eok();
}

error aviutl_get_exedit_window(HWND *const h) {
  if (!h) {
    return errg(err_null_pointer);
  }
  if (!g_exedit_fp) {
    return errg(err_unexpected);
  }
  *h = g_exedit_fp->hwnd;
  return eok();
}

HWND aviutl_get_exedit_window_must(void) {
  HWND h = NULL;
  error err = aviutl_get_exedit_window(&h);
  if (efailed(err)) {
    ereportmsg_i18n(err, gettext("Failed to obtain the window handle for Advanced Editing."));
    h = GetDesktopWindow();
  }
  return h;
}

error aviutl_get_my_window(HWND *const h) {
  if (!h) {
    return errg(err_null_pointer);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  *h = g_fp->hwnd;
  return eok();
}

HWND aviutl_get_my_window_must(void) {
  HWND h = NULL;
  error err = aviutl_get_my_window(&h);
  if (efailed(err)) {
    ereportmsg_i18n(err, gettext("Failed to obtain my filter window handle."));
    h = GetDesktopWindow();
  }
  return h;
}

FILTER *aviutl_get_exedit_audio_filter(void) { return g_exedit_audio_fp; }

error aviutl_get_sys_info(SYS_INFO *const si) {
  if (!si) {
    return errg(err_null_pointer);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_sys_info(g_editp, si)) {
    return errg(err_fail);
  }
  return eok();
}

error aviutl_get_editing_file_info(FILE_INFO *const fi) {
  if (!fi) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_file_info(g_editp, fi)) {
    return errg(err_fail);
  }
  if (fi->audio_rate == 0 || fi->audio_ch == 0) {
    return err(err_type_axr, err_axr_project_is_not_open);
  }
  return eok();
}

error aviutl_get_file_info(struct wstr const *const path, FILE_INFO *const fi, int *const samples) {
  if (!path) {
    return errg(err_invalid_arugment);
  }
  if (!fi || !samples) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }

  FILE_INFO current = {0};
  error err = aviutl_get_editing_file_info(&current);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  struct str s = {0}; // TODO: use TCHAR
  err = to_mbcs(path, &s);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  AVI_FILE_HANDLE afh = g_fp->exfunc->avi_file_open(s.ptr, fi, 0);
  if (!afh) {
    err = errg(err_fail);
    goto cleanup;
  }
  *samples = g_fp->exfunc->avi_file_set_audio_sample_rate(afh, current.audio_rate, current.audio_ch);
  g_fp->exfunc->avi_file_close(afh);

cleanup:
  ereport(sfree(&s));
  return err;
}

error aviutl_get_project_path(struct wstr *const dest) {
  if (!dest) {
    return errg(err_null_pointer);
  }

  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }

  FILE_INFO fi = {0};
  err = aviutl_get_editing_file_info(&fi);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }

  if (si.project_name == NULL || si.project_name[0] == '\0') {
    return err(err_type_axr, err_axr_project_has_not_yet_been_saved);
  }

  err = from_mbcs(&str_unmanaged(si.project_name), dest);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

error aviutl_get_frame(int *const f) {
  if (!f) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  *f = g_fp->exfunc->get_frame(g_editp);
  return eok();
}

error aviutl_set_frame(int *const f) {
  if (!f) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  *f = g_fp->exfunc->set_frame(g_editp, *f);
  return eok();
}

error aviutl_get_frame_n(int *const n) {
  if (!n) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  *n = g_fp->exfunc->get_frame_n(g_editp);
  return eok();
}

error aviutl_set_frame_n(int *const n) {
  if (!n) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  *n = g_fp->exfunc->set_frame_n(g_editp, *n);
  return eok();
}

error aviutl_get_select_frame(int *const start, int *const end) {
  if (!start || !end) {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_select_frame(g_editp, start, end)) {
    *start = -1;
    *end = -1;
  }
  return eok();
}

error aviutl_set_select_frame(int const start, int const end) {
  if (!g_editp || !g_fp) {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->set_select_frame(g_editp, start, end)) {
    return errg(err_fail);
  }
  return eok();
}

NODISCARD error aviutl_add_menu_item(
    struct wstr const *const name, HWND const window, int const id, int const def_key, int const flag) {
  if (!name || !window) {
    return errg(err_invalid_arugment);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  struct str tmp = {0};
  error err = to_mbcs(name, &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!g_fp->exfunc->add_menu_item(ov_deconster_(g_fp), tmp.ptr, window, id, def_key, flag)) {
    err = errg(err_fail);
    goto cleanup;
  }

cleanup:
  ereport(sfree(&tmp));
  return err;
}

error aviutl_ini_load_int(struct str const *const key, int const defvalue, int *const dest) {
  if (!key) {
    return errg(err_invalid_arugment);
  }
  if (!dest) {
    return errg(err_null_pointer);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  *dest = g_fp->exfunc->ini_load_int(ov_deconster_(g_fp), key->ptr, defvalue);
  return eok();
}

error aviutl_ini_load_str(struct str const *const key, struct str const *const defvalue, struct str *const dest) {
  if (!key || !defvalue) {
    return errg(err_invalid_arugment);
  }
  if (!dest) {
    return errg(err_null_pointer);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  struct str tmp = {0};
  error err = sgrow(&tmp, 1024);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!g_fp->exfunc->ini_load_str(ov_deconster_(g_fp), key->ptr, tmp.ptr, defvalue->ptr)) {
    err = errg(err_fail);
    goto cleanup;
  }
  err = scpy(dest, tmp.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  ereport(sfree(&tmp));
  return err;
}

error aviutl_ini_save_int(struct str const *const key, int const value) {
  if (!key) {
    return errg(err_invalid_arugment);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  g_fp->exfunc->ini_save_int(ov_deconster_(g_fp), key->ptr, value);
  return eok();
}

error aviutl_ini_save_str(struct str const *const key, struct str const *const value) {
  if (!key || !value) {
    return errg(err_invalid_arugment);
  }
  if (!g_fp) {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->ini_save_str(ov_deconster_(g_fp), key->ptr, value->ptr)) {
    return errg(err_fail);
  }
  return eok();
}

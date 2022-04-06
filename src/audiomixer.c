#include "ovbase.h"

#include <math.h>

#include "ovnum.h"
#include "ovthreads.h"
#include "ovutil/str.h"
#include "ovutil/win32.h"

#include <commdlg.h>

#include "aviutl.h"
#include "dynamics.h"
#include "error_axr.h"
#include "inlines.h"
#include "mixer.h"
#include "parallel_output.h"
#include "parallel_output_gui.h"
#include "version.h"

static struct mixer *g_mixer = NULL;

static int g_last_frame = -1;

static int16_t *g_warming_buffer = NULL;

static HFONT g_font = NULL;
static HWND g_id_combo = NULL;
static HWND g_params_label = NULL;
static UINT_PTR g_view_update_timer_id = 0;
static int g_selected_id = 0;
static bool g_need_update_view = false;
static bool g_saving = false;

static BOOL jumped(FILTER *fp, FILTER_PROC_INFO *fpip) {
  mixer_reset(g_mixer);

  // load recent frames to avoid audio glitch
  int const warming_up_frames =
      (int)(mixer_get_warming_up_duration(g_mixer) * mixer_get_sample_rate(g_mixer)) / fpip->audio_n + 1;
  mixer_set_warming(g_mixer, true);
  for (int i = maxi(0, fpip->frame - warming_up_frames); i < fpip->frame; ++i) {
    int const written = fp->exfunc->get_audio_filtering(fp, fpip->editp, i, g_warming_buffer);
    mixer_mix(g_mixer, g_warming_buffer, (size_t)written);
  }
  mixer_set_warming(g_mixer, false);

  // overwrite current buffer
  g_last_frame = fpip->frame - 1;
  int const written = fp->exfunc->get_audio_filtering(fp, fpip->editp, fpip->frame, g_warming_buffer);
  memcpy(fpip->audiop, g_warming_buffer, (size_t)(written * fpip->audio_ch) * sizeof(int16_t));
  mixer_mix(g_mixer, fpip->audiop, (size_t)written);
  return TRUE;
}

static BOOL filter_proc_master(FILTER *fp, FILTER_PROC_INFO *fpip) {
  aviutl_set_pointers(fp, fpip->editp);

  if (!mixer_get_warming(g_mixer) && (g_last_frame + 1 != fpip->frame)) {
    return jumped(fp, fpip);
  }

  mixer_mix(g_mixer, fpip->audiop, (size_t)fpip->audio_n);
  g_last_frame = fpip->frame;
  return TRUE;
}

static inline float slider_to_db(int v) {
  float f = (float)(v) * (1.f / 10000.f);
  if (f < 0) {
    f += 1.f;
    if (f < 0.00001f) {
      return -144.f;
    }
    return 20.f * log10f(f);
  }
  return f * 24.f;
}

static BOOL filter_proc_channel_strip(FILTER *fp, FILTER_PROC_INFO *fpip) {
  aviutl_set_pointers(fp, fpip->editp);
  int const id = fp->track[0];
  if (id == -1) {
    return TRUE;
  }
  bool updated = false;
  static float const div1000 = 1.f / 1000.f;
  static float const div10000 = 1.f / 10000.f;
  error err = mixer_update_channel(g_mixer,
                                   id,
                                   &(struct channel_effect_params){
                                       .pre_gain = slider_to_db(fp->track[1]),
                                       .lagger_duration = (float)(fp->track[2]) * div1000,
                                       .low_shelf_frequency = (float)fp->track[3],
                                       .low_shelf_gain = slider_to_db(fp->track[4]),
                                       .high_shelf_frequency = (float)fp->track[5],
                                       .high_shelf_gain = slider_to_db(fp->track[6]),
                                       .dynamics_threshold = (float)(fp->track[7]) * div10000,
                                       .dynamics_ratio = (float)(fp->track[8]) * div10000 * 0.4f + 0.2f,
                                       .dynamics_attack = (float)(fp->track[9]) * div10000,
                                       .dynamics_release = (float)(fp->track[10]) * div10000 * 0.82f,
                                       .aux_send_id = fp->track[11],
                                       .aux_send = slider_to_db(fp->track[12]),
                                       .post_gain = slider_to_db(fp->track[13]),
                                       .pan = (float)(fp->track[14]) * div10000,
                                   },
                                   (int16_t const *restrict const)fpip->audiop,
                                   (size_t)fpip->audio_n,
                                   &updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if ((g_selected_id == id) && updated) {
    g_need_update_view = true;
  }

cleanup:
  memset(fpip->audiop, 0, (size_t)(fpip->audio_n * fpip->audio_ch) * sizeof(int16_t));
  ereport(err);
  return TRUE;
}

NODISCARD static error set_current_format(void) {
  FILE_INFO fi = {0};
  error err = aviutl_get_editing_file_info(&fi);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  float const sample_rate = (float)fi.audio_rate;
  size_t const channels = (size_t)fi.audio_ch;
  if (fcmp(sample_rate, ==, mixer_get_sample_rate(g_mixer), 1e-6f) && channels == mixer_get_channels(g_mixer)) {
    goto cleanup;
  }
  // It seems that AviUtl sometimes writes to a buffer of two or more frames instead of one.
  // If the buffer size is reserved just below the required buffer size, it will result in buffer overrun.
  // To avoid this problem, reserve a larger buffer size.
  size_t const samples_per_frame = (size_t)((fi.audio_rate * fi.video_scale * 5) / (fi.video_rate * 2)) + 32;

  if (g_warming_buffer) {
    ereport(mem_aligned_free(&g_warming_buffer));
  }
  err = mem_aligned_alloc(&g_warming_buffer, samples_per_frame * channels, sizeof(int16_t), 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = mixer_set_format(g_mixer, sample_rate, channels, samples_per_frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  g_last_frame = -1;
  g_need_update_view = true;

cleanup:
  return err;
}

static BOOL filter_proc_aux1(FILTER *fp, FILTER_PROC_INFO *fpip) {
  aviutl_set_pointers(fp, fpip->editp);
  if ((size_t)fpip->audio_ch != mixer_get_channels(g_mixer)) {
    // The number of channels, etc., may be changed when using "音声読み込み",
    // but no notification event is generated when this happens.
    // If we can deal with this by the time this function is called,
    // there should be no problem, so we will rebuild it here.
    // I would really like to check the sample rate change, but I give up
    // because I can't accept the increased call cost to deal with edge cases.
    ereport(set_current_format());
  }

  int const id = fp->track[0];
  if (id == -1) {
    return TRUE;
  }

  bool updated = false;
  static float const div10000 = 1.f / 10000.f;
  error err = mixer_update_aux_channel(g_mixer,
                                       id,
                                       &(struct aux_channel_effect_params){
                                           .reverb =
                                               {
                                                   .pre_delay = (float)(fp->track[1]) * div10000,
                                                   .band_width = (float)(fp->track[2]) * div10000,
                                                   .diffuse = (float)(fp->track[3]) * div10000,
                                                   .decay = (float)(fp->track[4]) * div10000,
                                                   .damping = (float)(fp->track[5]) * div10000,
                                                   .excursion = (float)(fp->track[6]) * div10000,
                                                   .wet = slider_to_db(fp->track[7]),
                                               },
                                       },
                                       &updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (updated) {
    g_need_update_view = true; // FIXME: There is no place to display the parameters of Aux!
  }
cleanup:
  ereport(err);
  return TRUE;
}

static void hide_all(HWND window) {
  HWND h = NULL;
  for (;;) {
    h = FindWindowExW(window, h, NULL, NULL);
    if (!h) {
      break;
    }
    ShowWindow(h, SW_HIDE);
  }
}

static BOOL filter_init(FILTER *fp) {
  aviutl_set_pointers(fp, NULL);
  error err = aviutl_init();
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = mixer_create(&g_mixer);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  if (efailed(err)) {
    ereport(err);
    return FALSE;
  }
  return TRUE;
}

static BOOL filter_exit(FILTER *fp) {
  aviutl_set_pointers(fp, NULL);
  ereport(mixer_destroy(&g_mixer));
  if (g_warming_buffer) {
    ereport(mem_aligned_free(&g_warming_buffer));
  }
  ereport(aviutl_exit());
  return TRUE;
}

enum {
  timer_event_update_view = 1,
};

enum {
  ctl_id_combo = 1,
};

static BOOL wndproc_init(HWND window) {
  error err = eok();
  hide_all(window);
  g_font = CreateFontW(-14,
                       0,
                       0,
                       0,
                       FW_NORMAL,
                       0,
                       0,
                       0,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Courier New");
  if (!g_font) {
    err = errg(err_fail);
    goto cleanup;
  }

  RECT r = {0};
  if (!GetClientRect(window, &r)) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  g_id_combo = CreateWindowW(L"COMBOBOX",
                             NULL,
                             WS_CHILD | WS_TABSTOP | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             8,
                             8,
                             r.right - r.left - 16,
                             400,
                             window,
                             (HMENU)ctl_id_combo,
                             get_hinstance(),
                             NULL);
  if (!g_id_combo) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  SendMessageW(g_id_combo, WM_SETFONT, (WPARAM)g_font, 0);
  wchar_t s[32] = {0};
  for (int i = 0; i < 100; ++i) {
    wsprintfW(s, L"ID: %03d", i);
    SendMessageW(g_id_combo, CB_ADDSTRING, 0, (LPARAM)s);
  }
  SendMessageW(g_id_combo, CB_SETCURSEL, 0, 0);

  g_params_label = CreateWindowW(L"STATIC",
                                 L"",
                                 WS_CHILD | WS_VISIBLE | ES_LEFT,
                                 8,
                                 8 + 40,
                                 r.right - r.left - 16,
                                 r.bottom - r.top - 16 - 40,
                                 window,
                                 0,
                                 get_hinstance(),
                                 NULL);
  if (!g_params_label) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  SendMessageW(g_params_label, WM_SETFONT, (WPARAM)g_font, 0);
cleanup:
  ereport(err);
  return FALSE;
}

static BOOL wndproc_exit(HWND window) {
  if (g_view_update_timer_id) {
    KillTimer(window, g_view_update_timer_id);
    g_view_update_timer_id = 0;
  }
  if (g_font) {
    if (g_id_combo) {
      SendMessageW(g_id_combo, WM_SETFONT, 0, 0);
    }
    if (g_params_label) {
      SendMessageW(g_params_label, WM_SETFONT, 0, 0);
    }
    DeleteObject(g_font);
    g_font = NULL;
  }
  return FALSE;
}

static BOOL wndproc_change_window(HWND window) {
  if (g_view_update_timer_id) {
    KillTimer(window, g_view_update_timer_id);
    g_view_update_timer_id = 0;
  }
  if (IsWindowVisible(window)) {
    g_view_update_timer_id = SetTimer(window, timer_event_update_view, 400, NULL);
  }
  return FALSE;
}

static void update_params_view(void) {
  struct NATIVE_STR tmp = {0};
  struct channel_effect_params_str params = {0};
  bool found = false;
  error err = mixer_get_channel_parameter_str(g_mixer, g_selected_id, &params, &found);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!found) {
    SetWindowTextW(g_params_label, NULL);
    goto cleanup;
  }
  err = sgrow(&tmp, 512);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpym(&tmp,
              NSTR("[Pre Gain]\r\n"),
              NSTR("  Gain      "),
              params.pre_gain,
              NSTR(" dB\r\n\r\n"),
              NSTR("[Lag]\r\n"),
              NSTR("  Duration  "),
              params.lagger_duration,
              NSTR(" ms\r\n\r\n"),
              NSTR("[Low-Shelf EQ]\r\n"),
              NSTR("  Frequency "),
              params.low_shelf_frequency,
              NSTR(" Hz\r\n"),
              NSTR("  Gain      "),
              params.low_shelf_gain,
              NSTR(" dB\r\n\r\n"),
              NSTR("[High-Shelf EQ]\r\n"),
              NSTR("  Frequency "),
              params.high_shelf_frequency,
              NSTR(" Hz\r\n"),
              NSTR("  Gain      "),
              params.high_shelf_gain,
              NSTR(" dB\r\n\r\n"),
              NSTR("[Compressor]\r\n"),
              NSTR("  Threshold "),
              params.dynamics_threshold,
              NSTR(" dB\r\n"),
              NSTR("  Ratio     "),
              params.dynamics_ratio,
              NSTR(":1\r\n"),
              NSTR("  Attack    "),
              params.dynamics_attack,
              NSTR(" μs\r\n"),
              NSTR("  Release   "),
              params.dynamics_release,
              NSTR(" ms\r\n\r\n"),
              NSTR("[Aux1 Send]\r\n"),
              NSTR("  Gain      "),
              params.aux_send,
              NSTR(" dB\r\n\r\n"),
              NSTR("[Post Gain]\r\n"),
              NSTR("  Gain      "),
              params.post_gain,
              NSTR(" dB\r\n"),
              NSTR("  Pan       "),
              params.pan,
              NSTR("\r\n"));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  SetWindowTextW(g_params_label, tmp.ptr);
cleanup:
  ereport(sfree(&tmp));
  ereport(err);
}

static BOOL
filter_wndproc_channel_strip(HWND window, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp) {
  aviutl_set_pointers(fp, editp);
  (void)lparam;
  (void)wparam;

  BOOL r = FALSE;
  switch (message) {
  case WM_FILTER_INIT:
    r = wndproc_init(window);
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_EXIT:
    r = wndproc_exit(window);
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_UPDATE:
    ereport(set_current_format());
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_FILE_OPEN:
    ereport(set_current_format());
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_FILE_UPDATE:
    ereport(set_current_format());
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_CHANGE_WINDOW:
    r = wndproc_change_window(window);
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_SAVE_START:
    g_saving = true;
    break;
  case WM_FILTER_SAVE_END:
    g_saving = false;
    break;
  case WM_COMMAND:
    switch (LOWORD(wparam)) {
    case ctl_id_combo:
      if (HIWORD(wparam) == CBN_SELCHANGE) {
        g_selected_id = SendMessageW(g_id_combo, CB_GETCURSEL, 0, 0);
        update_params_view();
        g_need_update_view = false;
      }
      break;
    }
    break;
  case WM_TIMER:
    switch (wparam) {
    case timer_event_update_view:
      if (g_need_update_view) {
        update_params_view();
        g_need_update_view = false;
      }
      break;
    }
    break;
  }
  return r;
}

// チャンネルストリップ
#define CHANNEL_STRIP_NAME_MBCS "\x83\x60\x83\x83\x83\x93\x83\x6C\x83\x8B\x83\x58\x83\x67\x83\x8A\x83\x62\x83\x76"
static FILTER_DLL g_channel_strip_filter_dll = {
    .flag = FILTER_FLAG_PRIORITY_LOWEST | FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_AUDIO_FILTER |
            FILTER_FLAG_WINDOW_SIZE | FILTER_FLAG_EX_INFORMATION,
    .x = 240 | FILTER_WINDOW_SIZE_CLIENT,
    .y = 540 | FILTER_WINDOW_SIZE_CLIENT,
    .name = CHANNEL_STRIP_NAME_MBCS,
    .information = CHANNEL_STRIP_NAME_MBCS " " VERSION,
    .track_n = 15,
    .track_name =
        (TCHAR *[]){
            "ID",
            "\x93\xFC\x97\xCD\x89\xB9\x97\xCA" /* 入力音量 */,
            "\x92\x78\x89\x84" /* 遅延 */,
            "EQ LoFreq",
            "EQ LoGain",
            "EQ HiFreq",
            "EQ HiGain",
            "C Thresh",
            "C Ratio",
            "C Attack",
            "C Release",
            "Aux ID",
            "Aux Send",
            "\x8F\x6F\x97\xCD\x89\xB9\x97\xCA" /* 出力音量 */,
            "\x8D\xB6\x89\x45" /* 左右 */,
        },
    .track_default = (int[]){-1, 0, 0, 200, 0, 3000, 0, 6000, 0, 1800, 5500, -1, -10000, 0, 0},
    .track_s = (int[]){-1, -10000, 0, 1, -10000, 1, -10000, 0, 0, 0, 0, -1, -10000, -10000, -10000},
    .track_e =
        (int[]){100, 10000, 500, 24000, 10000, 24000, 10000, 10000, 10000, 10000, 10000, 100, 10000, 10000, 10000},
    .func_proc = filter_proc_channel_strip,
    .func_init = filter_init,
    .func_exit = filter_exit,
    .func_WndProc = filter_wndproc_channel_strip,
};

#define AUX1_NAME_MBCS "Aux"
static FILTER_DLL g_aux1_channel_strip_filter_dll = {
    .flag = FILTER_FLAG_PRIORITY_HIGHEST | FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_AUDIO_FILTER | FILTER_FLAG_NO_CONFIG,
    .name = CHANNEL_STRIP_NAME_MBCS " - " AUX1_NAME_MBCS,
    .track_n = 8,
    .track_name = (TCHAR *[]){"ID", "R PreDly", "R LPF", "R Diffuse", "R Decay", "R Damping", "R Excursion", "R Wet"},
    .track_default = (int[]){-1, 0, 10000, 10000, 5000, 50, 5000, 0},
    .track_s = (int[]){-1, 0, 0, 0, 0, 0, 0, -10000},
    .track_e = (int[]){100, 10000, 10000, 10000, 10000, 10000, 10000, 0},
    .func_proc = filter_proc_aux1,
};

#define MASTER_NAME_MBCS "\x83\x7D\x83\x58\x83\x5E\x81\x5B" // マスター
static FILTER_DLL g_master_channel_strip_filter_dll = {
    // FILTER_FLAG_RADIO_BUTTON is not necessary for the operation,
    // but it is necessary to hide the item from ExEdit's menu.
    .flag = FILTER_FLAG_PRIORITY_LOWEST | FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_AUDIO_FILTER | FILTER_FLAG_NO_CONFIG |
            FILTER_FLAG_RADIO_BUTTON,
    .name = CHANNEL_STRIP_NAME_MBCS " - " MASTER_NAME_MBCS,
    .func_proc = filter_proc_master,
};

struct paraout_params {
  FILTER *fp;
  void *editp;

  struct parallel_output *po;

  thrd_t th;
  error err;
};

static void output_notify(void *const userdata,
                          struct mixer *const m,
                          int const channel_type,
                          int const id,
                          float const *restrict const *const buf,
                          size_t const channels,
                          size_t const samples) {
  struct parallel_output_gui_context *ctx = userdata;
  if (!ctx) {
    return;
  }
  struct paraout_params *pp = ctx->userdata;
  if (!pp) {
    return;
  }
  if (efailed(pp->err)) {
    return;
  }
  if (channel_type == mixer_channel_type_other && !ctx->options.export_other) {
    return;
  }
  pp->err = parallel_output_write(
      pp->po, channel_type, id, buf, (size_t)(mixer_get_sample_rate(m)), channels, mixer_get_position(m), samples);
}

static int paraout_thread(void *userdata) {
  error err = eok();
  struct wstr progress_text = {0};
  struct paraout_params *pp = NULL;
  struct parallel_output *po = NULL;
  struct parallel_output_gui_context *ctx = userdata;
  bool cancelled = false;
  if (!ctx) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  pp = ctx->userdata;
  if (!pp) {
    err = errg(err_unexpected);
    goto cleanup;
  }

  FILTER *fp = pp->fp;
  void *editp = pp->editp;
  int s, e;
  if (!fp->exfunc->get_select_frame(editp, &s, &e)) {
    e = fp->exfunc->get_frame_n(editp);
    s = 0;
  }
  FILE_INFO fi = {0};
  if (!fp->exfunc->get_file_info(editp, &fi)) {
    err = errg(err_fail);
    goto cleanup;
  }
  uint64_t const header_size = UINT64_C(44);
  size_t const channels = (size_t)fi.audio_ch;
  size_t const samples_per_frame = (size_t)((fi.audio_rate * fi.video_scale) / fi.video_rate);
  uint64_t const file_size =
      header_size +
      (uint64_t)(samples_per_frame * channels * get_bit_size(ctx->options.bit_format)) * (uint64_t)(e - s + 1);
  if (file_size > UINT32_MAX) {
    err = err(err_type_axr, err_axr_wav_size_limit_exceeded);
    goto cleanup;
  }

  err = parallel_output_create(&po, &ctx->options.filename, ctx->options.bit_format);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  pp->po = po;
  mixer_set_userdata(g_mixer, ctx);
  mixer_set_output_notify_callback(g_mixer, output_notify);

  g_last_frame = s;

  DWORD tick = 0, new_tick;
  int prg = 0, prg_prev = -1;

  for (int i = s; i < e; ++i) {
    cancelled = parallel_output_gui_cancel_requested(ctx);
    if (cancelled) {
      break;
    }
    fp->exfunc->get_audio_filtered(editp, i, g_warming_buffer);
    if (efailed(pp->err)) {
      err = ethru(pp->err);
      pp->err = eok();
      goto cleanup;
    }
    prg = ((i - s) * 100) / (e - s);
    if (prg != prg_prev) {
      ctx->progress_func(ctx, prg);
      prg_prev = prg;
    }
    new_tick = GetTickCount();
    if (tick + 200 < new_tick || new_tick < tick) {
      wchar_t n1[32], n2[32], n3[32];
      err = scpym(&progress_text,
                  ov_utoa((uint64_t)prg, n1),
                  L"% ",
                  ov_utoa((uint64_t)(i - s), n2),
                  L" / ",
                  ov_utoa((uint64_t)(e - s), n3));
      if (efailed(err)) {
        ereport(err);
      } else {
        ctx->progress_text_func(ctx, &progress_text);
      }
      tick = new_tick;
    }
  }
  if (cancelled) {
    err = parallel_output_cancel(po);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  } else {
    ctx->progress_func(ctx, 100);
    ctx->progress_text_func(ctx, &wstr_unmanaged_const(L"ファイルの最終処理中..."));
    err = parallel_output_finalize(po, mixer_get_position(g_mixer));
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }

cleanup:
  ereport(sfree(&progress_text));
  mixer_set_userdata(g_mixer, NULL);
  mixer_set_output_notify_callback(g_mixer, NULL);
  if (po) {
    ereport(parallel_output_destroy(&po));
  }
  if (efailed(err)) {
    if (pp) {
      pp->err = err;
    } else {
      ereport(err);
    }
  }
  ctx->progress_func(ctx, parallel_output_progress_complete);
  return 0;
}

static struct parallel_output_gui_options g_parallel_output_gui_options = {
    .bit_format = parallel_output_bit_format_float32,
    .export_other = true,
};

NODISCARD static error paraout_start(struct parallel_output_gui_context *ctx) {
  struct paraout_params *pp = NULL;
  error err = eok();
  if (!ctx || !ctx->userdata) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  pp = ctx->userdata;
  if (thrd_create(&pp->th, paraout_thread, ctx) != thrd_success) {
    err = errg(err_fail);
    goto cleanup;
  }
cleanup:
  return err;
}

NODISCARD static error paraout_finish(struct parallel_output_gui_context *ctx) {
  struct paraout_params *pp = NULL;
  error err = eok();
  if (!ctx || !ctx->userdata) {
    err = errg(err_unexpected);
    goto cleanup;
  }
  pp = ctx->userdata;
  if (thrd_join(pp->th, NULL) != thrd_success) {
    err = errg(err_fail);
    goto cleanup;
  }
  if (efailed(pp->err)) {
    err = ethru(pp->err);
    pp->err = NULL;
    goto cleanup;
  }
  ereport(sfree(&g_parallel_output_gui_options.filename));
  g_parallel_output_gui_options = ctx->options;
  ctx->options.filename = (struct wstr){0};
cleanup:
  return err;
}

static BOOL
filter_wndproc_parallel_output(HWND window, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp) {
  (void)window;
  (void)wparam;
  (void)lparam;
  aviutl_set_pointers(fp, editp);
  BOOL r = FALSE;
  switch (message) {
  case WM_FILTER_FILE_OPEN:
    if (g_parallel_output_gui_options.filename.ptr != NULL) {
      g_parallel_output_gui_options.filename.ptr[0] = L'\0';
      g_parallel_output_gui_options.filename.len = 0;
    }
    g_parallel_output_gui_options.bit_format = parallel_output_bit_format_float32;
    g_parallel_output_gui_options.export_other = true;
    break;
  case WM_FILTER_EXPORT: {
    error err = parallel_output_gui_show(&g_parallel_output_gui_options,
                                         paraout_start,
                                         paraout_finish,
                                         &(struct paraout_params){
                                             .fp = fp,
                                             .editp = editp,
                                         });
    if (efailed(err)) {
      error_message_box(err, aviutl_get_exedit_window_must(), L"処理中にエラーが発生しました。");
    }
    aviutl_set_pointers(NULL, NULL);
  } break;
  }
  return r;
}

NODISCARD static error
find_token(struct wstr const *const input, wchar_t const *const key, int *const pos, size_t *const len) {
  ptrdiff_t p = 0;
  error err = sstr(input, key, &p);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (p == -1) {
    *pos = -1;
    *len = 0;
    goto cleanup;
  }
  p += wcslen(key);
  size_t l = 0;
  wchar_t const *cur = input->ptr + p, *end = input->ptr + input->len;
  while (cur < end) {
    wchar_t const c = *cur;
    if (c == L'\r' || c == L'\n') {
      break;
    }
    ++l;
    ++cur;
  }
  *pos = p;
  *len = l;

cleanup:
  return err;
}

static BOOL filter_project_load_parallel_output(FILTER *fp, void *editp, void *data, int size) {
  (void)fp;
  (void)editp;
  struct wstr const src = {.ptr = data, .len = (size_t)size};
  struct wstr filename = {0};
  int bit_format = 0;
  bool export_other = true;

  ptrdiff_t pos = -1;
  size_t len = 0;
  error err = find_token(&src, L"parallel_output_filename=", &pos, &len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (pos != -1 && len > 0) {
    err = sncpy(&filename, src.ptr + pos, len);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  err = find_token(&src, L"parallel_output_bit_format=", &pos, &len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (pos != -1 && len > 0) {
    int64_t v = 0;
    if (ov_atoi(src.ptr + pos, &v, false)) {
      bit_format = (int)v;
    }
  }
  err = find_token(&src, L"parallel_output_export_other=", &pos, &len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (pos != -1 && len > 0) {
    int64_t v = 0;
    if (ov_atoi(src.ptr + pos, &v, false)) {
      export_other = v != 0;
    }
  }
  err = scpy(&g_parallel_output_gui_options.filename, filename.ptr ? filename.ptr : L"");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  switch (bit_format) {
  case parallel_output_bit_format_int16:
  case parallel_output_bit_format_float32:
    g_parallel_output_gui_options.bit_format = (int)bit_format;
    break;
  default:
    g_parallel_output_gui_options.bit_format = parallel_output_bit_format_float32;
    break;
  }
  g_parallel_output_gui_options.export_other = export_other;
cleanup:
  ereport(sfree(&filename));
  if (efailed(err)) {
    ereport(err);
    return FALSE;
  }
  return TRUE;
}

static BOOL filter_project_save_parallel_output(FILTER *fp, void *editp, void *data, int *size) {
  (void)fp;
  (void)editp;
  struct wstr tmp = {0};
  wchar_t bit_format_str[32] = {0};
  error err = scpym(&tmp,
                    L"parallel_output_filename=",
                    g_parallel_output_gui_options.filename.ptr ? g_parallel_output_gui_options.filename.ptr : L"",
                    L"\r\n",
                    L"parallel_output_bit_format=",
                    ov_itoa((int64_t)g_parallel_output_gui_options.bit_format, bit_format_str),
                    L"\r\n",
                    L"parallel_output_export_other=",
                    g_parallel_output_gui_options.export_other ? L"1" : L"0",
                    L"\r\n");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  if (size) {
    *size = (int)(tmp.len * sizeof(wchar_t));
  }
  if (data) {
    memcpy(data, tmp.ptr, tmp.len * sizeof(wchar_t));
  }
cleanup:
  ereport(sfree(&tmp));
  if (efailed(err)) {
    ereport(err);
    return FALSE;
  }
  return TRUE;
}

static BOOL filter_exit_parallel_output(FILTER *fp) {
  (void)fp;
  ereport(sfree(&g_parallel_output_gui_options.filename));
  return TRUE;
}

// パラアウト
#define PARALLEL_OUTPUT_NAME_MBCS "\x83\x70\x83\x89\x83\x41\x83\x45\x83\x67"
static FILTER_DLL g_parallel_output_filter_dll = {
    .flag = FILTER_FLAG_PRIORITY_LOWEST | FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_EXPORT | FILTER_FLAG_NO_CONFIG,
    .name = CHANNEL_STRIP_NAME_MBCS " - " PARALLEL_OUTPUT_NAME_MBCS,
    .func_exit = filter_exit_parallel_output,
    .func_WndProc = filter_wndproc_parallel_output,
    .func_project_load = filter_project_load_parallel_output,
    .func_project_save = filter_project_save_parallel_output,
};

FILTER_DLL __declspec(dllexport) * *__stdcall GetFilterTableList(void);
FILTER_DLL __declspec(dllexport) * *__stdcall GetFilterTableList(void) {
  static FILTER_DLL *filter_list[] = {&g_channel_strip_filter_dll,
                                      &g_aux1_channel_strip_filter_dll,
                                      &g_master_channel_strip_filter_dll,
                                      &g_parallel_output_filter_dll,
                                      NULL};
  return (FILTER_DLL **)&filter_list;
}

static void
error_reporter(error const e, struct NATIVE_STR const *const message, struct ov_filepos const *const filepos) {
  struct NATIVE_STR tmp = {0};
  struct NATIVE_STR msg = {0};
  error err = error_to_string(e, &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  NATIVE_CHAR buf[1024] = {0};
  wsprintfW(buf, NSTR("\r\n(reported at %hs:%ld %hs())\r\n"), filepos->file, filepos->line, filepos->func);
  err = scpym(&msg, message->ptr, buf, tmp.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  OutputDebugStringW(msg.ptr);

cleanup:
  if (efailed(err)) {
    OutputDebugStringW(NSTR("failed to report error"));
    efree(&err);
  }
  eignore(sfree(&msg));
  eignore(sfree(&tmp));
}

static BOOL main_init(HINSTANCE const inst) {
  if (!ov_init(generic_error_message_mapper_jp)) {
    return FALSE;
  }
  error_register_reporter(error_reporter);
  ereportmsg(error_axr_init(), &native_unmanaged(NSTR("エラーメッセージマッパーの登録に失敗しました。")));
  set_hinstance(inst);
  return TRUE;
}

static BOOL main_exit(void) {
  ov_exit();
  return TRUE;
}

BOOL APIENTRY DllMain(HINSTANCE const inst, DWORD const reason, LPVOID const reserved);
BOOL APIENTRY DllMain(HINSTANCE const inst, DWORD const reason, LPVOID const reserved) {
  (void)reserved;
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    return main_init(inst);
  case DLL_PROCESS_DETACH:
    return main_exit();
  }
  return TRUE;
}

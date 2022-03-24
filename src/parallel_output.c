#include "parallel_output.h"

#include "ovnum.h"
#include "ovutil/str.h"
#include "ovutil/win32.h"

#include "mixer.h"

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct entry {
  int id;
  HANDLE h;
  size_t sample_rate;
  size_t channels;
  uint64_t pos;

  struct entry *next;
};

struct parallel_output {
  struct entry *head;
  struct entry *aux_head;
  struct entry *other_head;
  struct wstr filename;
  int bit_format;
  void *buffer;
  size_t buffer_size;
  error err;
};

NODISCARD static error entry_destroy(struct entry **ep) {
  if (!ep || !*ep) {
    return errg(err_invalid_arugment);
  }
  struct entry *e = *ep;
  if (e->h != INVALID_HANDLE_VALUE) {
    CloseHandle(e->h);
    e->h = INVALID_HANDLE_VALUE;
  }
  ereport(mem_free(ep));
  return eok();
}

NODISCARD static error entry_create(struct entry **ep) {
  if (!ep || *ep) {
    return errg(err_invalid_arugment);
  }
  error err = mem(ep, 1, sizeof(struct entry));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct entry *e = *ep;
  *e = (struct entry){.h = INVALID_HANDLE_VALUE};
cleanup:
  if (efailed(err)) {
    if (*ep) {
      ereport(entry_destroy(ep));
    }
  }
  return err;
}

static struct entry *get_head(struct parallel_output const *const p, int const channel_type) {
  switch (channel_type) {
  case mixer_channel_type_channel:
    return p->head;
  case mixer_channel_type_aux_channel:
    return p->aux_head;
  case mixer_channel_type_other:
    return p->other_head;
  }
  return NULL;
}

static void set_head(struct parallel_output *const p, int const channel_type, struct entry *new_head) {
  switch (channel_type) {
  case mixer_channel_type_channel:
    p->head = new_head;
    return;
  case mixer_channel_type_aux_channel:
    p->aux_head = new_head;
    return;
  case mixer_channel_type_other:
    p->other_head = new_head;
    return;
  }
}

static void entry_free_all(struct parallel_output *const p, int const channel_type) {
  struct entry *e = get_head(p, channel_type);
  set_head(p, channel_type, NULL);
  struct entry *next = NULL;
  while (e) {
    next = e->next;
    ereport(entry_destroy(&e));
    e = next;
  }
}

NODISCARD error parallel_output_create(struct parallel_output **pp,
                                       struct wstr const *const filename,
                                       int const bit_format) {
  if (!pp || *pp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(pp, 1, sizeof(struct parallel_output));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct parallel_output *p = *pp;
  *p = (struct parallel_output){
      .bit_format = bit_format,
  };
  err = scpy(&p->filename, filename->ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  size_t const channels = 2;
  p->buffer_size = 48000 * channels * get_bit_size(bit_format) / 10;
  err = mem_aligned_alloc(&p->buffer, p->buffer_size, 1, 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  if (efailed(err)) {
    if (*pp) {
      ereport(parallel_output_destroy(pp));
    }
  }
  return err;
}

NODISCARD error parallel_output_destroy(struct parallel_output **pp) {
  if (!pp || !*pp) {
    return errg(err_invalid_arugment);
  }
  struct parallel_output *p = *pp;
  entry_free_all(p, false);
  entry_free_all(p, true);
  ereport(sfree(&p->filename));
  if (p->buffer) {
    ereport(mem_aligned_free(&p->buffer));
  }
  ereport(mem_free(pp));
  return eok();
}

NODISCARD static error find(struct parallel_output *p, int const id, int const channel_type, struct entry **const r) {
  if (!p) {
    return errg(err_invalid_arugment);
  }
  if (!r) {
    return errg(err_null_pointer);
  }
  if (channel_type == mixer_channel_type_other && id != 0) {
    return errg(err_unexpected);
  }
  struct entry *e = NULL;
  struct entry *prev = NULL;
  for (e = get_head(p, channel_type); e; e = e->next) {
    if (e->id == id) {
      *r = e;
      return eok();
    }
    if (e->id > id) {
      goto not_found;
    }
    if (!e->next || e->next->id > id) {
      prev = e;
      goto not_found;
    }
  }

not_found:
  e = NULL;
  error err = entry_create(&e);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  e->id = id;
  if (prev) {
    e->next = prev->next;
    prev->next = e;
  } else {
    e->next = get_head(p, channel_type);
    set_head(p, channel_type, e);
  }
  *r = e;
  return eok();
}

enum wave_format {
  wave_format_pcm = 1,
  wave_format_ieee_float = 3,
};

struct __attribute__((packed)) wave_header {
  uint32_t riff_signature; // "RIFF"
  uint32_t riff_size;
  uint32_t format; // "WAVE"

  uint32_t fmt_signature; // "fmt "
  uint32_t fmt_size;
  uint16_t format_tag; // wave_format_ieee_float
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t avg_bytes_per_sec;
  uint16_t block_align;
  uint16_t bits;

  uint32_t data_signature;
  uint32_t data_size;
};

NODISCARD static error
generate_file_name(struct wstr *const dest, struct wstr const *const filename, int const id, int const channel_type) {
  struct wstr tmp = {0};
  wchar_t idbuf[32];
  size_t extpos = 0;
  error err = extract_file_extension(filename, &extpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = sncpy(&tmp, filename->ptr, extpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  wchar_t *postfix = NULL;
  wchar_t *idstr = NULL;
  switch (channel_type) {
  case mixer_channel_type_channel:
    postfix = L"_";
    idstr = ov_itoa((int64_t)id, idbuf);
    break;
  case mixer_channel_type_aux_channel:
    postfix = L"_aux";
    idstr = ov_itoa((int64_t)id, idbuf);
    break;
  case mixer_channel_type_other:
    postfix = L"";
    idstr = L"";
    break;
  }
  err = scatm(&tmp, postfix, idstr, filename->ptr + extpos);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpy(dest, tmp.ptr);
cleanup:
  ereport(sfree(&tmp));
  return err;
}

NODISCARD static error create_file(HANDLE *const dest,
                                   struct wstr const *const filename,
                                   size_t const sample_rate,
                                   size_t const channels,
                                   int const bit_format,
                                   int const id,
                                   int const channel_type) {
  struct wstr tmp = {0};
  error err = generate_file_name(&tmp, filename, id, channel_type);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  HANDLE h = CreateFileW(
      tmp.ptr, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    struct wstr errmsg = {0};
    ereport(scpym(&errmsg, L"ファイル \"", tmp.ptr, L"\" を作成できませんでした。"));
    err = emsg(err_type_hresult, HRESULT_FROM_WIN32(GetLastError()), errmsg.ptr ? &errmsg : NULL);
    goto cleanup;
  }
  struct wave_header wh = {
      .riff_signature = ('R' << 0) | ('I' << 8) | ('F' << 16) | ('F' << 24),
      .format = ('W' << 0) | ('A' << 8) | ('V' << 16) | ('E' << 24),
      .fmt_signature = ('f' << 0) | ('m' << 8) | ('t' << 16) | (' ' << 24),
      .fmt_size = 16,
      .format_tag = bit_format == parallel_output_bit_format_float32 ? wave_format_ieee_float : wave_format_pcm,
      .channels = (uint16_t)channels,
      .sample_rate = (uint32_t)sample_rate,
      .bits = (uint16_t)(get_bit_size(bit_format) * 8),
      .data_signature = ('d' << 0) | ('a' << 8) | ('t' << 16) | ('a' << 24),
  };
  wh.block_align = wh.channels * (wh.bits / 8);
  wh.avg_bytes_per_sec = wh.sample_rate * (uint32_t)(wh.block_align);
  size_t sz;
  err = write_file(h, &wh, sizeof(struct wave_header), &sz);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (sizeof(struct wave_header) != sz) {
    err = errg(err_fail);
    goto cleanup;
  }
  *dest = h;

cleanup:
  ereport(sfree(&tmp));
  return err;
}

static inline void float_to_interleaved_float(float *restrict const dest,
                                              float const *restrict const *const src,
                                              size_t const channels,
                                              size_t const src_offset,
                                              size_t const samples) {
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dest[i * channels + ch] = src[ch][src_offset + i];
    }
  }
}

static inline float clip(float x) {
  if (x < -1.f) {
    x = -1.f;
  }
  if (x > 1.f) {
    x = 1.f;
  }
  return x;
}

static inline void float_to_interleaved_int16(int16_t *restrict const dest,
                                              float const *restrict const *const src,
                                              size_t const channels,
                                              size_t const src_offset,
                                              size_t const samples) {
  static float const m = 32767.f;
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dest[i * channels + ch] = (int16_t)(clip(src[ch][src_offset + i]) * m);
    }
  }
}

static inline size_t minzu(size_t const a, size_t const b) { return a < b ? a : b; }

NODISCARD static error fill(struct parallel_output *p, struct entry *e, size_t const samples) {
  float *buffer = p->buffer;
  size_t const channels = e->channels;
  size_t const bit_size = get_bit_size(p->bit_format);
  size_t const block_size = p->buffer_size / (bit_size * channels);
  size_t remain = samples;
  HANDLE h = e->h;
  error err = eok();
  memset(buffer, 0, block_size * channels * bit_size);
  while (remain) {
    size_t const sz = minzu(block_size, (size_t)(remain));
    size_t const bytes = sz * channels * bit_size;
    size_t written;
    err = write_file(h, buffer, bytes, &written);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    if (written != bytes) {
      err = errg(err_fail);
      goto cleanup;
    }
    remain -= sz;
  }
cleanup:
  return err;
}

NODISCARD static error write(struct parallel_output *p,
                             int const id,
                             int const channel_type,
                             float const *restrict const *const buf,
                             size_t const sample_rate,
                             size_t const channels,
                             uint64_t const pos,
                             size_t const samples) {
  if (!p || !buf || !sample_rate || !channels) {
    return errg(err_invalid_arugment);
  }
  struct entry *e = NULL;
  error err = find(p, id, channel_type, &e);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (e->h == INVALID_HANDLE_VALUE) {
    err = create_file(&e->h, &p->filename, sample_rate, channels, p->bit_format, id, channel_type);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    e->sample_rate = sample_rate;
    e->channels = channels;
  } else {
    if (e->sample_rate != sample_rate || e->channels != channels) {
      err = errg(err_fail);
      goto cleanup;
    }
  }
  void *buffer = p->buffer;
  size_t const buffer_size = p->buffer_size;
  int const bit_format = p->bit_format;
  size_t const bit_size = get_bit_size(bit_format);
  size_t const block_size = buffer_size / (bit_size * channels);
  HANDLE h = e->h;
  uint64_t entry_pos = e->pos;
  if (entry_pos < pos) {
    err = fill(p, e, (size_t)(pos - entry_pos));
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  size_t processed = 0;
  while (processed < samples) {
    size_t const sz = minzu(block_size, samples - processed);
    switch (bit_format) {
    case parallel_output_bit_format_float32:
      float_to_interleaved_float(buffer, buf, channels, processed, sz);
      break;
    case parallel_output_bit_format_int16:
      float_to_interleaved_int16(buffer, buf, channels, processed, sz);
      break;
    }
    size_t const bytes = sz * channels * bit_size;
    size_t written;
    err = write_file(h, buffer, bytes, &written);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    if (written != bytes) {
      err = errg(err_fail);
      goto cleanup;
    }
    processed += sz;
  }
  e->pos = pos + samples;
cleanup:
  return err;
}

NODISCARD error parallel_output_write(struct parallel_output *p,
                                      int const channel_type,
                                      int const id,
                                      float const *restrict const *const buf,
                                      size_t const sample_rate,
                                      size_t const channels,
                                      uint64_t const pos,
                                      size_t const samples) {
  error err = write(p, id, channel_type, buf, sample_rate, channels, pos, samples);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD static error finalize(struct parallel_output *p, uint64_t const pos, int const channel_type) {
  error err = eok();
  for (struct entry *e = get_head(p, channel_type); e; e = e->next) {
    if (e->pos < pos) {
      err = fill(p, e, (size_t)(pos - e->pos));
      if (efailed(err)) {
        err = ethru(err);
        return err;
      }
      e->pos = pos;
    }
    if (!SetFilePointer(e->h, offsetof(struct wave_header, riff_size), NULL, FILE_BEGIN)) {
      err = errhr(HRESULT_FROM_WIN32(GetLastError()));
      return err;
    }
    uint32_t sz = sizeof(struct wave_header) - 8 + (uint32_t)(pos * e->channels * get_bit_size(p->bit_format));
    size_t written;
    err = write_file(e->h, &sz, sizeof(uint32_t), &written);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
    if (written != sizeof(uint32_t)) {
      err = errg(err_fail);
      return err;
    }
    if (!SetFilePointer(e->h, offsetof(struct wave_header, data_size), NULL, FILE_BEGIN)) {
      err = errhr(HRESULT_FROM_WIN32(GetLastError()));
      return err;
    }
    sz -= sizeof(struct wave_header) - 8;
    err = write_file(e->h, &sz, sizeof(uint32_t), &written);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
    if (written != sizeof(float)) {
      err = errg(err_fail);
      return err;
    }
  }
  return eok();
}

NODISCARD error parallel_output_finalize(struct parallel_output *p, uint64_t const pos) {
  if (!p) {
    return errg(err_invalid_arugment);
  }
  error err = finalize(p, pos, mixer_channel_type_channel);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  entry_free_all(p, mixer_channel_type_channel);
  err = finalize(p, pos, mixer_channel_type_aux_channel);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  entry_free_all(p, mixer_channel_type_aux_channel);
  err = finalize(p, pos, mixer_channel_type_other);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  entry_free_all(p, mixer_channel_type_other);
  return eok();
}

static void entry_free_and_remove_all(struct parallel_output *const p, int const channel_type) {
  struct wstr tmp = {0};
  error err = eok();
  struct entry *e = get_head(p, channel_type);
  set_head(p, channel_type, NULL);
  struct entry *next = NULL;
  while (e) {
    next = e->next;
    err = generate_file_name(&tmp, &p->filename, e->id, channel_type);
    ereport(entry_destroy(&e));
    if (esucceeded(err)) {
      if (!DeleteFileW(tmp.ptr)) {
        struct wstr errmsg = {0};
        ereport(scpym(&errmsg, L"ファイル \"", tmp.ptr, L"\" を削除できませんでした。"));
        ereport(emsg(err_type_hresult, HRESULT_FROM_WIN32(GetLastError()), errmsg.ptr ? &errmsg : NULL));
      }
    } else {
      ereport(err);
    }
    e = next;
  }
  ereport(sfree(&tmp));
}

NODISCARD error parallel_output_cancel(struct parallel_output *p) {
  if (!p) {
    return errg(err_invalid_arugment);
  }
  entry_free_and_remove_all(p, mixer_channel_type_channel);
  entry_free_and_remove_all(p, mixer_channel_type_aux_channel);
  entry_free_and_remove_all(p, mixer_channel_type_other);
  return eok();
}

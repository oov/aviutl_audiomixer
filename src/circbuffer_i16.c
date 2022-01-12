#include "circbuffer_i16.h"

#include <stdalign.h>

struct circbuffer_i16 {
  int16_t *ptr;

  size_t channels;
  size_t buffer_size;
  size_t remain;
  size_t writecur;
};

NODISCARD static error allocate(struct circbuffer_i16 *const c) {
  static size_t const align = 16, block_size = 4;

  error err = eok();
  size_t const real_buffer_size = ((c->buffer_size * c->channels) + block_size - 1) & ~(block_size - 1);
  if (!c->ptr) {
    err = mem_aligned_alloc(&c->ptr, real_buffer_size, sizeof(int16_t), align);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    memset(c->ptr, 0, real_buffer_size * sizeof(int16_t));
  }
cleanup:
  return err;
}

static void release(struct circbuffer_i16 *const c) {
  if (c->ptr) {
    ereport(mem_aligned_free(&c->ptr));
  }
}

static size_t readbuf(struct circbuffer_i16 const *const c,
                      int16_t const *restrict const src,
                      int16_t *restrict const dest,
                      size_t const samples) {
  size_t const buffer_size = c->buffer_size;
  size_t const channels = c->channels;
  size_t const remain = c->remain;
  size_t const readsize = remain >= samples ? samples : remain;
  size_t readcur = c->writecur + buffer_size - remain;
  if (readcur >= buffer_size) {
    readcur -= buffer_size;
  }
  size_t const sz = buffer_size - readcur;
  if (sz >= readsize) {
    memcpy(dest, src + readcur * channels, readsize * channels * sizeof(int16_t));
    return readsize;
  }
  size_t const sz2 = readsize - sz;
  memcpy(dest, src + readcur * channels, sz * channels * sizeof(int16_t));
  memcpy(dest + sz * channels, src, sz2 * channels * sizeof(int16_t));
  return readsize;
}

static inline void conv(float *restrict const *const dest,
                        int16_t const *restrict const src,
                        float const mul,
                        size_t const channels,
                        size_t const n,
                        size_t const dest_offset,
                        size_t const src_offset) {
  int16_t const *restrict sp = src + (src_offset * channels);
  for (size_t i = 0; i < n; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dest[ch][dest_offset + i] = (float)(sp[i * channels + ch]) * mul;
    }
  }
}

static size_t readbuf_float(struct circbuffer_i16 const *const c,
                            int16_t const *restrict const src,
                            float *restrict const *const dest,
                            float const mul,
                            size_t const samples) {
  size_t const buffer_size = c->buffer_size;
  size_t const channels = c->channels;
  size_t const remain = c->remain;
  size_t const readsize = remain >= samples ? samples : remain;
  size_t readcur = c->writecur + buffer_size - remain;
  if (readcur >= buffer_size) {
    readcur -= buffer_size;
  }
  size_t const sz = buffer_size - readcur;
  if (sz >= readsize) {
    conv(dest, src, mul, channels, readsize, 0, readcur);
    return readsize;
  }
  size_t const sz2 = readsize - sz;
  conv(dest, src, mul, channels, sz, 0, readcur);
  conv(dest, src, mul, channels, sz2, sz, 0);
  return readsize;
}

static size_t writebuf(struct circbuffer_i16 const *const c,
                       int16_t const *restrict const src,
                       int16_t *restrict const dest,
                       size_t const samples,
                       size_t const offset) {
  size_t const channels = c->channels;
  size_t const buffer_size = c->buffer_size;
  size_t const writecur = c->writecur;
  size_t const sz = buffer_size - writecur;
  if (sz >= samples) {
    memcpy(dest + writecur * channels, src + offset, samples * channels * sizeof(int16_t));
    size_t cur = writecur + samples;
    if (cur == buffer_size) {
      cur = 0;
    }
    return cur;
  }
  size_t const sz2 = samples - sz;
  memcpy(dest + writecur * channels, src + offset, sz * channels * sizeof(int16_t));
  memcpy(dest, src + (offset + sz) * channels, sz2 * channels * sizeof(int16_t));
  return sz2;
}

static size_t fillbuf(struct circbuffer_i16 const *const c, int16_t *restrict const dest, size_t const samples) {
  size_t const channels = c->channels;
  size_t const buffer_size = c->buffer_size;
  size_t const writecur = c->writecur;
  size_t const sz = buffer_size - writecur;
  if (sz >= samples) {
    memset(dest + writecur * channels, 0, samples * channels * sizeof(int16_t));
    size_t cur = writecur + samples;
    if (cur == buffer_size) {
      cur = 0;
    }
    return cur;
  }
  size_t const sz2 = samples - sz;
  memset(dest + writecur * channels, 0, sz * channels * sizeof(int16_t));
  memset(dest, 0, sz2 * channels * sizeof(int16_t));
  return sz2;
}

NODISCARD error circbuffer_i16_create(struct circbuffer_i16 **const cp) {
  if (!cp || *cp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(cp, 1, sizeof(struct circbuffer_i16));
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  **cp = (struct circbuffer_i16){0};
  return eok();
}

NODISCARD error circbuffer_i16_destroy(struct circbuffer_i16 **const cp) {
  if (!cp || !*cp) {
    return errg(err_invalid_arugment);
  }
  struct circbuffer_i16 *const c = *cp;
  release(c);
  ereport(mem_free(cp));
  return eok();
}

NODISCARD error circbuffer_i16_set_buffer_size(struct circbuffer_i16 *const c, size_t const buffer_size) {
  if (!c || !buffer_size) {
    return errg(err_invalid_arugment);
  }
  if (!c->channels) {
    c->buffer_size = buffer_size;
    return eok();
  }
  if (c->remain > buffer_size) {
    return errg(err_not_sufficient_buffer);
  }
  if (!c->remain) {
    release(c);
    c->buffer_size = buffer_size;
    c->writecur = 0;
    error err = allocate(c);
    if (efailed(err)) {
      err = ethru(err);
    }
    return err;
  }
  struct circbuffer_i16 tmp = {.channels = c->channels, .buffer_size = buffer_size};
  error err = allocate(&tmp);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  readbuf(c, (int16_t const *restrict const)c->ptr, (int16_t *restrict const)tmp.ptr, c->remain);
  release(c);
  c->ptr = tmp.ptr;
  tmp.ptr = NULL;
  c->writecur = c->remain;
  c->buffer_size = buffer_size;
  return err;
}

void circbuffer_i16_clear(struct circbuffer_i16 *const c) {
  c->remain = 0;
  c->writecur = 0;
}

NODISCARD error circbuffer_i16_set_channels(struct circbuffer_i16 *const c, size_t const channels) {
  if (!c || !channels) {
    return errg(err_invalid_arugment);
  }
  if (c->channels == channels) {
    return eok();
  }
  if (!c->buffer_size) {
    c->channels = channels;
    return eok();
  }
  struct circbuffer_i16 tmp = {
      .buffer_size = c->buffer_size,
      .channels = channels,
  };
  error err = allocate(&tmp);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  release(c);
  c->channels = channels;
  c->ptr = tmp.ptr;
  c->remain = 0;
  c->writecur = 0;
  return eok();
}

size_t circbuffer_i16_get_channels(struct circbuffer_i16 const *const c) { return c->channels; }

size_t circbuffer_i16_get_remain(struct circbuffer_i16 const *const c) { return c->remain; }

NODISCARD error circbuffer_i16_write_offset(struct circbuffer_i16 *const c,
                                            int16_t const *restrict const src,
                                            size_t const samples,
                                            size_t const offset) {
  if (!c || !src) {
    return errg(err_invalid_arugment);
  }
  if (!c->channels) {
    return errg(err_unexpected);
  }
  if (!samples) {
    return eok();
  }
  if (!c->ptr || c->remain + samples > c->buffer_size) {
    error err = circbuffer_i16_set_buffer_size(c, c->remain + samples);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
  }
  c->writecur = writebuf(c, src, (int16_t *restrict const)c->ptr, samples, offset);
  c->remain += samples;
  return eok();
}

NODISCARD error circbuffer_i16_write(struct circbuffer_i16 *const c,
                                     int16_t const *restrict const src,
                                     size_t const samples) {
  error err = circbuffer_i16_write_offset(c, src, samples, 0);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD error circbuffer_i16_write_silence(struct circbuffer_i16 *const c, size_t const samples) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!c->channels) {
    return errg(err_unexpected);
  }
  if (!samples) {
    return eok();
  }
  if (!c->ptr || c->remain + samples > c->buffer_size) {
    error err = circbuffer_i16_set_buffer_size(c, c->remain + samples);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
  }
  c->writecur = fillbuf(c, (int16_t *restrict const)c->ptr, samples);
  c->remain += samples;
  return eok();
}

NODISCARD error circbuffer_i16_write_nogrow(struct circbuffer_i16 *const c,
                                            int16_t const *restrict const src,
                                            size_t const samples) {
  if (!c || !src) {
    return errg(err_invalid_arugment);
  }
  if (!c->ptr || !c->channels) {
    return errg(err_unexpected);
  }
  if (!samples) {
    return eok();
  }
  size_t sz = samples;
  if (samples >= c->buffer_size) {
    c->remain = 0;
    c->writecur = 0;
    sz = c->buffer_size;
  } else if (c->remain + samples > c->buffer_size) {
    c->remain = c->buffer_size - samples;
  }
  error err = circbuffer_i16_write_offset(c, src, sz, samples - sz);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD error circbuffer_i16_read(struct circbuffer_i16 *const c,
                                    int16_t *restrict const dest,
                                    size_t const samples,
                                    size_t *const written) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!dest) {
    return errg(err_null_pointer);
  }
  if (!samples || !c->remain) {
    if (written) {
      *written = 0;
    }
    return eok();
  }
  size_t const readsize = readbuf(c, (int16_t const *restrict const)c->ptr, dest, samples);
  c->remain -= readsize;
  if (written) {
    *written = readsize;
  }
  return eok();
}

NODISCARD error circbuffer_i16_read_as_float(struct circbuffer_i16 *const c,
                                             float *restrict const *const dest,
                                             size_t const samples,
                                             float const mul,
                                             size_t *const written) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!dest) {
    return errg(err_null_pointer);
  }
  if (!samples || !c->remain) {
    if (written) {
      *written = 0;
    }
    return eok();
  }
  size_t const readsize = readbuf_float(c, (int16_t const *restrict const)c->ptr, dest, mul, samples);
  c->remain -= readsize;
  if (written) {
    *written = readsize;
  }
  return eok();
}

NODISCARD error circbuffer_i16_discard(struct circbuffer_i16 *const c, size_t const samples, size_t *const discarded) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!samples || !c->remain) {
    if (discarded) {
      *discarded = 0;
    }
    return eok();
  }
  size_t const readsize = c->remain >= samples ? samples : c->remain;
  c->remain -= readsize;
  if (discarded) {
    *discarded = readsize;
  }
  return eok();
}

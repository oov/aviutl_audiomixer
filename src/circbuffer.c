#include "circbuffer.h"

#include <stdalign.h>

struct circbuffer {
  float **ptr;
  size_t len;
  size_t cap;

  size_t buffer_size;
  size_t remain;
  size_t writecur;
};

NODISCARD static error allocate(struct circbuffer *const c, size_t const start_index) {
  static size_t const align = 16, block_size = 4;

  error err = eok();
  size_t const real_buffer_size = (c->buffer_size + block_size - 1) & ~(block_size - 1);
  for (size_t i = start_index, len = c->len; i < len; ++i) {
    if (!c->ptr[i]) {
      err = mem_aligned_alloc(c->ptr + i, real_buffer_size, sizeof(float), align);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
      memset(c->ptr[i], 0, real_buffer_size * sizeof(float));
    }
  }
cleanup:
  return err;
}

static void release(struct circbuffer *const c, size_t const start_index) {
  for (size_t i = start_index, len = c->len; i < len; ++i) {
    if (c->ptr[i]) {
      ereport(mem_aligned_free(c->ptr + i));
    }
  }
}

static size_t readbuf(struct circbuffer const *const c,
                      float const *restrict const *const src,
                      float *restrict const *const dest,
                      size_t const samples) {
  size_t const buffer_size = c->buffer_size;
  size_t const remain = c->remain;
  size_t const readsize = remain >= samples ? samples : remain;
  size_t readcur = c->writecur + buffer_size - remain;
  if (readcur >= buffer_size) {
    readcur -= buffer_size;
  }
  size_t const sz = buffer_size - readcur;
  if (sz >= readsize) {
    for (size_t i = 0, len = c->len; i < len; ++i) {
      float const *restrict const sp = src[i];
      float *restrict const dp = dest[i];
      memcpy(dp, sp + readcur, readsize * sizeof(float));
    }
    return readsize;
  }
  size_t const sz2 = readsize - sz;
  for (size_t i = 0, len = c->len; i < len; ++i) {
    float const *restrict const sp = src[i];
    float *restrict const dp = dest[i];
    memcpy(dp, sp + readcur, sz * sizeof(float));
    memcpy(dp + sz, sp, sz2 * sizeof(float));
  }
  return readsize;
}

static size_t writebuf(struct circbuffer const *const c,
                       float const *restrict const *const src,
                       float *restrict const *const dest,
                       size_t const samples,
                       size_t const offset) {
  size_t const buffer_size = c->buffer_size;
  size_t const writecur = c->writecur;
  size_t const sz = buffer_size - writecur;
  if (sz >= samples) {
    for (size_t i = 0, len = c->len; i < len; ++i) {
      float const *restrict const sp = src[i];
      float *restrict const dp = dest[i];
      memcpy(dp + writecur, sp + offset, samples * sizeof(float));
    }
    size_t cur = writecur + samples;
    if (cur == buffer_size) {
      cur = 0;
    }
    return cur;
  }
  size_t const sz2 = samples - sz;
  for (size_t i = 0, len = c->len; i < len; ++i) {
    float const *restrict const sp = src[i];
    float *restrict const dp = dest[i];
    memcpy(dp + writecur, sp + offset, sz * sizeof(float));
    memcpy(dp, sp + offset + sz, sz2 * sizeof(float));
  }
  return sz2;
}

static size_t fillbuf(struct circbuffer const *const c, float *restrict const *const dest, size_t const samples) {
  size_t const buffer_size = c->buffer_size;
  size_t const writecur = c->writecur;
  size_t const sz = c->buffer_size - writecur;
  if (sz >= samples) {
    for (size_t i = 0, len = c->len; i < len; ++i) {
      float *restrict const dp = dest[i];
      memset(dp + writecur, 0, samples * sizeof(float));
    }
    size_t cur = writecur + samples;
    if (cur == buffer_size) {
      cur = 0;
    }
    return cur;
  }
  size_t const sz2 = samples - sz;
  for (size_t i = 0, len = c->len; i < len; ++i) {
    float *restrict const dp = dest[i];
    memset(dp + writecur, 0, sz * sizeof(float));
    memset(dp, 0, sz2 * sizeof(float));
  }
  return sz2;
}

NODISCARD error circbuffer_create(struct circbuffer **const cp) {
  if (!cp || *cp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(cp, 1, sizeof(struct circbuffer));
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  **cp = (struct circbuffer){0};
  return eok();
}

NODISCARD error circbuffer_destroy(struct circbuffer **const cp) {
  if (!cp || !*cp) {
    return errg(err_invalid_arugment);
  }
  struct circbuffer *const c = *cp;
  release(c, 0);
  ereport(afree(c));
  ereport(mem_free(cp));
  return eok();
}

NODISCARD error circbuffer_set_buffer_size(struct circbuffer *const c, size_t const buffer_size) {
  if (!c || !buffer_size) {
    return errg(err_invalid_arugment);
  }
  if (!c->len) {
    c->buffer_size = buffer_size;
    return eok();
  }
  if (c->remain > buffer_size) {
    return errg(err_not_sufficient_buffer);
  }
  if (!c->remain) {
    c->buffer_size = buffer_size;
    release(c, 0);
    error err = allocate(c, 0);
    if (efailed(err)) {
      err = ethru(err);
    }
    return err;
  }
  struct circbuffer tmp = {.buffer_size = buffer_size};
  error err = circbuffer_set_channels(&tmp, c->len);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  readbuf(c, (float const *restrict const *const)c->ptr, tmp.ptr, c->remain);
  release(c, 0);
  for (size_t i = 0, len = c->len; i < len; ++i) {
    c->ptr[i] = tmp.ptr[i];
    tmp.ptr[i] = NULL;
  }
  ereport(afree(&tmp));
  c->writecur = c->remain;
  c->buffer_size = buffer_size;
  return err;
}

void circbuffer_clear(struct circbuffer *const c) {
  c->remain = 0;
  c->writecur = 0;
}

NODISCARD error circbuffer_set_channels(struct circbuffer *const c, size_t const channels) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  error err = eok();
  size_t const old_channels = c->len;
  if (old_channels == channels) {
    goto cleanup;
  }
  if (old_channels > channels) {
    release(c, channels);
    c->len = channels;
    goto cleanup;
  }
  err = agrow(c, channels);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  for (size_t i = old_channels; i < channels; ++i) {
    c->ptr[i] = NULL;
  }
  c->len = channels;
  if (c->buffer_size) {
    err = allocate(c, old_channels);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  c->remain = 0;
  c->writecur = 0;
cleanup:
  if (efailed(err)) {
    if (c->len != old_channels) {
      release(c, old_channels);
      c->len = old_channels;
    }
  }
  return err;
}

size_t circbuffer_get_channels(struct circbuffer const *const c) { return alen(c); }

size_t circbuffer_get_remain(struct circbuffer const *const c) { return c->remain; }

NODISCARD error circbuffer_write_offset(struct circbuffer *const c,
                                        float const *restrict const *const src,
                                        size_t const samples,
                                        size_t const offset) {
  if (!c || !src) {
    return errg(err_invalid_arugment);
  }
  if (!c->len || !samples) {
    return eok();
  }
  if (c->remain + samples > c->buffer_size) {
    error err = circbuffer_set_buffer_size(c, c->remain + samples);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
  }
  c->writecur = writebuf(c, src, (float *restrict const *const)c->ptr, samples, offset);
  c->remain += samples;
  return eok();
}

NODISCARD error circbuffer_write(struct circbuffer *const c,
                                 float const *restrict const *const src,
                                 size_t const samples) {
  error err = circbuffer_write_offset(c, src, samples, 0);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD error circbuffer_write_silence(struct circbuffer *const c, size_t const samples) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!c->len || !samples) {
    return eok();
  }
  if (c->remain + samples > c->buffer_size) {
    error err = circbuffer_set_buffer_size(c, c->remain + samples);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
  }
  c->writecur = fillbuf(c, (float *restrict const *const)c->ptr, samples);
  c->remain += samples;
  return eok();
}

NODISCARD error circbuffer_read(struct circbuffer *const c,
                                float *restrict const *const dest,
                                size_t const samples,
                                size_t *const written) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!dest) {
    return errg(err_null_pointer);
  }
  if (!c->len || !samples || !c->remain) {
    if (written) {
      *written = 0;
    }
    return eok();
  }
  size_t const readsize = readbuf(c, (float const *restrict const *const)c->ptr, dest, samples);
  c->remain -= readsize;
  if (written) {
    *written = readsize;
  }
  return eok();
}

NODISCARD error circbuffer_discard(struct circbuffer *const c, size_t const samples, size_t *const discarded) {
  if (!c) {
    return errg(err_invalid_arugment);
  }
  if (!c->len || !samples || !c->remain) {
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

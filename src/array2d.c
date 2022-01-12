#include "array2d.h"

NODISCARD error array2d_allocate(struct array2d *const c, size_t const channels, size_t const buffer_size) {
  static size_t const align = 16, block_size = 4;
  if (c->ptr) {
    return errg(err_unexpected);
  }
  size_t const real_buffer_size = (buffer_size + block_size - 1) & ~(block_size - 1);
  error err = mem(&c->ptr, channels, sizeof(float *));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  for (size_t ch = 0; ch < channels; ++ch) {
    c->ptr[ch] = NULL;
    err = mem_aligned_alloc(c->ptr + ch, real_buffer_size, sizeof(float), align);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    memset(c->ptr[ch], 0, real_buffer_size * sizeof(float));
  }
  c->channels = channels;
  c->buffer_size = buffer_size;
cleanup:
  if (efailed(err)) {
    array2d_release(c);
  }
  return err;
}

void array2d_release(struct array2d *const c) {
  if (c->ptr) {
    for (size_t ch = 0, channels = c->channels; ch < channels; ++ch) {
      ereport(mem_aligned_free(c->ptr + ch));
    }
    ereport(mem_free(&c->ptr));
    c->channels = 0;
    c->buffer_size = 0;
  }
}

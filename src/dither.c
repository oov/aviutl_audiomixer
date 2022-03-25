#include "dither.h"

NODISCARD error dither_create(struct dither *const d, size_t const channels) {
  if (!d || !channels) {
    return errg(err_invalid_arugment);
  }
  error err = agrow(d, channels);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  dither_reset(d);
  return eok();
}

NODISCARD error dither_destroy(struct dither *const d) {
  if (!d) {
    return errg(err_invalid_arugment);
  }
  error err = afree(d);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

void dither_reset(struct dither *const d) {
  if (!d) {
    return;
  }
  memset(d->ptr, 0, sizeof(struct dither_state) * d->len);
}

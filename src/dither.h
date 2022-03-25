#pragma once

#include "ovbase.h"

#include <math.h>

struct dither_state {
  float sample;
};

struct dither {
  struct dither_state *ptr;
  size_t len;
  size_t cap;
  uint32_t seed;
};

NODISCARD error dither_create(struct dither *const d, size_t const channels);
NODISCARD error dither_destroy(struct dither *const d);

void dither_reset(struct dither *const d);

static inline float dither_process(float x, struct dither *const d, size_t const channel, float const scale) {
  uint32_t const seed = d->seed;
  float const thr = 0.000000063095734f; // -144db
  if (fabsf(x) > thr) {
    struct dither_state *const ds = d->ptr + channel;
    static float const divider = 1.f / (float)(UINT32_MAX);
    float v = (float)(ov_splitmix32(seed)) * divider - 0.5f;
    x = x * (scale - 1.f) + (v - ds->sample);
    ds->sample = v;
  } else {
    x = x * scale;
  }
  d->seed = ov_splitmix32_next(seed);
  return x;
}

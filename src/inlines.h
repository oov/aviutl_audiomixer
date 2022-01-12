#pragma once

#include <math.h>

static inline int maxi(int const a, int const b) { return a > b ? a : b; }

static inline int fcompare(float x, float y, float tolerance) {
  return (x > y + tolerance) ? 1 : (y > x + tolerance) ? -1 : 0;
}
#define fcmp(x, op, y, tolerance) ((fcompare((x), (y), (tolerance)))op 0)

static inline void swap(float *restrict const **a, float *restrict const **b) {
  float *restrict const *tmp = *a;
  *a = *b;
  *b = tmp;
}

static inline float db_to_amp(float const db) {
  // powf(10.f, db / 20.f)
  // expf(db * logf(10.f) * 0.05f)
  return db <= -144.f ? 0.f : expf(db * 0.1151292546497023f);
}

static inline void clear(float *restrict const *const outputs, size_t const channels, size_t const samples) {
  for (size_t ch = 0; ch < channels; ++ch) {
    memset(outputs[ch], 0, samples * sizeof(float));
  }
}

static inline void mix(float *restrict const *const outputs,
                       float const *restrict const *const inputs,
                       size_t const channels,
                       size_t const samples) {
  for (size_t ch = 0; ch < channels; ++ch) {
    float const *restrict const i = inputs[ch];
    float *restrict const o = outputs[ch];
    for (size_t pos = 0; pos < samples; ++pos) {
      o[pos] += i[pos];
    }
  }
}

static inline void mix_with_gain(float *restrict const *const outputs,
                                 float const *restrict const *const inputs,
                                 float const gain_db,
                                 size_t const channels,
                                 size_t const samples) {
  float const m = db_to_amp(gain_db);
  for (size_t ch = 0; ch < channels; ++ch) {
    float const *restrict const i = inputs[ch];
    float *restrict const o = outputs[ch];
    for (size_t pos = 0; pos < samples; ++pos) {
      o[pos] += i[pos] * m;
    }
  }
}

static inline void gain(float *restrict const *const outputs,
                        float const *restrict const *const inputs,
                        float const gain_db,
                        size_t const channels,
                        size_t const samples) {
  float const g = db_to_amp(gain_db);
  for (size_t ch = 0; ch < channels; ++ch) {
    float const *restrict const i = inputs[ch];
    float *restrict const o = outputs[ch];
    for (size_t pos = 0; pos < samples; ++pos) {
      o[pos] = i[pos] * g;
    }
  }
}

static inline void stereo_pan_and_gain(float *restrict const *const outputs,
                                       float const *restrict const *const inputs,
                                       float const pan,
                                       float const gain_db,
                                       size_t const samples) {
  float const *restrict const i0 = inputs[0];
  float const *restrict const i1 = inputs[1];
  float *restrict const o0 = outputs[0];
  float *restrict const o1 = outputs[1];
  float const g = db_to_amp(gain_db);
  float const p = (pan + 1.f) * 0.5f;
  static float const hpi = 0.5f * 3.14159265358979323846f;
  float const th = hpi * p;
  float r = sinf(th);
  float l = cosf(th);
  l *= g;
  r *= g;
  float const ll = p < 0.5f ? (0.5f + p) : 1.f;
  float const rr = p > 0.5f ? (1.5f - p) : 1.f;
  float const rl = 1.f - ll;
  float const lr = 1.f - rr;

  float s0, s1;
  for (size_t pos = 0; pos < samples; ++pos) {
    s0 = i0[pos];
    s1 = i1[pos];
    o0[pos] = (s0 * ll + s1 * rl) * l;
    o1[pos] = (s0 * lr + s1 * rr) * r;
  }
}

static inline void interleaved_int16_to_float_generic(float *restrict const *const dest,
                                                      int16_t const *restrict const src,
                                                      size_t const channels,
                                                      size_t const samples) {
  static float const m = 1.f / 32768.f;
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dest[ch][i] = (float)(src[i * channels + ch]) * m;
    }
  }
}

static inline void interleaved_int16_to_float_stereo(float *restrict const *const dest,
                                                     int16_t const *restrict const src,
                                                     size_t const samples) {
  int16_t const *restrict sp = src;
  float *restrict const dp0 = dest[0];
  float *restrict const dp1 = dest[1];
  static float const m = 1.f / 32768.f;
  for (size_t i = 0; i < samples; ++i) {
    dp0[i] = (float)(sp[i * 2 + 0]) * m;
    dp1[i] = (float)(sp[i * 2 + 1]) * m;
  }
}

static inline void interleaved_int16_to_float_mono(float *restrict const *const dest,
                                                   int16_t const *restrict const src,
                                                   size_t const samples) {
  int16_t const *restrict sp = src;
  float *restrict const dp0 = dest[0];
  static float const m = 1.f / 32768.f;
  for (size_t i = 0; i < samples; ++i) {
    dp0[i] = (float)(sp[i]) * m;
  }
}

static inline void interleaved_int16_to_float(float *restrict const *const dest,
                                              int16_t const *restrict const src,
                                              size_t const channels,
                                              size_t const samples) {
  switch (channels) {
  case 1:
    interleaved_int16_to_float_mono(dest, src, samples);
    break;
  case 2:
    interleaved_int16_to_float_stereo(dest, src, samples);
    break;
  default:
    interleaved_int16_to_float_generic(dest, src, channels, samples);
    break;
  }
}

static inline float clip_hard(float x) {
  if (x < -1.f) {
    x = -1.f;
  }
  if (x > 1.f) {
    x = 1.f;
  }
  return x;
}

static inline float clip_soft(float x) {
  if (x < -1.f) {
    x = -1.f;
  }
  if (x > 1.f) {
    x = 1.f;
  }
  static float const t = 1.f / 3.f;
  return x - t * x * x * x;
}

static inline void float_to_interleaved_int16_generic(int16_t *restrict const dest,
                                                      float const *restrict const *const src,
                                                      size_t const channels,
                                                      size_t const samples) {
  static float const m = 32767.f;
  for (size_t i = 0; i < samples; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dest[i * channels + ch] = (int16_t)(clip_soft(src[ch][i]) * m);
    }
  }
}

static inline void float_to_interleaved_int16_stereo(int16_t *restrict const dest,
                                                     float const *restrict const *const src,
                                                     size_t const samples) {
  float const *restrict sp0 = src[0];
  float const *restrict sp1 = src[1];
  int16_t *restrict const dp = dest;
  static float const m = 32767.f;
  for (size_t i = 0; i < samples; ++i) {
    dp[i * 2 + 0] = (int16_t)(clip_soft(sp0[i]) * m);
    dp[i * 2 + 1] = (int16_t)(clip_soft(sp1[i]) * m);
  }
}

static inline void float_to_interleaved_int16_mono(int16_t *restrict const dest,
                                                   float const *restrict const *const src,
                                                   size_t const samples) {
  float const *restrict sp0 = src[0];
  int16_t *restrict const dp = dest;
  static float const m = 32767.f;
  for (size_t i = 0; i < samples; ++i) {
    dp[i] = (int16_t)(clip_soft(sp0[i]) * m);
  }
}

static inline void float_to_interleaved_int16(int16_t *restrict const dest,
                                              float const *restrict const *const src,
                                              size_t const channels,
                                              size_t const samples) {
  switch (channels) {
  case 1:
    float_to_interleaved_int16_mono(dest, src, samples);
    break;
  case 2:
    float_to_interleaved_int16_stereo(dest, src, samples);
    break;
  default:
    float_to_interleaved_int16_generic(dest, src, channels, samples);
    break;
  }
}

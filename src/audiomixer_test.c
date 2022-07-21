#include "circbuffer_i16.c"

#include "ovtest.h"

#include <math.h>

static void f(int16_t v) { (void)v; }

__declspec(noinline) static void float_to_interleaved_int16_1(int16_t *restrict const dest,
                                                              float const *restrict const *const src,
                                                              size_t const channels,
                                                              size_t const n,
                                                              size_t const dest_offset,
                                                              size_t const src_offset) {
  static float const t = 1.f / 3.f;
  for (size_t ch = 0; ch < channels; ++ch) {
    float const *restrict sp = src[ch] + src_offset;
    int16_t *restrict const dp = dest + (dest_offset * channels) + ch;
    for (size_t i = 0; i < n; ++i) {
      float x = fmaxf(-1.f, fminf(1.f, sp[i]));
      dp[i * channels] = (int16_t)((x - t * x * x * x) * 32767.f);
    }
  }
}

static void bench_float_to_interleaved_int16_1(void) {
  _Alignas(16) float in[1024] = {0};
  float *inp[1] = {in};
  _Alignas(16) int16_t out[1024] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_1(out, (float const *restrict const *const)inp, 1, 1024, 0, 0);
  }
  f(out[0]);
}

static inline float clip_2(float x) {
  if (x < -1.f) {
    x = -1.f;
  }
  if (x > 1.f) {
    x = 1.f;
  }
  static float const t = 1.f / 3.f;
  return x - t * x * x * x;
}

__declspec(noinline) static void float_to_interleaved_int16_2(int16_t *restrict const dest,
                                                              float const *restrict const *const src,
                                                              size_t const channels,
                                                              size_t const n,
                                                              size_t const dest_offset,
                                                              size_t const src_offset) {
  for (size_t ch = 0; ch < channels; ++ch) {
    float const *restrict sp = src[ch] + src_offset;
    int16_t *restrict const dp = dest + (dest_offset * channels) + ch;
    for (size_t i = 0; i < n; ++i) {
      dp[i * channels] = (int16_t)(clip_2(sp[i]) * 32767.f);
    }
  }
}

static void bench_float_to_interleaved_int16_2(void) {
  _Alignas(16) float in[1024] = {0};
  float *inp[1] = {in};
  _Alignas(16) int16_t out[1024] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_2(out, (float const *restrict const *const)inp, 1, 1024, 0, 0);
  }
  f(out[0]);
}

static void bench_float_to_interleaved_int16_stereo_1(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_2(out, (float const *restrict const *const)inp, 2, 1024, 0, 0);
  }
  f(out[0]);
}

__declspec(noinline) static void float_to_interleaved_int16_stereo_2(int16_t *restrict const dest,
                                                                     float const *restrict const *const src,
                                                                     size_t const n,
                                                                     size_t const dest_offset,
                                                                     size_t const src_offset) {
  float const *restrict sp0 = src[0] + src_offset;
  float const *restrict sp1 = src[1] + src_offset;
  int16_t *restrict const dp = dest + dest_offset * 2;
  for (size_t si = 0, di = 0; si < n; ++si, di += 2) {
    dp[di + 0] = (int16_t)(clip_2(sp0[si]) * 32767.f);
    dp[di + 1] = (int16_t)(clip_2(sp1[si]) * 32767.f);
  }
}

static void bench_float_to_interleaved_int16_stereo_2(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_stereo_2(out, (float const *restrict const *const)inp, 1024, 0, 0);
  }
  f(out[0]);
}

__declspec(noinline) static void float_to_interleaved_int16_stereo_3(int16_t *restrict const dest,
                                                                     float const *restrict const *const src,
                                                                     size_t const n,
                                                                     size_t const dest_offset,
                                                                     size_t const src_offset) {
  float const *restrict sp0 = src[0] + src_offset;
  float const *restrict sp1 = src[1] + src_offset;
  int16_t *restrict const dp = dest + (dest_offset * 2);
  for (size_t i = 0; i < n; ++i) {
    dp[i * 2 + 0] = (int16_t)(clip_2(sp0[i]) * 32767.f);
    dp[i * 2 + 1] = (int16_t)(clip_2(sp1[i]) * 32767.f);
  }
}

static void bench_float_to_interleaved_int16_stereo_3(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_stereo_3(out, (float const *restrict const *const)inp, 1024, 0, 0);
  }
  f(out[0]);
}

static void bench_float_to_interleaved_int16_generic_1(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_2(out, (float const *restrict const *const)inp, 2, 1024, 0, 0);
  }
  f(out[0]);
}

__declspec(noinline) static void float_to_interleaved_int16_generic_2(int16_t *restrict const dest,
                                                                      float const *restrict const *const src,
                                                                      size_t const channels,
                                                                      size_t const n,
                                                                      size_t const dest_offset,
                                                                      size_t const src_offset) {
  int16_t *restrict const dp = dest + dest_offset * channels;
  for (size_t si = 0, di = 0; si < n; ++si, di += channels) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dp[di + ch] = (int16_t)(clip_2(src[ch][si + src_offset]) * 32767.f);
    }
  }
}

static void bench_float_to_interleaved_int16_generic_2(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_generic_2(out, (float const *restrict const *const)inp, 2, 1024, 0, 0);
  }
  f(out[0]);
}

__declspec(noinline) static void float_to_interleaved_int16_generic_3(int16_t *restrict const dest,
                                                                      float const *restrict const *const src,
                                                                      size_t const channels,
                                                                      size_t const n,
                                                                      size_t const dest_offset,
                                                                      size_t const src_offset) {
  int16_t *restrict const dp = dest + dest_offset * channels;
  for (size_t i = 0; i < n; ++i) {
    for (size_t ch = 0; ch < channels; ++ch) {
      dp[i * channels + ch] = (int16_t)(clip_2(src[ch][i + src_offset]) * 32767.f);
    }
  }
}

static void bench_float_to_interleaved_int16_generic_3(void) {
  _Alignas(16) float in0[1024] = {0};
  _Alignas(16) float in1[1024] = {0};
  float *inp[2] = {in0, in1};
  _Alignas(16) int16_t out[2048] = {0};
  uint32_t t = (uint32_t)get_global_hint();
  for (size_t i = 0; i < 1024; ++i) {
    inp[0][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
    inp[1][i] = (float)(ov_splitmix32(t));
    t = ov_splitmix32_next(t);
  }
  for (int i = 0; i < 10000; ++i) {
    float_to_interleaved_int16_generic_3(out, (float const *restrict const *const)inp, 2, 1024, 0, 0);
  }
  f(out[0]);
}

TEST_LIST = {
    {"bench_float_to_interleaved_int16_1", bench_float_to_interleaved_int16_1},
    {"bench_float_to_interleaved_int16_2", bench_float_to_interleaved_int16_2},
    {"bench_float_to_interleaved_int16_stereo_1", bench_float_to_interleaved_int16_stereo_1},
    {"bench_float_to_interleaved_int16_stereo_2", bench_float_to_interleaved_int16_stereo_2},
    {"bench_float_to_interleaved_int16_stereo_3", bench_float_to_interleaved_int16_stereo_3},
    {"bench_float_to_interleaved_int16_generic_1", bench_float_to_interleaved_int16_generic_1},
    {"bench_float_to_interleaved_int16_generic_2", bench_float_to_interleaved_int16_generic_2},
    {"bench_float_to_interleaved_int16_generic_3", bench_float_to_interleaved_int16_generic_3},
    {NULL, NULL},
};

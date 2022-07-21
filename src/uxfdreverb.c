// This code is based on UXFDReverb.
// https://github.com/khoin/UXFDReverb
//
// In jurisdictions that recognize copyright laws, this software is to
// be released into the public domain.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// THE AUTHOR(S) SHALL NOT BE LIABLE FOR ANYTHING, ARISING FROM, OR IN
// CONNECTION WITH THE SOFTWARE OR THE DISTRIBUTION OF THE SOFTWARE.
#include "uxfdreverb.h"

#include <math.h>

#include "inlines.h"

enum {
  num_delays = 12,
  num_taps = 14,
};

struct delay {
  float *ptr;
  size_t len;
  size_t writecur;
  size_t readcur;
};

struct uxfdreverb {
  float pre_delay;  // 0 - 1 (0s - 0.25s)
  float band_width; // 0 - 1
  float diffuse;    // 0 - 1
  float decay;      // 0 - 1
  float damping;    // 0 - 1
  float excursion;  // 0 - 32
  float wet;        // 0 - 1
  float dry;        // 0 - 1

  struct delay delays[num_delays];
  size_t taps[num_taps];
  float *pre_delay_ptr;
  size_t pre_delay_len;
  size_t pre_delay_writecur;
  float lp1, lp2, lp3, curtime;
  float sample_rate;
  size_t channels;
  bool sample_rate_changed : 1;
  bool need_parameter_update : 1;
};

static size_t pre_delay_max(struct uxfdreverb const *const r) { return (size_t)(r->sample_rate) / 4; }

NODISCARD error uxfdreverb_create(struct uxfdreverb **const rp) {
  if (!rp || *rp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(rp, 1, sizeof(struct uxfdreverb));
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  **rp = (struct uxfdreverb){
      .pre_delay = 0.f,
      .band_width = 0.9999f,
      .diffuse = 1.f,
      .decay = 0.5f,
      .damping = 0.005f,
      .excursion = 16.f,
      .wet = 0.3f,
      .dry = 0.6f,
      .sample_rate = 48000.f,
      .channels = 2,
      .sample_rate_changed = true,
      .need_parameter_update = true,
  };
  return eok();
}

NODISCARD error uxfdreverb_destroy(struct uxfdreverb **const rp) {
  if (!rp || !*rp) {
    return errg(err_invalid_arugment);
  }
  struct uxfdreverb *const r = *rp;
  for (size_t i = 0; i < num_delays; ++i) {
    if (r->delays[i].ptr) {
      ereport(mem_aligned_free(&r->delays[i].ptr));
    }
  }
  if (r->pre_delay_ptr) {
    ereport(mem_aligned_free(&r->pre_delay_ptr));
  }
  ereport(mem_free(rp));
  return eok();
}

void uxfdreverb_set_format(struct uxfdreverb *const r, float const sample_rate, size_t const channels) {
  bool sample_rate_is_same = fcmp(r->sample_rate, ==, sample_rate, 1e-12f);
  if (sample_rate_is_same && r->channels == channels) {
    return;
  }
  r->sample_rate = sample_rate;
  r->channels = channels;
  r->sample_rate_changed = !sample_rate_is_same;
  r->need_parameter_update = true;
}

void uxfdreverb_set_pre_delay(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->pre_delay, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->pre_delay = v;
}

void uxfdreverb_set_band_width(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->band_width, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->band_width = v;
}

void uxfdreverb_set_diffuse(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->diffuse, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->diffuse = v;
}

void uxfdreverb_set_decay(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->decay, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->decay = v;
}

void uxfdreverb_set_damping(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->damping, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->damping = v;
}

void uxfdreverb_set_excursion(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->excursion, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->excursion = v;
}

void uxfdreverb_set_wet(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->wet, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->wet = v;
}

void uxfdreverb_set_dry(struct uxfdreverb *const r, float const v) {
  if (fcmp(r->dry, ==, v, 1e-12f)) {
    return;
  }
  r->need_parameter_update = true;
  r->dry = v;
}

NODISCARD static error recreate_delays(struct uxfdreverb *const r) {
  static float const delay_lengths[num_delays] = {
      0.004771345f,
      0.003595309f,
      0.012734787f,
      0.009307483f,
      0.022579886f,
      0.149625349f,
      0.060481839f,
      0.1249958f,
      0.030509727f,
      0.141695508f,
      0.089244313f,
      0.106280031f,
  };
  float const sample_rate = r->sample_rate;
  float *ptrs[num_delays] = {0};
  size_t lens[num_delays] = {0};
  error err = eok();
  for (size_t i = 0; i < num_delays; ++i) {
    lens[i] = (size_t)(roundf(delay_lengths[i] * sample_rate));
    err = mem_aligned_alloc(&ptrs[i], lens[i], sizeof(float), 16);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
  }
  for (size_t i = 0; i < num_delays; ++i) {
    if (r->delays[i].ptr) {
      ereport(mem_aligned_free(&r->delays[i].ptr));
    }
    r->delays[i] = (struct delay){
        .ptr = ptrs[i],
        .len = lens[i],
    };
  }
cleanup:
  if (efailed(err)) {
    for (size_t i = 0; i < num_delays; ++i) {
      if (ptrs[i]) {
        ereport(mem_aligned_free(&ptrs[i]));
      }
    }
  }
  return err;
}

static void update_taps(struct uxfdreverb *const r) {
  static float const taps[num_taps] = {
      0.008937872f,
      0.099929438f,
      0.064278754f,
      0.067067639f,
      0.066866033f,
      0.006283391f,
      0.035818689f,
      0.011861161f,
      0.121870905f,
      0.041262054f,
      0.08981553f,
      0.070931756f,
      0.011256342f,
      0.004065724f,
  };
  float const sample_rate = r->sample_rate;
  for (size_t i = 0; i < num_taps; ++i) {
    r->taps[i] = (size_t)(roundf(taps[i] * sample_rate));
  }
}

NODISCARD static error recreate_pre_delay(struct uxfdreverb *const r) {
  size_t const pre_delay_len = pre_delay_max(r) * 2;
  float *pre_delay_ptr = NULL;
  error err = mem_aligned_alloc(&pre_delay_ptr, pre_delay_len, sizeof(float), 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (r->pre_delay_ptr) {
    ereport(mem_aligned_free(&r->pre_delay_ptr));
  }
  r->pre_delay_ptr = pre_delay_ptr;
  r->pre_delay_len = pre_delay_len;
  r->pre_delay_writecur = 0;
cleanup:
  return err;
}

static inline float clamp(float x, float const mn, float const mx) {
  if (x < mn) {
    x = mn;
  }
  if (x > mx) {
    x = mx;
  }
  return x;
}

NODISCARD static error update_internal_parameter(struct uxfdreverb *const r) {
  error err = eok();
  if (r->sample_rate_changed) {
    err = recreate_delays(r);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    err = recreate_pre_delay(r);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    update_taps(r);
    uxfdreverb_clear(r);
  }
  r->pre_delay = clamp(r->pre_delay, 0.f, 1.f);
  r->band_width = clamp(r->band_width, 0.f, 1.f);
  r->diffuse = clamp(r->diffuse, 0.f, 1.f);
  r->decay = clamp(r->decay, 0.f, 1.f);
  r->damping = clamp(r->damping, 0.f, 1.f);
  r->excursion = clamp(r->excursion, 0.f, 32.f);
  r->wet = clamp(r->wet, 0.f, 1.f);
  r->dry = clamp(r->dry, 0.f, 1.f);
cleanup:
  return err;
}

NODISCARD error uxfdreverb_update_internal_parameter(struct uxfdreverb *const r, bool *const updated) {
  if (!r->need_parameter_update) {
    if (updated) {
      *updated = false;
    }
    return eok();
  }
  error err = update_internal_parameter(r);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  r->sample_rate_changed = false;
  r->need_parameter_update = false;
  if (updated) {
    *updated = true;
  }
  return eok();
}

static inline void
to_mono(float *restrict const o, float const *restrict const i0, float const *restrict const i1, size_t const samples) {
  for (size_t i = 0; i < samples; ++i) {
    o[i] = (i0[i] + i1[i]) * 0.5f;
  }
}

static inline void write_pre_delay(float *restrict const o,
                                   size_t const olen,
                                   size_t const cur,
                                   float const *restrict const i0,
                                   float const *restrict const i1,
                                   size_t const samples) {
  size_t const sz = olen - cur;
  if (sz >= samples) {
    to_mono(o + cur, i0, i1, samples);
  } else {
    to_mono(o + cur, i0, i1, sz);
    to_mono(o, i0 + sz, i1 + sz, samples - sz);
  }
}

static inline void advance_all_cursor(struct delay *restrict const d) {
#if 0
  for (size_t i = 0; i < num_delays; ++i) {
    d[i].readcur = (d[i].readcur + 1) % d[i].len;
    d[i].writecur = (d[i].writecur + 1) % d[i].len;
  }
#else
  _Alignas(16) size_t cur[num_delays * 2];
  _Alignas(16) size_t len[num_delays * 2];
  for (size_t i = 0; i < num_delays; ++i) {
    cur[i] = d[i].readcur;
    len[i] = d[i].len;
    cur[i + num_delays] = d[i].writecur;
    len[i + num_delays] = d[i].len;
  }
  for (size_t i = 0; i < num_delays * 2; ++i) {
    ++cur[i];
    if (cur[i] == len[i]) {
      cur[i] = 0;
    }
  }
  for (size_t i = 0; i < num_delays; ++i) {
    d[i].readcur = cur[i];
    d[i].writecur = cur[i + num_delays];
  }
#endif
}

static inline void write_delay(struct delay *restrict const d, float const v) { d->ptr[d->writecur] = v; }

static inline float read_delay(struct delay const *restrict const d) { return d->ptr[d->readcur]; }

static inline float read_delay_at(struct delay const *restrict const d, size_t const idx) {
  size_t const len = d->len;
  size_t cur = d->readcur + idx;
  if (cur >= len) {
    cur -= len;
  }
  return d->ptr[cur];
}

static float read_delay_at_approx(struct delay const *restrict const d, float const idx) {
  size_t const iidx = (size_t)idx;
  float const frac = idx - (float)iidx;
  float const *const ptr = d->ptr;
  size_t const len = d->len;
  size_t readcur = d->readcur + iidx;
  if (readcur >= len) {
    readcur -= len;
  }
  size_t next_readcur = readcur + 1;
  if (next_readcur >= len) {
    next_readcur -= len;
  }
  float const x = ptr[readcur];
  float const y = ptr[next_readcur];
  return x + frac * (y - x);
}

static inline float read_pre_delay(struct delay const *restrict const d) { return d->ptr[d->writecur]; }

void uxfdreverb_process(struct uxfdreverb *const r,
                        float const *restrict const *const inputs,
                        float *restrict const *const outputs,
                        size_t const samples) {
  static float const pi = 3.14159265358979323846264338327950288f;
  size_t const pd = (size_t)(r->pre_delay * r->sample_rate * 0.25f);
  float const bw = r->band_width;
  float const fi = r->diffuse * 0.75f;
  float const si = r->diffuse * 0.625f;
  float const dc = r->decay;
  float const ft = r->diffuse * 0.76f;
  float const st = clamp(dc + 0.15f, 0.25f, 0.5f);
  float const dp = r->damping;
  float const ex = r->excursion;
  float const we = r->wet * 0.6f; // lo and ro are both multiplied by 0.6 anyways
  float const dr = r->dry;

  float const timestep = 1.f / r->sample_rate;

  float *restrict const pre_delay_ptr = r->pre_delay_ptr;
  size_t const pre_delay_len = r->pre_delay_len;
  size_t const block_size = pre_delay_max(r);

  struct delay *const delays = r->delays;
  size_t const *const taps = r->taps;

  float lp1 = r->lp1;
  float lp2 = r->lp2;
  float lp3 = r->lp3;
  float curtime = r->curtime;
  size_t pre_delay_writecur = r->pre_delay_writecur;
  size_t pre_delay_readcur = r->pre_delay_writecur + pre_delay_len - pd;
  if (pre_delay_readcur >= pre_delay_len) {
    pre_delay_readcur -= pre_delay_len;
  }

  float const *restrict i0 = inputs[0];
  float const *restrict i1 = inputs[1];
  float *restrict o0 = outputs[0];
  float *restrict o1 = outputs[1];

  float lo, ro, split, excursion;
  size_t remain = samples, block, i;
  while (remain) {
    block = remain < block_size ? remain : block_size;
    write_pre_delay(pre_delay_ptr, pre_delay_len, pre_delay_writecur, i0, i1, block);
    for (i = 0; i < block; ++i) {
      lo = 0.f;
      ro = 0.f;
      lp1 = pre_delay_ptr[pre_delay_readcur] * bw + (1 - bw) * lp1;

      // Please note: The groupings and formatting below does not bear any useful information about
      //              the topology of the network. I just want orderly looking text.

      // pre
      write_delay(delays + 0, lp1 - fi * read_delay(delays + 0));
      write_delay(delays + 1, fi * (read_pre_delay(delays + 0) - read_delay(delays + 1)) + read_delay(delays + 0));
      write_delay(delays + 2, fi * read_pre_delay(delays + 1) + read_delay(delays + 1) - si * read_delay(delays + 2));
      write_delay(delays + 3, si * (read_pre_delay(delays + 2) - read_delay(delays + 3)) + read_delay(delays + 2));

      split = si * read_pre_delay(delays + 3) + read_delay(delays + 3);

      // 1Hz (footnote 14, pp. 665)
      excursion = ex * (1.f + cosf((curtime + (timestep * (float)i)) * pi * 2.f));

      // left
      write_delay(delays + 4,
                  split + dc * read_delay(delays + 11) +
                      ft * read_delay_at_approx(delays + 4, excursion)); // tank diffuse 1
      write_delay(delays + 5,
                  read_delay_at_approx(delays + 4, excursion) - ft * read_pre_delay(delays + 4)); // long delay 1
      lp2 = (1 - dp) * read_delay(delays + 5) + dp * lp2;                                         // damp 1
      write_delay(delays + 6, dc * lp2 - st * read_delay(delays + 6));                            // tank diffuse 2
      write_delay(delays + 7, read_delay(delays + 6) + st * read_pre_delay(delays + 6));          // long delay 2

      // right
      write_delay(delays + 8,
                  split + dc * read_delay(delays + 7) +
                      ft * read_delay_at_approx(delays + 8, excursion)); // tank diffuse 3
      write_delay(delays + 9,
                  read_delay_at_approx(delays + 8, excursion) - ft * read_pre_delay(delays + 8)); // long delay 3
      lp3 = (1 - dp) * read_delay(delays + 9) + dp * lp3;                                         // damper 2
      write_delay(delays + 10, dc * lp3 - st * read_delay(delays + 10));                          // tank diffuse 4
      write_delay(delays + 11, read_delay(delays + 10) + st * read_pre_delay(delays + 10));       // long delay 4

      lo = read_delay_at(delays + 9, taps[0]) + read_delay_at(delays + 9, taps[1]) -
           read_delay_at(delays + 10, taps[2]) + read_delay_at(delays + 11, taps[3]) -
           read_delay_at(delays + 5, taps[4]) - read_delay_at(delays + 6, taps[5]) - read_delay_at(delays + 7, taps[6]);

      ro = read_delay_at(delays + 5, taps[7]) + read_delay_at(delays + 5, taps[8]) -
           read_delay_at(delays + 6, taps[9]) + read_delay_at(delays + 7, taps[10]) -
           read_delay_at(delays + 9, taps[11]) - read_delay_at(delays + 10, taps[12]) -
           read_delay_at(delays + 11, taps[13]);

      // write
      o0[i] = i0[i] * dr + lo * we;
      o1[i] = i1[i] * dr + ro * we;

      advance_all_cursor(delays);
      if (++pre_delay_readcur == pre_delay_len) {
        pre_delay_readcur = 0;
      }
    }
    i0 += block;
    i1 += block;
    o0 += block;
    o1 += block;
    curtime += (float)(block)*timestep;
    pre_delay_writecur += block;
    if (pre_delay_writecur >= pre_delay_len) {
      pre_delay_writecur -= pre_delay_len;
    }
    remain -= block;
  }
  r->lp1 = lp1;
  r->lp2 = lp2;
  r->lp3 = lp3;
  r->curtime = curtime;
  r->pre_delay_writecur = pre_delay_writecur;
}

void uxfdreverb_clear(struct uxfdreverb *const r) {
  r->lp1 = 0.f;
  r->lp2 = 0.f;
  r->lp3 = 0.f;
  r->curtime = 0.f;
  struct delay *restrict const d = r->delays;
  for (size_t i = 0; i < num_delays; ++i) {
    memset(d[i].ptr, 0, d[i].len * sizeof(float));
    d[i].writecur = d[i].len - 1;
    d[i].readcur = 0;
  }
  memset(r->pre_delay_ptr, 0, r->pre_delay_len * sizeof(float));
  r->pre_delay_writecur = 0;
}

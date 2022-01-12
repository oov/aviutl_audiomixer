// This code is based on mdaDynamics.cpp.
// https://sourceforge.net/projects/mda-vst/
//
// mda VST plug-ins
//
// Copyright (c) 2008 Paul Kellett
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include "dynamics.h"

#include "ovnum.h"
#include "ovutil/str.h"

#include <math.h>

#include "inlines.h"

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct dynamics {
  float thresh;
  float ratio;
  float output;
  float attack;
  float release;
  float limiter;
  float gate_thresh;
  float gate_attack;
  float gate_decay;
  float fx_mix;

  float thr, rat, env, env2, att, rel, trim, lthr, xthr, xrat, dry;
  float genv, gatt, irel;

  float sample_rate;
  size_t channels;
  bool use_gate_limiter;
  bool need_parameter_update;
};

void dynamics_set_format(struct dynamics *const d, float const sample_rate, size_t const channels) {
  if (fcmp(d->sample_rate, ==, sample_rate, 1e-12f) && d->channels == channels) {
    return;
  }
  d->need_parameter_update = true;
  d->sample_rate = sample_rate;
  d->channels = channels;
}

void dynamics_set_thresh(struct dynamics *const d, float const v) {
  if (fcmp(d->thresh, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->thresh = v;
}

float dynamics_get_ratio(struct dynamics const *const d) { return d->ratio; }

void dynamics_set_ratio(struct dynamics *const d, float const v) {
  if (fcmp(d->ratio, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->ratio = v;
}

void dynamics_set_output(struct dynamics *const d, float const v) {
  if (fcmp(d->output, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->output = v;
}

void dynamics_set_attack(struct dynamics *const d, float const v) {
  if (fcmp(d->attack, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->attack = v;
}

void dynamics_set_release(struct dynamics *const d, float const v) {
  if (fcmp(d->release, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->release = v;
}

void dynamics_set_limiter(struct dynamics *const d, float const v) {
  if (fcmp(d->limiter, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->limiter = v;
}

void dynamics_set_gate_thresh(struct dynamics *const d, float const v) {
  if (fcmp(d->gate_thresh, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->gate_thresh = v;
}

void dynamics_set_gate_attack(struct dynamics *const d, float const v) {
  if (fcmp(d->gate_attack, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->gate_attack = v;
}

void dynamics_set_gate_decay(struct dynamics *const d, float const v) {
  if (fcmp(d->gate_decay, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->gate_decay = v;
}

void dynamics_set_fx_mix(struct dynamics *const d, float const v) {
  if (fcmp(d->fx_mix, ==, v, 1e-12f)) {
    return;
  }
  d->need_parameter_update = true;
  d->fx_mix = v;
}

static void update_internal_parameter(struct dynamics *const d) {
  d->use_gate_limiter = false;
  d->thr = powf(10.f, 2.f * d->thresh - 2.f);
  d->rat = 2.5f * d->ratio - 0.5f;
  if (d->rat > 1.f) {
    d->rat = 1.f + 16.f * (d->rat - 1.f) * (d->rat - 1.f);
    d->use_gate_limiter = true;
  }
  if (d->rat < 0.f) {
    d->rat = 0.6f * d->rat;
    d->use_gate_limiter = true;
  }
  d->trim = powf(10.f, 2.f * d->output); // was  - 1.f);
  d->att = powf(10.f, -0.002f - 2.f * d->attack);
  d->rel = powf(10.f, -2.f - 3.f * d->release);

  if (d->limiter > 0.98f) {
    d->lthr = 0.f; // limiter
  } else {
    d->lthr = 0.99f * powf(10.f, floorf(30.f * d->limiter - 20.f) / 20.f);
    d->use_gate_limiter = true;
  }

  if (d->gate_thresh < 0.02f) {
    d->xthr = 0.f; // expander
  } else {
    d->xthr = powf(10.f, 3.f * d->gate_thresh - 3.f);
    d->use_gate_limiter = true;
  }
  d->xrat = 1.f - powf(10.f, -2.f - 3.3f * d->gate_decay);
  d->irel = powf(10.f, -2.f / d->sample_rate);
  d->gatt = powf(10.f, -0.002f - 3.f * d->gate_attack);

  if (d->rat < 0.f && d->thr < 0.1f) {
    d->rat *= d->thr * 15.f;
  }

  d->dry = 1.0f - d->fx_mix;
  d->trim *= d->fx_mix; // fx mix
}

void dynamics_update_internal_parameter(struct dynamics *const d, bool *const updated) {
  if (!d->need_parameter_update) {
    if (updated) {
      *updated = false;
    }
    return;
  }
  update_internal_parameter(d);
  d->need_parameter_update = false;
  if (updated) {
    *updated = true;
  }
}

static void write_str(NATIVE_CHAR *dest, NATIVE_CHAR const *src) {
  for (; *src != NSTR('\0'); ++src, ++dest) {
    *dest = *src;
  }
  *dest = NSTR('\0');
}

void dynamics_get_attack_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(-301030.1f / (d->sample_rate * log10f(1.f - d->att)));
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

float dynamics_get_attack_duration(struct dynamics const *const d) {
  return floorf(-301030.1f / (d->sample_rate * log10f(1.f - d->att))) / 1000000.f;
}

void dynamics_get_fx_mix_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(100.f * d->fx_mix);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_gate_attack_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(-301030.1f / (d->sample_rate * log10f(1.f - d->gatt)));
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_gate_decay_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(-1806.f / (d->sample_rate * log10f(d->xrat)));
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_gate_threshold_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  if (d->xthr == 0.f) {
    write_str(dest, NSTR("off"));
    return;
  }
  int64_t const v = (int64_t)(60.f * d->gate_thresh - 60.f);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_output_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(40.f * d->output);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_limiter_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  if (d->lthr == 0.f) {
    write_str(dest, NSTR("off"));
    return;
  }
  int64_t const v = (int64_t)(30.f * d->limiter - 20.f);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void dynamics_get_ratio_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  float f = 0.f;
  if (d->ratio > 0.58f) {
    if (d->ratio < 0.62f) {
      write_str(dest, NSTR("oo"));
      return;
    }
    f = -d->rat;
  } else {
    if (d->ratio < 0.2f) {
      f = 0.5f + 2.5f * d->ratio;
    } else {
      f = 1.f / (1.f - d->rat);
    }
  }
  NATIVE_CHAR tmp[64];
  write_str(dest, ov_ftoa((double)f, 2, NSTR('.'), tmp));
}

void dynamics_get_release_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(-301.0301f / (d->sample_rate * log10f(1.f - d->rel)));
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

float dynamics_get_release_duration(struct dynamics const *const d) {
  return floorf(-301.0301f / (d->sample_rate * log10f(1.f - d->rel))) / 1000.f;
}

void dynamics_get_threshold_str(struct dynamics const *const d, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(40.f * d->thresh - 40.f);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

NODISCARD error dynamics_create(struct dynamics **const dp) {
  if (!dp || *dp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(dp, 1, sizeof(struct dynamics));
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  **dp = (struct dynamics){
      .thresh = 0.60f,
      .ratio = 0.40f,
      .output = 0.10f,
      .attack = 0.18f,
      .release = 0.55f,
      .limiter = 1.00f,
      .gate_thresh = 0.00f,
      .gate_attack = 0.10f,
      .gate_decay = 0.50f,
      .fx_mix = 1.00f,
      .env = 0.f,
      .env2 = 0.f,
      .genv = 0.f,
      .sample_rate = 48000.f,
      .channels = 2,
      .need_parameter_update = true,
  };
  return eok();
}

NODISCARD error dynamics_destroy(struct dynamics **const dp) {
  if (!dp || !*dp) {
    return errg(err_invalid_arugment);
  }
  ereport(mem_free(dp));
  return eok();
}

void dynamics_clear(struct dynamics *const d) {
  d->env = 0;
  d->env2 = 0;
  d->genv = 0;
}

static void process_mono(struct dynamics *const d,
                         float const *restrict const *const inputs,
                         float *restrict const *const outputs,
                         size_t const samples) {
  float const ra = d->rat, xra = d->xrat, re = (1.f - d->rel), at = d->att, ga = d->gatt;
  float const tr = d->trim, th = d->thr, lth = d->use_gate_limiter && d->lthr == 0.f ? 1000.f : d->lthr, xth = d->xthr,
              y = d->dry;
  float const *restrict in1 = inputs[0];
  float *restrict out1 = outputs[0];
  float a, i, g, e = d->env, e2 = d->env2, ge = d->genv;

  if (d->use_gate_limiter) { // comp/gate/lim
    for (size_t pos = 0; pos < samples; ++pos) {
      a = in1[pos];
      i = fabsf(a);

      e = (i > e) ? e + at * (i - e) : e * re;
      e2 = (i > e) ? i : e2 * re; // ir;

      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr;

      if (g < 0.f) {
        g = 0.f;
      }
      if (g * e2 > lth) {
        g = lth / e2; // limit
      }

      ge = (e > xth) ? ge + ga - ga * ge : ge * xra; // gate

      out1[pos] = a * (g * ge + y);
    }
  } else { // compressor only
    for (size_t pos = 0; pos < samples; ++pos) {
      a = in1[pos];
      i = fabsf(a);

      e = (i > e) ? e + at * (i - e) : e * re;                // envelope
      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr; // gain

      out1[pos] = a * (g + y); // vca
    }
  }
  d->env = (e < 1.e-10f) ? 0.f : e;
  d->env2 = (e2 < 1.e-10f) ? 0.f : e2;
  d->genv = (ge < 1.e-10f) ? 0.f : ge;
}

static void process_stereo(struct dynamics *const d,
                           float const *restrict const *const inputs,
                           float *restrict const *const outputs,
                           size_t const samples) {
  float const ra = d->rat, xra = d->xrat, re = (1.f - d->rel), at = d->att, ga = d->gatt;
  float const tr = d->trim, th = d->thr, lth = d->use_gate_limiter && d->lthr == 0.f ? 1000.f : d->lthr, xth = d->xthr,
              y = d->dry;
  float const *restrict in1 = inputs[0];
  float const *restrict in2 = inputs[1];
  float *restrict out1 = outputs[0];
  float *restrict out2 = outputs[1];
  float a, b, i, g, e = d->env, e2 = d->env2, ge = d->genv;

  if (d->use_gate_limiter) { // comp/gate/lim
    for (size_t pos = 0; pos < samples; ++pos) {
      a = in1[pos];
      b = in2[pos];
      i = fmaxf(fabsf(a), fabsf(b));

      e = (i > e) ? e + at * (i - e) : e * re;
      e2 = (i > e) ? i : e2 * re; // ir;

      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr;

      if (g < 0.f) {
        g = 0.f;
      }
      if (g * e2 > lth) {
        g = lth / e2; // limit
      }

      ge = (e > xth) ? ge + ga - ga * ge : ge * xra; // gate

      i = g * ge + y;
      out1[pos] = a * i;
      out2[pos] = b * i;
    }
  } else { // compressor only
    for (size_t pos = 0; pos < samples; ++pos) {
      a = in1[pos];
      b = in2[pos];
      i = fmaxf(fabsf(a), fabsf(b));

      e = (i > e) ? e + at * (i - e) : e * re;                // envelope
      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr; // gain

      i = g + y; // vca
      out1[pos] = a * i;
      out2[pos] = b * i;
    }
  }
  d->env = (e < 1.e-10f) ? 0.f : e;
  d->env2 = (e2 < 1.e-10f) ? 0.f : e2;
  d->genv = (ge < 1.e-10f) ? 0.f : ge;
}

static void process_generic(struct dynamics *const d,
                            float const *restrict const *const inputs,
                            float *restrict const *const outputs,
                            size_t const samples) {
  size_t ch;
  size_t const chs = d->channels;
  float const ra = d->rat, xra = d->xrat, re = (1.f - d->rel), at = d->att, ga = d->gatt;
  float const tr = d->trim, th = d->thr, lth = d->use_gate_limiter && d->lthr == 0.f ? 1000.f : d->lthr, xth = d->xthr,
              y = d->dry;
  float i, g, e = d->env, e2 = d->env2, ge = d->genv;

  if (d->use_gate_limiter) { // comp/gate/lim
    for (size_t pos = 0; pos < samples; ++pos) {
      i = 0.f; // get peak level
      for (ch = 0; ch < chs; ++ch) {
        i = fmaxf(i, fabsf(inputs[ch][pos]));
      }

      e = (i > e) ? e + at * (i - e) : e * re;
      e2 = (i > e) ? i : e2 * re; // ir;

      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr;

      if (g < 0.f) {
        g = 0.f;
      }
      if (g * e2 > lth) {
        g = lth / e2; // limit
      }

      ge = (e > xth) ? ge + ga - ga * ge : ge * xra; // gate

      i = g * ge + y;
      for (ch = 0; ch < chs; ++ch) {
        outputs[ch][pos] = inputs[ch][pos] * i;
      }
    }
  } else { // compressor only
    for (size_t pos = 0; pos < samples; ++pos) {
      i = 0.f; // get peak level
      for (ch = 0; ch < chs; ++ch) {
        i = fmaxf(i, fabsf(inputs[ch][pos]));
      }

      e = (i > e) ? e + at * (i - e) : e * re;                // envelope
      g = (e > th) ? tr / (1.f + ra * ((e / th) - 1.f)) : tr; // gain

      i = g + y; // vca
      for (ch = 0; ch < chs; ++ch) {
        outputs[ch][pos] = inputs[ch][pos] * i;
      }
    }
  }
  d->env = (e < 1.e-10f) ? 0.f : e;
  d->env2 = (e2 < 1.e-10f) ? 0.f : e2;
  d->genv = (ge < 1.e-10f) ? 0.f : ge;
}

void dynamics_process(struct dynamics *const d,
                      float const *restrict const *const inputs,
                      float *restrict const *const outputs,
                      size_t const samples) {
  switch (d->channels) {
  case 1:
    process_mono(d, inputs, outputs, samples);
    break;
  case 2:
    process_stereo(d, inputs, outputs, samples);
    break;
  default:
    process_generic(d, inputs, outputs, samples);
    break;
  }
}

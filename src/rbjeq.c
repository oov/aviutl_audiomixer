#include "rbjeq.h"

#include "ovnum.h"
#include "ovutil/str.h"

#include <math.h>

#include "inlines.h"

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct channel {
  float in0, in1, out0, out1;
};

struct channels {
  struct channel *ptr;
  size_t len;
  size_t cap;
};

struct rbjeq {
  float b0a0, b1a0, b2a0, a1a0, a2a0;
  float sample_rate, frequency, q, gain;
  struct channels buffers;
  int filter_type;
  size_t channels;
  bool need_parameter_update;
};

NODISCARD error rbjeq_create(struct rbjeq **const eqp) {
  if (!eqp || *eqp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(eqp, 1, sizeof(struct rbjeq));
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  **eqp = (struct rbjeq){
      .sample_rate = 48000.f,
      .frequency = 1000.f,
      .q = 1.f,
      .channels = 2,
      .filter_type = rbjeq_type_low_pass,
      .need_parameter_update = true,
  };
  return eok();
}

NODISCARD error rbjeq_destroy(struct rbjeq **const eqp) {
  if (!eqp || !*eqp) {
    return errg(err_invalid_arugment);
  }
  ereport(afree(&(*eqp)->buffers));
  ereport(mem_free(eqp));
  return eok();
}

void rbjeq_set_format(struct rbjeq *const eq, float const sample_rate, size_t const channels) {
  if (fcmp(eq->sample_rate, ==, sample_rate, 1e-12f) && eq->channels == channels) {
    return;
  }
  eq->sample_rate = sample_rate;
  eq->channels = channels;
  eq->need_parameter_update = true;
}

void rbjeq_set_type(struct rbjeq *const eq, int const v) {
  if (eq->filter_type == v) {
    return;
  }
  eq->filter_type = v;
  eq->need_parameter_update = true;
}

void rbjeq_set_frequency(struct rbjeq *const eq, float const v) {
  if (fcmp(eq->frequency, ==, v, 1e-12f)) {
    return;
  }
  eq->frequency = v;
  eq->need_parameter_update = true;
}

static void write_str(NATIVE_CHAR *dest, NATIVE_CHAR const *src) {
  for (; *src != NSTR('\0'); ++src, ++dest) {
    *dest = *src;
  }
  *dest = NSTR('\0');
}

void rbjeq_get_frequency_str(struct rbjeq *const eq, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(eq->frequency);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void rbjeq_set_q(struct rbjeq *const eq, float const v) {
  if (fcmp(eq->q, ==, v, 1e-12f)) {
    return;
  }
  eq->q = v;
  eq->need_parameter_update = true;
}

float rbjeq_get_gain(struct rbjeq const *const eq) { return eq->gain; }
void rbjeq_set_gain(struct rbjeq *const eq, float const v) {
  if (fcmp(eq->gain, ==, v, 1e-12f)) {
    return;
  }
  eq->gain = v;
  eq->need_parameter_update = true;
}

void rbjeq_get_gain_str(struct rbjeq *const eq, NATIVE_CHAR dest[16]) {
  NATIVE_CHAR tmp[64];
  write_str(dest, ov_ftoa((double)eq->gain, 2, NSTR('.'), tmp));
}

NODISCARD static error update_internal_parameter(struct rbjeq *const eq) {
  if (eq->buffers.len != eq->channels) {
    error err = agrow(&eq->buffers, eq->channels);
    if (efailed(err)) {
      err = ethru(err);
      return err;
    }
    eq->buffers.len = eq->channels;
    rbjeq_clear(eq);
  }
  static float const pi = 3.14159265358979323846264338327950288f;
  float const sample_rate = eq->sample_rate;
  float const freq = fmaxf(0.f, fminf(eq->frequency, sample_rate * 0.5f));
  float const q = fmaxf(eq->q, 1e-12f);
  float const gain = eq->gain;

  float const A = powf(10.f, gain / 40.f);
  float const w0 = 2.f * pi * freq / sample_rate;
  float const tsin = sinf(w0), tcos = cosf(w0);
  float const alpha = tsin / (2.f * q);
  float const beta = sqrtf(A) / q;
  float a0, a1, a2, b0, b1, b2;
  switch (eq->filter_type) {
  case rbjeq_type_low_pass:
    b0 = (1.f - tcos) / 2.f;
    b1 = 1.f - tcos;
    b2 = (1.f - tcos) / 2.f;
    a0 = 1.f + alpha;
    a1 = -2.f * tcos;
    a2 = 1.f - alpha;
    break;
  case rbjeq_type_high_pass:
    b0 = (1.f + tcos) / 2.f;
    b1 = -(1.f + tcos);
    b2 = (1.f + tcos) / 2.f;
    a0 = 1.f + alpha;
    a1 = -2.f * tcos;
    a2 = 1.f - alpha;
    break;
  case rbjeq_type_band_pass:
    b0 = alpha;
    b1 = 0.f;
    b2 = -alpha;
    a0 = 1.f + alpha;
    a1 = -2.f * tcos;
    a2 = 1.f - alpha;
    break;
  case rbjeq_type_notch:
    b0 = 1.f;
    b1 = -2.f * tcos;
    b2 = 1.f;
    a0 = 1.f + alpha;
    a1 = -2.f * tcos;
    a2 = 1.f - alpha;
    break;
  case rbjeq_type_peaking:
    b0 = 1.f + (alpha * A);
    b1 = -2.f * tcos;
    b2 = 1.f - (alpha * A);
    a0 = 1.f + (alpha / A);
    a1 = -2.f * tcos;
    a2 = 1.f - (alpha / A);
    break;
  case rbjeq_type_all_pass:
    b0 = 1.f - alpha;
    b1 = -2.f * tcos;
    b2 = 1.f + alpha;
    a0 = 1.f + alpha;
    a1 = -2.f * tcos;
    a2 = 1.f - alpha;
    break;
  case rbjeq_type_low_shelf:
    b0 = A * ((A + 1.f) - (A - 1.f) * tcos + beta * tsin);
    b1 = 2.f * A * ((A - 1.f) - (A + 1.f) * tcos);
    b2 = A * ((A + 1.f) - (A - 1.f) * tcos - beta * tsin);
    a0 = (A + 1.f) + (A - 1.f) * tcos + beta * tsin;
    a1 = -2.f * ((A - 1.f) + (A + 1.f) * tcos);
    a2 = (A + 1.f) + (A - 1.f) * tcos - beta * tsin;
    break;
  case rbjeq_type_high_shelf:
    b0 = A * ((A + 1.f) + (A - 1.f) * tcos + beta * tsin);
    b1 = -2.f * A * ((A - 1.f) + (A + 1.f) * tcos);
    b2 = A * ((A + 1.f) + (A - 1.f) * tcos - beta * tsin);
    a0 = (A + 1.f) - (A - 1.f) * tcos + beta * tsin;
    a1 = 2.f * ((A - 1.f) - (A + 1.f) * tcos);
    a2 = (A + 1.f) - (A - 1.f) * tcos - beta * tsin;
    break;
  default:
    return errg(err_unexpected);
  }
  eq->b0a0 = b0 / a0;
  eq->b1a0 = b1 / a0;
  eq->b2a0 = b2 / a0;
  eq->a1a0 = a1 / a0;
  eq->a2a0 = a2 / a0;
  return eok();
}

NODISCARD error rbjeq_update_internal_parameter(struct rbjeq *const eq, bool *const updated) {
  if (!eq->need_parameter_update) {
    if (updated) {
      *updated = false;
    }
    return eok();
  }
  error err = update_internal_parameter(eq);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  eq->need_parameter_update = false;
  if (updated) {
    *updated = true;
  }
  return eok();
}

float rbjeq_get_lookahead_duration(struct rbjeq *const eq) {
  if (eq->sample_rate == 0.f) {
    return 0.f;
  }
  return 2.f / eq->sample_rate;
}

void rbjeq_process(struct rbjeq *const eq,
                   float const *restrict const *const inputs,
                   float *restrict const *const outputs,
                   size_t const samples) {
  float const denom = 1e-24f;
  float const b0a0 = eq->b0a0;
  float const b1a0 = eq->b1a0;
  float const b2a0 = eq->b2a0;
  float const a1a0 = eq->a1a0;
  float const a2a0 = eq->a2a0;
  struct channel *const chbufs = eq->buffers.ptr;
  float i0, i1, o0, o1, last, v;
  float const *in;
  float *out;
  for (size_t ch = 0, chlen = eq->buffers.len; ch < chlen; ++ch) {
    struct channel *chbuf = chbufs + ch;
    i0 = chbuf->in0;
    i1 = chbuf->in1;
    o0 = chbuf->out0;
    o1 = chbuf->out1;
    in = inputs[ch];
    out = outputs[ch];
    for (size_t i = 0; i < samples; ++i) {
      v = in[i];
      last = b0a0 * v + b1a0 * i0 + b2a0 * i1 - a1a0 * o0 - a2a0 * o1 + denom;
      last -= denom;
      i1 = i0;
      i0 = v;
      o1 = o0;
      o0 = last;
      out[i] = last;
    }
    chbuf->in0 = i0;
    chbuf->in1 = i1;
    chbuf->out0 = o0;
    chbuf->out1 = o1;
  }
}

void rbjeq_clear(struct rbjeq *const eq) {
  for (size_t ch = 0, chlen = eq->buffers.len; ch < chlen; ++ch) {
    eq->buffers.ptr[ch] = (struct channel){0};
  }
}

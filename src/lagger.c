#include "lagger.h"

#include "ovnum.h"
#include "ovutil/str.h"

#include "circbuffer.h"
#include "inlines.h"

#include <math.h>

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct lagger {
  struct circbuffer *buf;
  float duration;
  float sample_rate;
  size_t samples;
  size_t channels;
  bool need_parameter_update;
};

NODISCARD error lagger_create(struct lagger **const lp) {
  if (!lp || *lp) {
    return errg(err_invalid_arugment);
  }
  struct lagger tmp = {0};
  error err = circbuffer_create(&tmp.buf);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = mem(lp, 1, sizeof(struct lagger));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  **lp = (struct lagger){
      .buf = tmp.buf,
      .sample_rate = 48000.f,
      .channels = 2,
      .need_parameter_update = true,
  };
  tmp.buf = NULL;

cleanup:
  if (tmp.buf) {
    ereport(circbuffer_destroy(&tmp.buf));
  }
  return err;
}

NODISCARD error lagger_destroy(struct lagger **const lp) {
  if (!lp || !*lp) {
    return errg(err_invalid_arugment);
  }
  ereport(circbuffer_destroy(&(*lp)->buf));
  ereport(mem_free(lp));
  return eok();
}

void lagger_clear(struct lagger *const l) {
  circbuffer_clear(l->buf);
  ereport(circbuffer_write_silence(l->buf, l->samples));
}

float lagger_get_duration(struct lagger const *const l) { return l->duration; }

static void write_str(NATIVE_CHAR *dest, NATIVE_CHAR const *src) {
  for (; *src != NSTR('\0'); ++src, ++dest) {
    *dest = *src;
  }
  *dest = NSTR('\0');
}

void lagger_get_duration_str(struct lagger const *const l, NATIVE_CHAR dest[16]) {
  int64_t const v = (int64_t)(1000.f * l->duration);
  NATIVE_CHAR tmp[32];
  write_str(dest, ov_itoa(v, tmp));
}

void lagger_set_format(struct lagger *const l, float const sample_rate, size_t const channels) {
  if (fcmp(l->sample_rate, ==, sample_rate, 1e-12f) && l->channels == channels) {
    return;
  }
  l->sample_rate = sample_rate;
  l->channels = channels;
  l->need_parameter_update = true;
}

void lagger_set_duration(struct lagger *const l, float const duration) {
  if (fcmp(l->duration, ==, duration, 1e-12f)) {
    return;
  }
  l->duration = duration;
  l->need_parameter_update = true;
}

NODISCARD error lagger_update_internal_parameter(struct lagger *const l, bool *const updated) {
  if (!l->need_parameter_update) {
    if (updated) {
      *updated = false;
    }
    return eok();
  }
  l->samples = (size_t)(l->duration * l->sample_rate);
  circbuffer_clear(l->buf);
  error err = circbuffer_set_channels(l->buf, l->channels);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  err = circbuffer_write_silence(l->buf, l->samples);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  l->need_parameter_update = false;
  if (updated) {
    *updated = true;
  }
  return eok();
}

void lagger_process(struct lagger *const l,
                    float const *restrict const *const inputs,
                    float *restrict const *const outputs,
                    size_t const samples) {
  if (!l->samples) {
    for (size_t ch = 0, chlen = l->channels; ch < chlen; ++ch) {
      memcpy(outputs[ch], inputs[ch], samples * sizeof(float));
    }
    return;
  }
  if (l->samples >= samples) {
    size_t written = 0;
    ereport(circbuffer_read(l->buf, outputs, samples, &written));
    ereport(circbuffer_write(l->buf, inputs, written));
    return;
  }
  size_t written = 0;
  ereport(circbuffer_read(l->buf, outputs, l->samples, &written));
  for (size_t ch = 0, chlen = l->channels; ch < chlen; ++ch) {
    memcpy(outputs[ch] + written, inputs[ch], (samples - written) * sizeof(float));
  }
  ereport(circbuffer_write_offset(l->buf, inputs, written, samples - written));
}

#include "channel.h"

#include <math.h>

#include "ovnum.h"
#include "ovutil/str.h"

#include "circbuffer_i16.h"
#include "dynamics.h"
#include "inlines.h"
#include "lagger.h"
#include "rbjeq.h"

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

struct channel {
  size_t used_at;
  struct circbuffer_i16 *buf;
  struct lagger *lagger;
  struct rbjeq *low_shelf;
  struct rbjeq *high_shelf;
  struct dynamics *dyn;
  int id;
  int aux_send_id;

  float pre_gain;
  float aux_send;
  float post_gain;
  float pan;
  struct channel *next;

  bool parameter_changed;
};

NODISCARD static error channel_set_format(struct channel *const c, float const sample_rate, size_t const channels) {
  error err = eok();
  err = circbuffer_i16_set_channels(c->buf, channels);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  lagger_set_format(c->lagger, sample_rate, channels);
  rbjeq_set_format(c->low_shelf, sample_rate, channels);
  rbjeq_set_format(c->high_shelf, sample_rate, channels);
  dynamics_set_format(c->dyn, sample_rate, channels);
cleanup:
  return err;
}

static void channel_set_effects(struct channel *const c, struct channel_effect_params const *e) {
  if (fcmp(c->pre_gain, !=, e->pre_gain, 1e-12f)) {
    c->pre_gain = e->pre_gain;
    c->parameter_changed = true;
  }
  lagger_set_duration(c->lagger, e->lagger_duration);
  rbjeq_set_frequency(c->low_shelf, e->low_shelf_frequency);
  rbjeq_set_gain(c->low_shelf, e->low_shelf_gain);
  rbjeq_set_frequency(c->high_shelf, e->high_shelf_frequency);
  rbjeq_set_gain(c->high_shelf, e->high_shelf_gain);
  dynamics_set_thresh(c->dyn, e->dynamics_threshold);
  dynamics_set_ratio(c->dyn, e->dynamics_ratio);
  dynamics_set_attack(c->dyn, e->dynamics_attack);
  dynamics_set_release(c->dyn, e->dynamics_release);
  if (c->aux_send_id != e->aux_send_id) {
    c->aux_send_id = e->aux_send_id;
    c->parameter_changed = true;
  }
  if (fcmp(c->aux_send, !=, e->aux_send, 1e-12f)) {
    c->aux_send = e->aux_send;
    c->parameter_changed = true;
  }
  if (fcmp(c->post_gain, !=, e->post_gain, 1e-12f)) {
    c->post_gain = e->post_gain;
    c->parameter_changed = true;
  }
  if (fcmp(c->pan, !=, e->pan, 1e-12f)) {
    c->pan = e->pan;
    c->parameter_changed = true;
  }
}

NODISCARD static error channel_update_internal_parameter(struct channel *const c, bool *const updated) {
  bool lagger_updated = false;
  bool low_shelf_updated = false;
  bool high_shelf_updated = false;
  bool dynamics_updated = false;
  error err = lagger_update_internal_parameter(c->lagger, &lagger_updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = rbjeq_update_internal_parameter(c->low_shelf, &low_shelf_updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = rbjeq_update_internal_parameter(c->high_shelf, &high_shelf_updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  dynamics_update_internal_parameter(c->dyn, &dynamics_updated);
  if (updated) {
    *updated = c->parameter_changed || lagger_updated || low_shelf_updated || high_shelf_updated || dynamics_updated;
  }
  c->parameter_changed = false;

cleanup:
  return err;
}

NODISCARD static error channel_destroy(struct channel **cp) {
  if (!cp || !*cp) {
    return errg(err_invalid_arugment);
  }
  struct channel *c = *cp;
  if (c->buf) {
    ereport(circbuffer_i16_destroy(&c->buf));
  }
  if (c->lagger) {
    ereport(lagger_destroy(&c->lagger));
  }
  if (c->low_shelf) {
    ereport(rbjeq_destroy(&c->low_shelf));
  }
  if (c->high_shelf) {
    ereport(rbjeq_destroy(&c->high_shelf));
  }
  if (c->dyn) {
    ereport(dynamics_destroy(&c->dyn));
  }
  ereport(mem_free(cp));
  return eok();
}

NODISCARD static error channel_create(struct channel **const cp) {
  static float const sqrt2 = 1.41421356237309504880f;

  if (!cp || *cp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(cp, 1, sizeof(struct channel));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct channel *const c = *cp;
  *c = (struct channel){0};
  err = circbuffer_i16_create(&c->buf);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = lagger_create(&c->lagger);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = rbjeq_create(&c->low_shelf);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  rbjeq_set_type(c->low_shelf, rbjeq_type_low_shelf);
  rbjeq_set_q(c->low_shelf, 1.f / sqrt2);

  err = rbjeq_create(&c->high_shelf);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  rbjeq_set_type(c->high_shelf, rbjeq_type_high_shelf);
  rbjeq_set_q(c->high_shelf, 1.f / sqrt2);

  err = dynamics_create(&c->dyn);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  dynamics_set_output(c->dyn, 0.f);

  err = channel_set_format(c, 48000.f, 2);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  channel_set_effects(c,
                      &(struct channel_effect_params){
                          .pre_gain = 1.f,
                          .lagger_duration = 0.f,
                          .low_shelf_frequency = 200.f,
                          .low_shelf_gain = 0.f,
                          .high_shelf_frequency = 3000.f,
                          .high_shelf_gain = 0.f,
                          .dynamics_threshold = 0.4f,
                          .dynamics_ratio = 0.6f,
                          .dynamics_attack = 0.18f,
                          .dynamics_release = 0.55f,
                          .aux_send = 0.f,
                          .post_gain = 1.f,
                          .pan = 1.f,
                      });
  err = channel_update_internal_parameter(c, NULL);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  if (efailed(err)) {
    if (*cp) {
      ereport(channel_destroy(cp));
    }
  }
  return err;
}

static float channel_get_lookahead_duration(struct channel const *const c) {
  float r = fmaxf(0.f, lagger_get_duration(c->lagger));
  r = fmaxf(r, rbjeq_get_lookahead_duration(c->low_shelf));
  r = fmaxf(r, dynamics_get_attack_duration(c->dyn) + dynamics_get_release_duration(c->dyn));
  return r;
}

static void channel_reset(struct channel *const c) {
  c->used_at = 0;
  circbuffer_i16_clear(c->buf);
  lagger_clear(c->lagger);
  rbjeq_clear(c->low_shelf);
  rbjeq_clear(c->high_shelf);
  dynamics_clear(c->dyn);
}

// ----------------------------------------------------------------

struct channel_list {
  struct channel *head;
  channel_notify_func notify_func;
  channel_write_to_send_func write_to_send_target_func;
  void *userdata;
};

static struct channel *get_head(struct channel_list const *const cl) { return cl->head; }
static void set_head(struct channel_list *const cl, struct channel *const new_head) { cl->head = new_head; }

void channel_list_set_userdata(struct channel_list *const cl, void *const userdata) { cl->userdata = userdata; }
void channel_list_set_notify_callback(struct channel_list *const cl, channel_notify_func f) { cl->notify_func = f; }
void channel_list_set_write_to_send_target_callback(struct channel_list *const cl, channel_write_to_send_func f) {
  cl->write_to_send_target_func = f;
}

static void free_all(struct channel_list *const cl) {
  struct channel *next = NULL;
  struct channel *c = get_head(cl);
  set_head(cl, NULL);
  while (c) {
    next = c->next;
    ereport(channel_destroy(&c));
    c = next;
  }
}

NODISCARD error channel_list_destroy(struct channel_list **const clp) {
  if (!clp || !*clp) {
    return errg(err_invalid_arugment);
  }
  struct channel_list *cl = *clp;
  free_all(cl);
  ereport(mem_free(clp));
  return eok();
}

NODISCARD error channel_list_create(struct channel_list **const clp) {
  if (!clp || *clp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(clp, 1, sizeof(struct channel_list));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct channel_list *cl = *clp;
  *cl = (struct channel_list){0};
cleanup:
  if (efailed(err)) {
    if (*clp) {
      ereport(channel_list_destroy(clp));
    }
  }
  return err;
}

NODISCARD static error find(struct channel_list *const cl, int const id, size_t counter, struct channel **const r) {
  struct channel *c = NULL;
  struct channel *prev = NULL;
  for (c = get_head(cl); c; c = c->next) {
    if (c->id == id) {
      if (c->used_at == counter) {
        // already used
        *r = NULL;
        return eok();
      }
      if (c->used_at + 1 != counter) {
        // reuse
        channel_reset(c);
      }
      c->used_at = counter;
      *r = c;
      return eok();
    }
    if (c->id > id) {
      goto not_found;
    }
    if (!c->next || c->next->id > id) {
      prev = c;
      goto not_found;
    }
  }

not_found:
  c = NULL;
  error err = channel_create(&c);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  c->id = id;
  if (prev) {
    c->next = prev->next;
    prev->next = c;
  } else {
    c->next = get_head(cl);
    set_head(cl, c);
  }
  *r = c;
  return eok();
}

NODISCARD error channel_list_channel_update(struct channel_list *const cl,
                                            int const id,
                                            size_t const counter,
                                            struct channel_effect_params const *e,
                                            int16_t const *restrict const src,
                                            size_t const samples,
                                            bool *const updated) {
  struct channel *c = NULL;
  error err = find(cl, id, counter, &c);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!c) {
    goto cleanup;
  }
  channel_set_effects(c, e);
  err = channel_update_internal_parameter(c, updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = circbuffer_i16_write(c->buf, src, samples);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }

cleanup:
  return err;
}

void channel_list_mix(struct channel_list const *const cl,
                      size_t const counter,
                      size_t const samples,
                      float *restrict const *const mixbuf,
                      float *restrict const *const chbuf,
                      float *restrict const *const tmpbuf) {
  float *restrict const *ch = chbuf;
  float *restrict const *tmp = tmpbuf;
  static float const i16_to_float = 1.f / 32768.f;
  for (struct channel *c = get_head(cl); c; c = c->next) {
    if (c->used_at != counter && circbuffer_i16_get_remain(c->buf) == 0) {
      continue;
    }
    size_t read = 0;
    ereport(circbuffer_i16_read_as_float(c->buf, ch, samples, i16_to_float * db_to_amp(c->pre_gain), &read));
    if (read < samples) {
      for (size_t i = 0, channels = circbuffer_i16_get_channels(c->buf); i < channels; ++i) {
        memset(ch[i] + read, 0, (samples - read) * sizeof(float));
      }
    }
    if (lagger_get_duration(c->lagger) > 0.f) {
      lagger_process(c->lagger, (float const *restrict const *)ch, tmp, samples);
      swap(&ch, &tmp);
    }
    if (fcmp(rbjeq_get_gain(c->low_shelf), !=, 0.f, 1e-12f)) {
      rbjeq_process(c->low_shelf, (float const *restrict const *)ch, tmp, samples);
      swap(&ch, &tmp);
    }
    if (fcmp(rbjeq_get_gain(c->high_shelf), !=, 0.f, 1e-12f)) {
      rbjeq_process(c->high_shelf, (float const *restrict const *)ch, tmp, samples);
      swap(&ch, &tmp);
    }
    if (fcmp(dynamics_get_ratio(c->dyn), !=, 0.2f, 1e-12f)) {
      dynamics_process(c->dyn, (float const *restrict const *)ch, tmp, samples);
      swap(&ch, &tmp);
    }
    if (c->aux_send_id > -1 && fcmp(c->aux_send, >, -144.f, 1e-12f) && cl->write_to_send_target_func) {
      ereport(cl->write_to_send_target_func(
          cl->userdata, c->aux_send_id, counter, (float const *restrict const *)ch, samples, c->aux_send));
    }
    size_t const channels = circbuffer_i16_get_channels(c->buf);
    if (channels == 2) {
      stereo_pan_and_gain(tmp, (float const *restrict const *)ch, c->pan, c->post_gain, samples);
      swap(&ch, &tmp);
    } else {
      gain(tmp, (float const *restrict const *)ch, c->post_gain, channels, samples);
      swap(&ch, &tmp);
    }
    if (cl->notify_func) {
      cl->notify_func(cl->userdata, c->id, (float const *restrict const *)ch, channels, samples);
    }
    mix(mixbuf, (float const *restrict const *)ch, channels, samples);
  }
}

void channel_list_gc(struct channel_list *const cl, size_t const counter) {
  struct channel *prev = NULL;
  struct channel *next = NULL;
  struct channel *c = get_head(cl);
  while (c) {
    if (c->used_at == counter || circbuffer_i16_get_remain(c->buf) > 0) {
      prev = c;
      c = c->next;
      continue;
    }
    next = c->next;
    if (prev) {
      prev->next = next;
    } else {
      set_head(cl, next);
    }
    ereport(channel_destroy(&c));
    c = next;
  }
}

NODISCARD error channel_list_set_format(struct channel_list const *const cl,
                                        float const sample_rate,
                                        size_t const channels,
                                        bool *const updated) {
  error err = eok();
  bool upd = false;
  for (struct channel *c = get_head(cl); c; c = c->next) {
    err = channel_set_format(c, sample_rate, channels);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    bool b = false;
    err = channel_update_internal_parameter(c, &b);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    upd = upd || b;
  }
  if (updated) {
    *updated = upd;
  }
cleanup:
  return err;
}

void channel_list_reset(struct channel_list const *const cl) {
  for (struct channel *c = get_head(cl); c; c = c->next) {
    channel_reset(c);
  }
}

static void write_str(NATIVE_CHAR *dest, NATIVE_CHAR const *src) {
  for (; *src != NSTR('\0'); ++src, ++dest) {
    *dest = *src;
  }
  *dest = NSTR('\0');
}

static void write_double(NATIVE_CHAR dest[16], double value, NATIVE_CHAR buf[64]) {
  write_str(dest, ov_ftoa(value, 2, NSTR('.'), buf));
}

static void get_effect_params_str(struct channel const *const c, struct channel_effect_params_str *params) {
  NATIVE_CHAR tmp[64];
  write_double(params->pre_gain, (double)c->pre_gain, tmp);
  lagger_get_duration_str(c->lagger, params->lagger_duration);
  rbjeq_get_frequency_str(c->low_shelf, params->low_shelf_frequency);
  rbjeq_get_gain_str(c->low_shelf, params->low_shelf_gain);
  rbjeq_get_frequency_str(c->high_shelf, params->high_shelf_frequency);
  rbjeq_get_gain_str(c->high_shelf, params->high_shelf_gain);
  dynamics_get_threshold_str(c->dyn, params->dynamics_threshold);
  dynamics_get_ratio_str(c->dyn, params->dynamics_ratio);
  dynamics_get_attack_str(c->dyn, params->dynamics_attack);
  dynamics_get_release_str(c->dyn, params->dynamics_release);
  write_double(params->aux_send, (double)c->aux_send, tmp);
  write_double(params->post_gain, (double)c->post_gain, tmp);
  write_double(params->pan, (double)c->pan, tmp);
}

NODISCARD error channel_list_get_effect_params_str(struct channel_list const *const cl,
                                                   int const id,
                                                   struct channel_effect_params_str *params,
                                                   bool *const found) {
  for (struct channel *c = get_head(cl); c; c = c->next) {
    if (c->id == id) {
      get_effect_params_str(c, params);
      if (found) {
        *found = true;
      }
      return eok();
    }
    if (c->id > id) {
      goto not_found;
    }
    if (!c->next || c->next->id > id) {
      goto not_found;
    }
  }

not_found:
  if (found) {
    *found = false;
  }
  return eok();
}

float channel_list_get_longest_lookahead_duration(struct channel_list const *const cl) {
  float v = 0.f;
  for (struct channel *c = get_head(cl); c; c = c->next) {
    v = fmaxf(v, channel_get_lookahead_duration(c));
  }
  return v;
}

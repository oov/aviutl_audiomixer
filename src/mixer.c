#include "mixer.h"

#include "array2d.h"
#include "dynamics.h"
#include "inlines.h"

struct mixer {
  struct channel_list *cl;
  struct aux_channel_list *acl;
  struct dynamics *limiter;
  void *userdata;
  mixer_output_notify_func output_notify_func;

  float sample_rate;
  size_t channels;
  size_t samples_per_frame;
  size_t frame_counter;
  uint64_t position;
  bool warming;

  struct dither dither;
  struct array2d mixbuf;
  struct array2d chbuf;
  struct array2d auxbuf;
  struct array2d subbuf;
};

static void release_buffer(struct mixer *const m) {
  array2d_release(&m->subbuf);
  array2d_release(&m->auxbuf);
  array2d_release(&m->chbuf);
  array2d_release(&m->mixbuf);
  ereport(dither_destroy(&m->dither));
}

NODISCARD static error allocate_buffer(struct mixer *const m, size_t const samples_per_frame, size_t const channels) {
  error err = dither_create(&m->dither, channels);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = array2d_allocate(&m->mixbuf, channels, samples_per_frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = array2d_allocate(&m->chbuf, channels, samples_per_frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = array2d_allocate(&m->auxbuf, channels, samples_per_frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = array2d_allocate(&m->subbuf, channels, samples_per_frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  if (efailed(err)) {
    release_buffer(m);
  }
  return err;
}

NODISCARD error mixer_destroy(struct mixer **const mp) {
  if (!mp || !*mp) {
    return errg(err_invalid_arugment);
  }
  struct mixer *m = *mp;
  if (m->limiter) {
    ereport(dynamics_destroy(&m->limiter));
  }
  ereport(channel_list_destroy(&m->cl));
  ereport(aux_channel_list_destroy(&m->acl));
  release_buffer(m);
  ereport(mem_free(mp));
  return eok();
}

NODISCARD static error write_to_send(void *const userdata,
                                     int const send_id,
                                     size_t const frame_counter,
                                     float const *restrict const *const src,
                                     size_t const samples,
                                     float const gain_db) {
  struct mixer *const m = userdata;
  error err = aux_channel_list_add(m->acl, send_id, frame_counter, src, samples, gain_db);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

static void channel_notify(void *const userdata,
                           int const id,
                           float const *restrict const *const buf,
                           size_t const channels,
                           size_t const samples) {
  struct mixer *const m = userdata;
  if (m->output_notify_func && !m->warming) {
    m->output_notify_func(m->userdata, m, mixer_channel_type_channel, id, buf, channels, samples);
  }
}

static void aux_channel_notify(void *const userdata,
                               int const id,
                               float const *restrict const *const buf,
                               size_t const channels,
                               size_t const samples) {
  struct mixer *const m = userdata;
  if (m->output_notify_func && !m->warming) {
    m->output_notify_func(m->userdata, m, mixer_channel_type_aux_channel, id, buf, channels, samples);
  }
}

NODISCARD error mixer_create(struct mixer **const mp) {
  if (!mp || *mp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(mp, 1, sizeof(struct mixer));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct mixer *const m = *mp;
  *m = (struct mixer){
      .frame_counter = 1,
  };
  err = dynamics_create(&m->limiter);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  dynamics_set_thresh(m->limiter, 1.f);
  dynamics_set_ratio(m->limiter, 0.6f);
  dynamics_set_attack(m->limiter, 0.f);   // 2 μsec
  dynamics_set_release(m->limiter, 0.8f); // 134 msec
  dynamics_set_output(m->limiter, 0.f);
  dynamics_set_limiter(m->limiter, 0.7f);

  err = channel_list_create(&m->cl);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = aux_channel_list_create(&m->acl);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  channel_list_set_userdata(m->cl, m);
  channel_list_set_write_to_send_target_callback(m->cl, write_to_send);
  channel_list_set_notify_callback(m->cl, channel_notify);
  aux_channel_list_set_userdata(m->acl, m);
  aux_channel_list_set_notify_callback(m->acl, aux_channel_notify);

  err = mixer_set_format(m, 1.f, 1, 16);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  if (efailed(err)) {
    if (*mp) {
      ereport(mixer_destroy(mp));
    }
  }
  return err;
}

void mixer_reset(struct mixer *const m) {
  dynamics_clear(m->limiter);
  channel_list_reset(m->cl);
  aux_channel_list_reset(m->acl);
  m->frame_counter = 1;
  m->position = 0;
  dither_reset(&m->dither);
}

NODISCARD error mixer_set_format(struct mixer *const m,
                                 float const sample_rate,
                                 size_t const channels,
                                 size_t const samples_per_frame) {
  if (fcmp(sample_rate, ==, m->sample_rate, 1e-12f) && channels == m->channels &&
      samples_per_frame == m->samples_per_frame) {
    return eok();
  }
  struct mixer tmp = {0};
  error err = allocate_buffer(&tmp, samples_per_frame, channels);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  release_buffer(m);

  err = channel_list_set_format(m->cl, sample_rate, channels, NULL);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = aux_channel_list_set_format(m->acl, sample_rate, channels, samples_per_frame, NULL);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  dynamics_set_format(m->limiter, sample_rate, channels);
  dynamics_update_internal_parameter(m->limiter, NULL);

  m->subbuf = tmp.subbuf;
  m->auxbuf = tmp.auxbuf;
  m->chbuf = tmp.chbuf;
  m->mixbuf = tmp.mixbuf;
  m->dither = tmp.dither;
  m->sample_rate = sample_rate;
  m->channels = channels;
  m->samples_per_frame = samples_per_frame;

  mixer_reset(m);
cleanup:
  return err;
}

NODISCARD error mixer_get_channel_parameter_str(struct mixer *const m,
                                                int const id,
                                                struct channel_effect_params_str *dest,
                                                bool *const found) {
  if (!m || !dest || !found) {
    return errg(err_invalid_arugment);
  }
  error err = channel_list_get_effect_params_str(m->cl, id, dest, found);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD error mixer_update_channel(struct mixer *const m,
                                     int const channel_id,
                                     struct channel_effect_params const *e,
                                     int16_t const *restrict const src,
                                     size_t const samples,
                                     bool *const updated) {
  error err = channel_list_channel_update(m->cl, channel_id, m->frame_counter, e, src, samples, updated);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

NODISCARD error mixer_update_aux_channel(struct mixer *const m,
                                         int const aux_channel_id,
                                         struct aux_channel_effect_params const *e,
                                         bool *const updated) {
  error err = aux_channel_list_channel_update(m->acl, aux_channel_id, m->frame_counter, e, updated);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  return eok();
}

void mixer_mix(struct mixer *const m, int16_t *restrict const buffer, size_t const samples) {
  size_t const channels = m->channels;
  size_t const frame_counter = m->frame_counter;
  float *restrict const *mixbuf = m->mixbuf.ptr;
  float *restrict const *chbuf = m->chbuf.ptr;
  float *restrict const *subbuf = m->subbuf.ptr;

  interleaved_int16_to_float(mixbuf, buffer, channels, samples);

  if (m->output_notify_func && !m->warming) {
    m->output_notify_func(
        m->userdata, m, mixer_channel_type_other, 0, (float const *restrict const *const)mixbuf, channels, samples);
  }

  channel_list_mix(m->cl, frame_counter, samples, mixbuf, chbuf, subbuf);
  aux_channel_list_mix(m->acl, frame_counter, samples, mixbuf, subbuf);

  dynamics_process(m->limiter, (float const *restrict const *const)mixbuf, subbuf, samples);
  swap(&mixbuf, &subbuf);

  float_to_interleaved_int16(buffer, (float const *restrict const *const)mixbuf, &m->dither, channels, samples);

  if ((frame_counter & 0xff) == 0xff) {
    channel_list_gc(m->cl, frame_counter);
    aux_channel_list_gc(m->acl, frame_counter);
  }
  ++m->frame_counter;
  if (!m->warming) {
    m->position += (uint64_t)samples;
  }
}

bool mixer_get_warming(struct mixer const *const m) { return m->warming; }
void mixer_set_warming(struct mixer *const m, bool const warming) { m->warming = warming; }

float mixer_get_warming_up_duration(struct mixer const *const m) {
  return fmaxf(dynamics_get_attack_duration(m->limiter) + dynamics_get_release_duration(m->limiter),
               channel_list_get_longest_lookahead_duration(m->cl));
}

float mixer_get_sample_rate(struct mixer const *const m) { return m->sample_rate; }
size_t mixer_get_channels(struct mixer const *const m) { return m->channels; }
uint64_t mixer_get_position(struct mixer const *const m) { return m->position; }
void mixer_set_userdata(struct mixer *const m, void *const userdata) { m->userdata = userdata; }
void mixer_set_output_notify_callback(struct mixer *const m, mixer_output_notify_func f) { m->output_notify_func = f; }

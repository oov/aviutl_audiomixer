#include "aux_channel.h"

#include <math.h>
#include <stdatomic.h>

#include "array2d.h"
#include "inlines.h"
#include "uxfdreverb.h"

static const struct aux_channel_effect_reverb_params reverb_presets[aux_channel_reverb_preset_max] = {
    {
        // room
        .band_width = 0.56f,
        .pre_delay = 0.1f,
        .diffuse = 0.5f,
        .decay = 0.32f,
        .damping = 0.64f,
        .excursion = 0.f,
    },
    {
        // church
        .band_width = 0.98f,
        .pre_delay = 0.05f,
        .diffuse = 0.9f,
        .decay = 0.82f,
        .damping = 0.29f,
        .excursion = 0.8f,
    },
};

struct aux_channel {
  size_t used_at;
  size_t parameter_updated_at;
  struct uxfdreverb *reverb;
  struct array2d buf;
  int id;

  struct aux_channel *next;
};

static float g_sample_rate = 0;
static size_t g_channels = 0;
static size_t g_buffer_size = 0;

static void clear_buffer(struct aux_channel *const c) {
  clear((float *restrict const *)c->buf.ptr, c->buf.channels, c->buf.buffer_size);
}

NODISCARD static error aux_channel_set_format(struct aux_channel *const c,
                                              float const sample_rate,
                                              size_t const channels,
                                              size_t const buffer_size) {
  struct array2d buf = {0};
  error err = array2d_allocate(&buf, channels, buffer_size);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  array2d_release(&c->buf);
  c->buf = buf;
  clear_buffer(c);
  uxfdreverb_set_format(c->reverb, sample_rate, channels);
cleanup:
  return err;
}

static void aux_channel_reset(struct aux_channel *const c) {
  clear_buffer(c);
  uxfdreverb_clear(c->reverb);
}

static void aux_channel_set_effects(struct aux_channel *const c, struct aux_channel_effect_params const *e) {
  struct aux_channel_effect_reverb_params const *rev = &e->reverb;
  if (0 <= e->reverb_preset && e->reverb_preset < aux_channel_reverb_preset_max) {
    rev = reverb_presets + e->reverb_preset;
  }
  uxfdreverb_set_band_width(c->reverb, rev->band_width);
  uxfdreverb_set_pre_delay(c->reverb, rev->pre_delay);
  uxfdreverb_set_diffuse(c->reverb, rev->diffuse);
  uxfdreverb_set_decay(c->reverb, rev->decay);
  uxfdreverb_set_damping(c->reverb, rev->damping);
  uxfdreverb_set_excursion(c->reverb, rev->excursion);
  uxfdreverb_set_wet(c->reverb, db_to_amp(e->reverb.wet)); // ignore preset wet
}

NODISCARD static error aux_channel_update_internal_parameter(struct aux_channel *const c, bool *const updated) {
  error err = uxfdreverb_update_internal_parameter(c->reverb, updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
cleanup:
  return err;
}

NODISCARD static error aux_channel_destroy(struct aux_channel **cp) {
  if (!cp || !*cp) {
    return errg(err_invalid_arugment);
  }
  struct aux_channel *c = *cp;
  array2d_release(&c->buf);
  if (c->reverb) {
    ereport(uxfdreverb_destroy(&c->reverb));
  }
  ereport(mem_free(cp));
  return eok();
}

NODISCARD static error aux_channel_create(struct aux_channel **const cp) {
  if (!cp || *cp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(cp, 1, sizeof(struct aux_channel));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct aux_channel *const c = *cp;
  *c = (struct aux_channel){0};
  err = uxfdreverb_create(&c->reverb);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  uxfdreverb_set_dry(c->reverb, 0.f);
  err = aux_channel_set_format(c, g_sample_rate, g_channels, g_buffer_size);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = uxfdreverb_update_internal_parameter(c->reverb, NULL);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  if (efailed(err)) {
    if (*cp) {
      ereport(aux_channel_destroy(cp));
    }
  }
  return err;
}

struct aux_channel_list {
  struct aux_channel *head;
  aux_channel_notify_func notify_func;
  void *userdata;
};

static struct aux_channel *get_head(struct aux_channel_list const *const acl) { return acl->head; }
static void set_head(struct aux_channel_list *const acl, struct aux_channel *const new_head) { acl->head = new_head; }
void aux_channel_list_set_userdata(struct aux_channel_list *const acl, void *const userdata) {
  acl->userdata = userdata;
}

void aux_channel_list_set_notify_callback(struct aux_channel_list *const acl, aux_channel_notify_func f) {
  acl->notify_func = f;
}

static void free_all(struct aux_channel_list *const acl) {
  struct aux_channel *next = NULL;
  struct aux_channel *ac = get_head(acl);
  set_head(acl, NULL);
  while (ac) {
    next = ac->next;
    ereport(aux_channel_destroy(&ac));
    ac = next;
  }
}

NODISCARD error aux_channel_list_destroy(struct aux_channel_list **const aclp) {
  if (!aclp || !*aclp) {
    return errg(err_invalid_arugment);
  }
  struct aux_channel_list *acl = *aclp;
  free_all(acl);
  ereport(mem_free(aclp));
  return eok();
}

NODISCARD error aux_channel_list_create(struct aux_channel_list **const aclp) {
  if (!aclp || *aclp) {
    return errg(err_invalid_arugment);
  }
  error err = mem(aclp, 1, sizeof(struct aux_channel_list));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  struct aux_channel_list *acl = *aclp;
  *acl = (struct aux_channel_list){0};
cleanup:
  if (efailed(err)) {
    if (*aclp) {
      ereport(aux_channel_list_destroy(aclp));
    }
  }
  return err;
}

NODISCARD static error
find(struct aux_channel_list *const acl, int const id, size_t const counter, struct aux_channel **const r) {
  struct aux_channel *c = NULL;
  struct aux_channel *prev = NULL;
  for (c = get_head(acl); c; c = c->next) {
    if (c->id == id) {
      if (c->used_at != counter) {
        if (c->used_at + 1 != counter) {
          // reuse of an used channel
          aux_channel_reset(c);
        } else {
          // first call in current round
          clear_buffer(c);
        }
        c->used_at = counter;
      }
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
  error err = aux_channel_create(&c);
  if (efailed(err)) {
    err = ethru(err);
    return err;
  }
  c->id = id;
  if (prev) {
    c->next = prev->next;
    prev->next = c;
  } else {
    c->next = get_head(acl);
    set_head(acl, c);
  }
  *r = c;
  return eok();
}

NODISCARD error aux_channel_list_channel_update(struct aux_channel_list *const acl,
                                                int const id,
                                                size_t const counter,
                                                struct aux_channel_effect_params const *e,
                                                bool *const updated) {
  struct aux_channel *c = NULL;
  error err = find(acl, id, counter, &c);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  aux_channel_set_effects(c, e);
  err = aux_channel_update_internal_parameter(c, updated);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  c->parameter_updated_at = counter;

cleanup:
  return err;
}

NODISCARD error aux_channel_list_add(struct aux_channel_list *const acl,
                                     int const id,
                                     size_t const counter,
                                     float const *restrict const *const src,
                                     size_t const samples,
                                     float const gain_db) {
  struct aux_channel *c = NULL;
  error err = find(acl, id, counter, &c);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  float *restrict const *const mixbuf = c->buf.ptr;
  mix_with_gain(mixbuf, src, gain_db, c->buf.channels, samples);

cleanup:
  return err;
}

void aux_channel_list_mix(struct aux_channel_list const *const acl,
                          size_t const counter,
                          size_t const samples,
                          float *restrict const *const mixbuf,
                          float *restrict const *const subbuf) {
  float *restrict const *buf;
  float *restrict const *tmp = subbuf;
  for (struct aux_channel *c = get_head(acl); c; c = c->next) {
    if (c->parameter_updated_at != counter) {
      continue;
    }
    buf = c->buf.ptr;
    size_t const channels = c->buf.channels;
    uxfdreverb_process(c->reverb, (float const *restrict const *)buf, tmp, samples);
    swap(&buf, &tmp);
    if (acl->notify_func) {
      acl->notify_func(acl->userdata, c->id, (float const *restrict const *)buf, channels, samples);
    }
    mix(mixbuf, (float const *restrict const *)buf, channels, samples);
  }
}

void aux_channel_list_gc(struct aux_channel_list *const acl, size_t const counter) {
  struct aux_channel *prev = NULL;
  struct aux_channel *next = NULL;
  struct aux_channel *c = get_head(acl);
  while (c) {
    if (c->used_at == counter) {
      prev = c;
      c = c->next;
      continue;
    }
    next = c->next;
    if (prev) {
      prev->next = next;
    } else {
      set_head(acl, next);
    }
    ereport(aux_channel_destroy(&c));
    c = next;
  }
}

NODISCARD error aux_channel_list_set_format(struct aux_channel_list const *const acl,
                                            float const sample_rate,
                                            size_t const channels,
                                            size_t const buffer_size,
                                            bool *const updated) {
  g_sample_rate = sample_rate;
  g_channels = channels;
  g_buffer_size = buffer_size;
  bool upd = false;
  error err = eok();
  for (struct aux_channel *c = get_head(acl); c; c = c->next) {
    err = aux_channel_set_format(c, sample_rate, channels, buffer_size);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    bool b = false;
    err = aux_channel_update_internal_parameter(c, &b);
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

void aux_channel_list_reset(struct aux_channel_list const *const acl) {
  for (struct aux_channel *c = get_head(acl); c; c = c->next) {
    aux_channel_reset(c);
  }
}

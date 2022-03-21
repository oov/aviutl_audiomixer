#pragma once

#include "ovbase.h"

struct aux_channel_effect_reverb_params {
  float band_width;
  float pre_delay;
  float diffuse;
  float decay;
  float damping;
  float excursion;
  float wet;
};

struct aux_channel_effect_params {
  struct aux_channel_effect_reverb_params reverb;
};

typedef void (*aux_channel_notify_func)(void *const userdata,
                                        int const id,
                                        float const *restrict const *const buf,
                                        size_t const channels,
                                        size_t const samples);

struct aux_channel_list;

NODISCARD error aux_channel_list_create(struct aux_channel_list **const aclp);
NODISCARD error aux_channel_list_destroy(struct aux_channel_list **const aclp);

void aux_channel_list_set_userdata(struct aux_channel_list *const acl, void *const userdata);
void aux_channel_list_set_notify_callback(struct aux_channel_list *const acl, aux_channel_notify_func f);

NODISCARD error aux_channel_list_set_format(struct aux_channel_list const *const acl,
                                            float const sample_rate,
                                            size_t const channels,
                                            size_t const buffer_size,
                                            bool *const updated);

NODISCARD error aux_channel_list_channel_update(struct aux_channel_list *const acl,
                                                int const id,
                                                size_t const counter,
                                                struct aux_channel_effect_params const *e,
                                                bool *const updated);

NODISCARD error aux_channel_list_add(struct aux_channel_list *const acl,
                                     int const id,
                                     size_t const counter,
                                     float const *restrict const *const src,
                                     size_t const samples,
                                     float const gain_db);

void aux_channel_list_mix(struct aux_channel_list const *const acl,
                          size_t const counter,
                          size_t const samples,
                          float *restrict const *const mixbuf,
                          float *restrict const *const tmpbuf);

void aux_channel_list_gc(struct aux_channel_list *const acl, size_t const counter);
void aux_channel_list_reset(struct aux_channel_list const *const acl);

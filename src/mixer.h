#pragma once

#include "ovbase.h"

#include "aux_channel.h"
#include "channel.h"

struct mixer;

enum mixer_channel_type {
  mixer_channel_type_channel,
  mixer_channel_type_aux_channel,
  mixer_channel_type_other,
};

typedef void (*mixer_output_notify_func)(void *const userdata,
                                         struct mixer *const m,
                                         int const channel_type,
                                         int const id,
                                         float const *restrict const *const buf,
                                         size_t const channels,
                                         size_t const samples);

NODISCARD error mixer_create(struct mixer **const mp);
NODISCARD error mixer_destroy(struct mixer **const mp);

NODISCARD error mixer_set_format(struct mixer *const m,
                                 float const sample_rate,
                                 size_t const channels,
                                 size_t const samples_per_frame);

NODISCARD error mixer_get_channel_parameter_str(struct mixer *const m,
                                                int const channel_id,
                                                struct channel_effect_params_str *dest,
                                                bool *const found);

NODISCARD error mixer_update_channel(struct mixer *const m,
                                     int const channel_id,
                                     struct channel_effect_params const *e,
                                     int16_t const *restrict const src,
                                     size_t const samples,
                                     bool *const updated);

NODISCARD error mixer_update_aux_channel(struct mixer *const m,
                                         int const aux_channel_id,
                                         struct aux_channel_effect_params const *e,
                                         bool *const updated);

void mixer_mix(struct mixer *const m, int16_t *restrict const buffer, size_t const samples);

void mixer_reset(struct mixer *const m);

bool mixer_get_warming(struct mixer const *const m);
void mixer_set_warming(struct mixer *const m, bool const warming);
float mixer_get_warming_up_duration(struct mixer const *const m);
float mixer_get_sample_rate(struct mixer const *const m);
size_t mixer_get_channels(struct mixer const *const m);
uint64_t mixer_get_position(struct mixer const *const m);
void mixer_set_userdata(struct mixer *const m, void *const userdata);
void mixer_set_output_notify_callback(struct mixer *const m, mixer_output_notify_func f);

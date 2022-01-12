#pragma once

#include "ovbase.h"

struct channel_effect_params {
  float pre_gain;
  float lagger_duration;
  float low_shelf_frequency;
  float low_shelf_gain;
  float high_shelf_frequency;
  float high_shelf_gain;
  float dynamics_threshold;
  float dynamics_ratio;
  float dynamics_attack;
  float dynamics_release;
  int aux_send_id;
  float aux_send;
  float post_gain;
  float pan;
};

struct channel_effect_params_str {
  NATIVE_CHAR pre_gain[16];
  NATIVE_CHAR lagger_duration[16];
  NATIVE_CHAR low_shelf_frequency[16];
  NATIVE_CHAR low_shelf_gain[16];
  NATIVE_CHAR high_shelf_frequency[16];
  NATIVE_CHAR high_shelf_gain[16];
  NATIVE_CHAR dynamics_threshold[16];
  NATIVE_CHAR dynamics_ratio[16];
  NATIVE_CHAR dynamics_attack[16];
  NATIVE_CHAR dynamics_release[16];
  NATIVE_CHAR aux_send[16];
  NATIVE_CHAR post_gain[16];
  NATIVE_CHAR pan[16];
};

typedef void (*channel_notify_func)(void *const userdata,
                                    int const id,
                                    float const *restrict const *const buf,
                                    size_t const channels,
                                    size_t const samples);

typedef NODISCARD error (*channel_write_to_send_func)(void *const userdata,
                                                      int const send_id,
                                                      size_t const counter,
                                                      float const *restrict const *const src,
                                                      size_t const samples,
                                                      float const gain_db);

struct channel_list;

NODISCARD error channel_list_create(struct channel_list **const clp);
NODISCARD error channel_list_destroy(struct channel_list **const clp);

void channel_list_set_userdata(struct channel_list *const cl, void *const userdata);
void channel_list_set_notify_callback(struct channel_list *const cl, channel_notify_func f);
void channel_list_set_write_to_send_target_callback(struct channel_list *const cl, channel_write_to_send_func f);

NODISCARD error channel_list_set_format(struct channel_list const *const cl,
                                        float const sample_rate,
                                        size_t const channels,
                                        bool *const updated);

float channel_list_get_longest_lookahead_duration(struct channel_list const *const cl);

NODISCARD error channel_list_get_effect_params_str(struct channel_list const *const cl,
                                                   int const id,
                                                   struct channel_effect_params_str *params,
                                                   bool *const found);

NODISCARD error channel_list_channel_update(struct channel_list *const cl,
                                            int const id,
                                            size_t const counter,
                                            struct channel_effect_params const *e,
                                            int16_t const *restrict const src,
                                            size_t const samples,
                                            bool *const updated);

void channel_list_mix(struct channel_list const *const cl,
                      size_t const counter,
                      size_t const samples,
                      float *restrict const *const mixbuf,
                      float *restrict const *const chbuf,
                      float *restrict const *const tmpbuf);

void channel_list_gc(struct channel_list *const cl, size_t const counter);
void channel_list_reset(struct channel_list const *const cl);

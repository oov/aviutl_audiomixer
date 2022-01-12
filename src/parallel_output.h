#pragma once

#include "ovbase.h"

struct parallel_output;

enum parallel_output_bit_format {
  parallel_output_bit_format_int16 = 16,
  parallel_output_bit_format_float32 = -32,
};

static inline size_t get_bit_size(int const bit_format) {
  return bit_format == parallel_output_bit_format_float32 ? sizeof(float) : sizeof(int16_t);
}

NODISCARD error parallel_output_create(struct parallel_output **pp,
                                       struct wstr const *const filename,
                                       int const bit_format);
NODISCARD error parallel_output_destroy(struct parallel_output **pp);

NODISCARD error parallel_output_write(struct parallel_output *p,
                                      int const channel_type,
                                      int const id,
                                      float const *restrict const *const buf,
                                      size_t const sample_rate,
                                      size_t const channels,
                                      uint64_t const pos,
                                      size_t const samples);
NODISCARD error parallel_output_finalize(struct parallel_output *p, uint64_t const pos);
NODISCARD error parallel_output_cancel(struct parallel_output *p);

#pragma once

#include "ovbase.h"

struct circbuffer_i16;

NODISCARD error circbuffer_i16_create(struct circbuffer_i16 **const cp);
NODISCARD error circbuffer_i16_destroy(struct circbuffer_i16 **const cp);

NODISCARD error circbuffer_i16_set_buffer_size(struct circbuffer_i16 *const c, size_t const buffer_size);

void circbuffer_i16_clear(struct circbuffer_i16 *const c);

NODISCARD error circbuffer_i16_set_channels(struct circbuffer_i16 *const c, size_t const channels);
size_t circbuffer_i16_get_channels(struct circbuffer_i16 const *const c);
size_t circbuffer_i16_get_remain(struct circbuffer_i16 const *const c);

NODISCARD error circbuffer_i16_write(struct circbuffer_i16 *const c,
                                     int16_t const *restrict const src,
                                     size_t const samples);
NODISCARD error circbuffer_i16_write_offset(struct circbuffer_i16 *const c,
                                            int16_t const *restrict const src,
                                            size_t const samples,
                                            size_t const offset);
NODISCARD error circbuffer_i16_write_silence(struct circbuffer_i16 *const c, size_t const samples);
NODISCARD error circbuffer_i16_write_nogrow(struct circbuffer_i16 *const c,
                                            int16_t const *restrict const src,
                                            size_t const samples);

NODISCARD error circbuffer_i16_read(struct circbuffer_i16 *const c,
                                    int16_t *restrict const dest,
                                    size_t const samples,
                                    size_t *const written);
NODISCARD error circbuffer_i16_read_as_float(struct circbuffer_i16 *const c,
                                             float *restrict const *const dest,
                                             size_t const samples,
                                             float const mul,
                                             size_t *const written);
NODISCARD error circbuffer_i16_discard(struct circbuffer_i16 *const c, size_t const samples, size_t *const discarded);

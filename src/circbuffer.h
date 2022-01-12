#pragma once

#include "ovbase.h"

struct circbuffer;

NODISCARD error circbuffer_create(struct circbuffer **const cp);
NODISCARD error circbuffer_destroy(struct circbuffer **const cp);

NODISCARD error circbuffer_set_buffer_size(struct circbuffer *const c, size_t const buffer_size);

void circbuffer_clear(struct circbuffer *const c);

NODISCARD error circbuffer_set_channels(struct circbuffer *const c, size_t const channels);
size_t circbuffer_get_channels(struct circbuffer const *const c);
size_t circbuffer_get_remain(struct circbuffer const *const c);

NODISCARD error circbuffer_write(struct circbuffer *const c,
                                 float const *restrict const *const src,
                                 size_t const samples);
NODISCARD error circbuffer_write_offset(struct circbuffer *const c,
                                        float const *restrict const *const src,
                                        size_t const samples,
                                        size_t const offset);
NODISCARD error circbuffer_write_silence(struct circbuffer *const c, size_t const samples);

NODISCARD error circbuffer_read(struct circbuffer *const c,
                                float *restrict const *const dest,
                                size_t const samples,
                                size_t *const written);
NODISCARD error circbuffer_discard(struct circbuffer *const c, size_t const samples, size_t *const discarded);

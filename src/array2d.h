#pragma once

#include "ovbase.h"

struct array2d {
  float **ptr;
  size_t channels;
  size_t buffer_size;
};

NODISCARD error array2d_allocate(struct array2d *const c, size_t const channels, size_t const buffer_size);
void array2d_release(struct array2d *const c);

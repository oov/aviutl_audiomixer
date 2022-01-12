#pragma once

#include "ovbase.h"

struct lagger;

NODISCARD error lagger_create(struct lagger **const lp);
NODISCARD error lagger_destroy(struct lagger **const lp);

void lagger_clear(struct lagger *const l);

float lagger_get_duration(struct lagger const *const l);
void lagger_get_duration_str(struct lagger const *const l, NATIVE_CHAR dest[16]); // by msecs

void lagger_set_format(struct lagger *const l, float const sample_rate, size_t const channels);
void lagger_set_duration(struct lagger *const l, float const duration);
NODISCARD error lagger_update_internal_parameter(struct lagger *const l, bool *const updated);

void lagger_process(struct lagger *const l,
                    float const *restrict const *const inputs,
                    float *restrict const *const outputs,
                    size_t const samples);

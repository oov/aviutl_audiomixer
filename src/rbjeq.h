
#include "ovbase.h"

struct rbjeq;

enum rbjeq_type {
  rbjeq_type_low_pass,
  rbjeq_type_high_pass,
  rbjeq_type_band_pass,
  rbjeq_type_notch,
  rbjeq_type_low_shelf,
  rbjeq_type_high_shelf,
  rbjeq_type_peaking,
  rbjeq_type_all_pass,
};

NODISCARD error rbjeq_create(struct rbjeq **const eqp);
NODISCARD error rbjeq_destroy(struct rbjeq **const eqp);

void rbjeq_set_format(struct rbjeq *const eq, float const sample_rate, size_t const channels);
void rbjeq_set_type(struct rbjeq *const eq, int const v);
void rbjeq_set_frequency(struct rbjeq *const eq, float const v);
void rbjeq_get_frequency_str(struct rbjeq *const eq, NATIVE_CHAR dest[16]);

void rbjeq_set_q(struct rbjeq *const eq, float const v);
float rbjeq_get_gain(struct rbjeq const *const eq);
void rbjeq_set_gain(struct rbjeq *const eq, float const v);
void rbjeq_get_gain_str(struct rbjeq *const eq, NATIVE_CHAR dest[16]);

NODISCARD error rbjeq_update_internal_parameter(struct rbjeq *const eq, bool *const updated);

float rbjeq_get_lookahead_duration(struct rbjeq *const eq);

void rbjeq_process(struct rbjeq *const eq,
                   float const *restrict const *const inputs,
                   float *restrict const *const outputs,
                   size_t const samples);
void rbjeq_clear(struct rbjeq *const eq);

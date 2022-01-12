// This code is based on UXFDReverb.
// https://github.com/khoin/UXFDReverb
//
// In jurisdictions that recognize copyright laws, this software is to
// be released into the public domain.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// THE AUTHOR(S) SHALL NOT BE LIABLE FOR ANYTHING, ARISING FROM, OR IN
// CONNECTION WITH THE SOFTWARE OR THE DISTRIBUTION OF THE SOFTWARE.
#pragma once

#include "ovbase.h"

struct uxfdreverb;

NODISCARD error uxfdreverb_create(struct uxfdreverb **const rp);
NODISCARD error uxfdreverb_destroy(struct uxfdreverb **const rp);

void uxfdreverb_set_format(struct uxfdreverb *const r, float const sample_rate, size_t const channels);
void uxfdreverb_set_pre_delay(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_band_width(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_diffuse(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_decay(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_damping(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_excursion(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_wet(struct uxfdreverb *const r, float const v);
void uxfdreverb_set_dry(struct uxfdreverb *const r, float const v);

NODISCARD error uxfdreverb_update_internal_parameter(struct uxfdreverb *const r, bool *const updated);

void uxfdreverb_process(struct uxfdreverb *const r,
                        float const *restrict const *const inputs,
                        float *restrict const *const outputs,
                        size_t const samples);
void uxfdreverb_clear(struct uxfdreverb *const r);

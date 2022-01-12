// This code is based on mdaDynamics.cpp.
// https://sourceforge.net/projects/mda-vst/
//
// mda VST plug-ins
//
// Copyright (c) 2008 Paul Kellett
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#pragma once

#include "ovbase.h"

struct dynamics;

NODISCARD error dynamics_create(struct dynamics **const dp);
NODISCARD error dynamics_destroy(struct dynamics **const dp);

void dynamics_set_format(struct dynamics *const d, float const sample_rate, size_t const channels);
void dynamics_set_thresh(struct dynamics *const d, float const v);
float dynamics_get_ratio(struct dynamics const *const d);
void dynamics_set_ratio(struct dynamics *const d, float const v);
void dynamics_set_output(struct dynamics *const d, float const v);
void dynamics_set_attack(struct dynamics *const d, float const v);
void dynamics_set_release(struct dynamics *const d, float const v);
void dynamics_set_limiter(struct dynamics *const d, float const v);
void dynamics_set_gate_thresh(struct dynamics *const d, float const v);
void dynamics_set_gate_attack(struct dynamics *const d, float const v);
void dynamics_set_gate_decay(struct dynamics *const d, float const v);
void dynamics_set_fx_mix(struct dynamics *const d, float const v);

void dynamics_update_internal_parameter(struct dynamics *const d, bool *const updated);

float dynamics_get_attack_duration(struct dynamics const *const d);
float dynamics_get_release_duration(struct dynamics const *const d);

void dynamics_get_attack_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_fx_mix_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_gate_attack_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_gate_decay_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_gate_threshold_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_output_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_limiter_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_ratio_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_release_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);
void dynamics_get_threshold_str(struct dynamics const *const d, NATIVE_CHAR dest[16]);

void dynamics_process(struct dynamics *const d,
                      float const *restrict const *const inputs,
                      float *restrict const *const outputs,
                      size_t const samples);
void dynamics_clear(struct dynamics *const d);

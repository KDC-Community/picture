/*
 * File: pictureosc.cpp
 */

#include "userosc.h"

#if 1
#include "figure.h"
#else
#define VERTICES 6
#define WAVETABLES 1
static float figure_x[WAVETABLES][VERTICES] = {
    {1.0, 0.5, 0.0, 0.5, 1.0, 0.5}
};
static float figure_y[WAVETABLES][VERTICES] = {
    {0.0, 0.0, 0.5, 1.0, 0.5, 0.0}
};
#endif

#define BUFSIZE (VERTICES*4)

static q31_t wavetable[WAVETABLES][BUFSIZE];

typedef struct State {
    float w0;
    float phase;
    float lfo, lfoz;
    float shape;
    uint8_t table;
    uint8_t interpolation;
    uint8_t flags;
} State;

static State s_state;

enum {
    k_inter_lin = 0,
    k_inter_cos,
    k_nointer,
};

enum {
    k_flags_none = 0,
    k_flag_reset = 1<<0,
};

static void init_wavetable(q31_t *table, const int vertices, float *vx,  float *vy) {
    q31_t x, y;
    for(int i = 0; i < vertices; i++) {
        x = f32_to_q31(vx[i] / 2);
        y = f32_to_q31(vy[i] / 2 + 0.5);
        table[i]                = y;
        table[i + vertices]     = 0x7fffffff - x;
        table[i + 2 * vertices] = 0x7fffffff - y;
        table[i + 3 * vertices] = x;
    }
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    s_state.w0    = 0.f;
    s_state.phase = 0.f;
    s_state.lfo = s_state.lfoz = 0.f;
    s_state.interpolation = k_inter_lin;
    s_state.flags = k_flags_none;
    for (int i = 0; i < WAVETABLES; i++) {
        init_wavetable(wavetable[i], VERTICES, figure_x[i], figure_y[i]);
    }
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
    const uint8_t flags = s_state.flags;
    s_state.flags = k_flags_none;

    const float w0 = s_state.w0 = osc_w0f_for_note((params->pitch)>>8, params->pitch & 0xFF);
    float phase = (flags & k_flag_reset) ? 0.f : s_state.phase;
  
    const float lfo = s_state.lfo = q31_to_f32(params->shape_lfo);
    float lfoz = (flags & k_flag_reset) ? lfo : s_state.lfoz;
    const float lfo_inc = (lfo - lfoz) / frames;
    const int img = s_state.table;

    q31_t * __restrict y = (q31_t *) yn;
    const q31_t * y_e = y + frames;

    for (; y != y_e; ) {
        float pos = phase * BUFSIZE;
        uint32_t idx = (uint32_t) pos;
        const float frac = pos - idx;

        const float m = q31_to_f32(wavetable[img][idx++]);
        const float n = q31_to_f32(wavetable[img][(idx == BUFSIZE) ? 0 : idx]);
        float sig;
        switch(s_state.interpolation) {
            case k_inter_cos:
                sig = cosintf(frac, m, n);
                break;
            case k_nointer:
                sig = m;
                break;
        default:
            sig = linintf(frac, m, n);
        }

        float pwm = phase + 0.75;
        pwm -= (uint32_t) pwm;
        if (pwm < (s_state.shape + lfoz)) {
            sig = 0.5f;
        }

        *(y++) = f32_to_q31(sig);
        phase += w0;
        phase -= (uint32_t) phase;
        lfoz += lfo_inc;
    }
    s_state.phase = phase;
    s_state.lfoz = lfoz;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
    s_state.flags |= k_flag_reset;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
    const float valf = param_val_to_f32(value);
    switch (index) {
    case k_user_osc_param_id1:
        s_state.interpolation = value;
        break;
    case k_user_osc_param_id2:
    case k_user_osc_param_id3:
    case k_user_osc_param_id4:
    case k_user_osc_param_id5:
    case k_user_osc_param_id6:
    case k_user_osc_param_shape:
        s_state.shape = valf;
        break;
    case k_user_osc_param_shiftshape:
        s_state.table = 0.99 * valf * WAVETABLES;
    default:
        break;
  }
}

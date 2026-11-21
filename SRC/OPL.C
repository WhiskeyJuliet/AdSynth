#include "opl.h"
#include <conio.h>
#include <stdlib.h>

static uint16_t g_oplPort = OPL_BASE_PORT;

static const uint8_t g_modOffsets[9] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12
};
static const uint8_t g_carOffsets[9] = {
    0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15
};

static const uint16_t g_fNums[12] = {
    0x157, 0x16B, 0x181, 0x198, 0x1B0, 0x1CA,
    0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287
};

static void OPL_Delay(int count) {
    int i;
    for (i = 0; i < count; ++i) {
        inp(g_oplPort);
    }
}

void OPL_Write(uint8_t reg, uint8_t val) {
    outp(g_oplPort, reg);
    OPL_Delay(6);
    outp(g_oplPort + 1, val);
    OPL_Delay(35);
}

void OPL_Reset(void) {
    int i;
    for (i = 1; i <= 0xF5; ++i) {
        OPL_Write(i, 0x00);
    }
}

void OPL_Init(uint16_t basePort) {
    g_oplPort = basePort;
    OPL_Reset();
    OPL_Write(0x01, 0x20); /* Enable Waveform Select */
}

void OPL_Shutdown(void) {
    OPL_Reset();
}

void OPL_SetInstrument(uint8_t ch, const OPL_Instrument *inst) {
    uint8_t mod, car;
    if (ch >= 9 || !inst) return;

    mod = g_modOffsets[ch];
    car = g_carOffsets[ch];

    OPL_Write(0x20 + mod, inst->mod_mult_vib_am);
    OPL_Write(0x40 + mod, inst->mod_ksl_level);
    OPL_Write(0x60 + mod, inst->mod_attack_decay);
    OPL_Write(0x80 + mod, inst->mod_sustain_rel);
    OPL_Write(0xE0 + mod, inst->mod_wave);

    OPL_Write(0x20 + car, inst->car_mult_vib_am);
    OPL_Write(0x40 + car, inst->car_ksl_level);
    OPL_Write(0x60 + car, inst->car_attack_decay);
    OPL_Write(0x80 + car, inst->car_sustain_rel);
    OPL_Write(0xE0 + car, inst->car_wave);

    OPL_Write(0xC0 + ch, inst->feedback_conn);
}

/* Force envelope generator to sustain continuously for Let Ring and Pitch Bend */
void OPL_SetInstrumentSustained(uint8_t ch, const OPL_Instrument *inst) {
    uint8_t mod, car;
    uint8_t carSL, carRR;
    if (ch >= 9 || !inst) return;

    mod = g_modOffsets[ch];
    car = g_carOffsets[ch];

    /* Modulator: EG-TYP = 1 (bit 5) so it holds sustain */
    OPL_Write(0x20 + mod, (uint8_t)(inst->mod_mult_vib_am | 0x20));
    OPL_Write(0x40 + mod, inst->mod_ksl_level);
    OPL_Write(0x60 + mod, inst->mod_attack_decay);
    OPL_Write(0x80 + mod, inst->mod_sustain_rel);
    OPL_Write(0xE0 + mod, inst->mod_wave);

    /* Carrier: EG-TYP = 1 (bit 5) and keep sustain audible */
    OPL_Write(0x20 + car, (uint8_t)(inst->car_mult_vib_am | 0x20));
    OPL_Write(0x40 + car, inst->car_ksl_level);
    OPL_Write(0x60 + car, inst->car_attack_decay);

    carSL = inst->car_sustain_rel & 0xF0;
    carRR = inst->car_sustain_rel & 0x0F;
    if (carSL >= 0x70) carSL = 0x10;

    OPL_Write(0x80 + car, (uint8_t)(carSL | carRR));
    OPL_Write(0xE0 + car, inst->car_wave);

    OPL_Write(0xC0 + ch, inst->feedback_conn);
}

void OPL_NoteOn(uint8_t ch, uint8_t note, uint8_t octave) {
    uint16_t fnum;
    uint8_t noteIndex;
    if (ch >= 9) return;

    noteIndex = note % 12;
    fnum = g_fNums[noteIndex];

    OPL_Write(0xB0 + ch, 0x00);
    OPL_Write(0xA0 + ch, (uint8_t)(fnum & 0xFF));
    OPL_Write(0xB0 + ch, (uint8_t)(0x20 | ((octave & 0x07) << 2) | ((fnum >> 8) & 0x03)));
}

void OPL_NoteOff(uint8_t ch) {
    if (ch >= 9) return;
    OPL_Write(0xB0 + ch, 0x00);
}

void OPL_SetPitch(uint8_t ch, int32_t pitch100) {
    int32_t s, f;
    int oct, semi;
    uint32_t f1, f2, fnum;

    if (ch >= 9) return;
    if (pitch100 < 1200) pitch100 = 1200;
    if (pitch100 > 9500) pitch100 = 9500;

    s = pitch100 / 100;
    f = pitch100 % 100;
    oct = (int)(s / 12);
    semi = (int)(s % 12);

    if (semi < 11) {
        f1 = g_fNums[semi];
        f2 = g_fNums[semi + 1];
        fnum = f1 + ((f2 - f1) * f) / 100;
    } else {
        f1 = g_fNums[11];
        f2 = (uint32_t)g_fNums[0] * 2;
        fnum = f1 + ((f2 - f1) * f) / 100;
    }

    if (fnum >= 0x2AE) {
        fnum >>= 1;
        oct++;
    }

    if (oct > 7) oct = 7;
    if (fnum > 0x3FF) fnum = 0x3FF;

    OPL_Write(0xA0 + ch, (uint8_t)(fnum & 0xFF));
    OPL_Write(0xB0 + ch, (uint8_t)(0x20 | ((oct & 0x07) << 2) | ((fnum >> 8) & 0x03)));
}

int OPL_FNumToNote(uint16_t fnum, uint8_t *outNote) {
    int i, bestIdx = 0;
    int minDiff = 9999;

    fnum &= 0x3FF;
    for (i = 0; i < 12; ++i) {
        int diff = abs((int)g_fNums[i] - (int)fnum);
        if (diff < minDiff) {
            minDiff = diff;
            bestIdx = i;
        }
    }
    *outNote = (uint8_t)(bestIdx + 1);
    return 1;
}

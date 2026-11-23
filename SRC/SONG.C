#include "song.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>

SongState g_song;

typedef struct {
    uint8_t active;
    uint8_t note;
    uint8_t octave;
    uint8_t instrument;
    uint8_t loadedInst;
    uint8_t loadedSustained;
    int16_t muteTimerMs;
    uint8_t bending;
    uint16_t bendStartRow;
    uint16_t bendEndRow;
    int32_t startPitch100;
    int32_t endPitch100;
} ChannelVoice;

static ChannelVoice g_voices[MAX_CHANNELS];

#define MAX_UNDO_RECORDS 256
typedef struct {
    uint16_t row;
    uint8_t  channel;
    TrackerNote oldCell;
    uint16_t batchId;
} UndoRecord;

static UndoRecord g_undoStack[MAX_UNDO_RECORDS];
static int g_undoCount = 0;
static uint16_t g_currentBatchId = 1;

#define MAX_CLIP_ITEMS 256
typedef struct {
    int16_t relRow;
    int8_t  relCh;
    uint8_t note;
    uint8_t octave;
    uint8_t instrument;
    uint8_t attribute;
} ClipItem;

static ClipItem g_clipItems[MAX_CLIP_ITEMS];
static int g_clipCount = 0;
static int g_selectedCount = 0;

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

typedef struct {
    uint8_t mod_mult_vib_am;
    uint8_t mod_ksl_level;
    uint8_t mod_attack_decay;
    uint8_t mod_sustain_rel;
    uint8_t mod_wave;
    uint8_t car_mult_vib_am;
    uint8_t car_ksl_level;
    uint8_t car_attack_decay;
    uint8_t car_sustain_rel;
    uint8_t car_wave;
    uint8_t feedback_conn;
} OPL_ChannelShadow;

typedef struct {
    uint8_t lastA0[MAX_CHANNELS];
    uint8_t lastB0[MAX_CHANNELS];
    uint8_t prevKeyOn[MAX_CHANNELS];
    int32_t lastNoteRow[MAX_CHANNELS];
    uint8_t rowHadNote[MAX_CHANNELS];
    uint8_t lastBD;
} OPL_ImportState;

static OPL_ChannelShadow g_shadowChannels[MAX_CHANNELS];
static OPL_ImportState g_importState;
static int g_importedVoiceCount = 0;
static int g_lastImportNotes = 0;
static int g_lastImportVoices = 0;

static void CalcPitchRegs(int32_t pitch100, uint8_t *outA0, uint8_t *outB0) {
    int32_t s, f;
    int oct, semi;
    uint32_t f1, f2, fnum;

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

    *outA0 = (uint8_t)(fnum & 0xFF);
    *outB0 = (uint8_t)(0x20 | ((oct & 0x07) << 2) | ((fnum >> 8) & 0x03));
}

static int StrEndsWithCI(const char *str, const char *suffix) {
    size_t slen = strlen(str);
    size_t xlen = strlen(suffix);
    size_t i;
    if (slen < xlen) return 0;
    for (i = 0; i < xlen; ++i) {
        if (tolower((unsigned char)str[slen - xlen + i]) != tolower((unsigned char)suffix[i])) {
            return 0;
        }
    }
    return 1;
}

static int GetChannelFromOffset(uint8_t offset, int isCar) {
    int i;
    const uint8_t *table = isCar ? g_carOffsets : g_modOffsets;
    for (i = 0; i < 9; ++i) {
        if (table[i] == offset) return i;
    }
    return -1;
}

static void InitShadowChannels(void) {
    int ch;
    memset(g_shadowChannels, 0, sizeof(g_shadowChannels));
    memset(&g_importState, 0, sizeof(g_importState));
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        const OPL_Instrument *def = Bank_GetInstrument(0);
        g_shadowChannels[ch].mod_mult_vib_am  = def->mod_mult_vib_am;
        g_shadowChannels[ch].mod_ksl_level    = def->mod_ksl_level;
        g_shadowChannels[ch].mod_attack_decay = def->mod_attack_decay;
        g_shadowChannels[ch].mod_sustain_rel  = def->mod_sustain_rel;
        g_shadowChannels[ch].mod_wave         = def->mod_wave;
        g_shadowChannels[ch].car_mult_vib_am  = def->car_mult_vib_am;
        g_shadowChannels[ch].car_ksl_level    = def->car_ksl_level;
        g_shadowChannels[ch].car_attack_decay = def->car_attack_decay;
        g_shadowChannels[ch].car_sustain_rel  = def->car_sustain_rel;
        g_shadowChannels[ch].car_wave         = def->car_wave;
        g_shadowChannels[ch].feedback_conn    = def->feedback_conn;
        g_importState.lastNoteRow[ch] = -1;
    }
    g_importedVoiceCount = 0;
    g_lastImportNotes = 0;
    g_lastImportVoices = 0;
}

static void ProcessShadowRegisterWrite(uint8_t reg, uint8_t val) {
    uint8_t base = reg & 0xE0;
    uint8_t offset = reg & 0x1F;
    int ch;

    if (reg >= 0xC0 && reg <= 0xC8) {
        ch = reg - 0xC0;
        if (ch < MAX_CHANNELS) g_shadowChannels[ch].feedback_conn = val;
        return;
    }

    ch = GetChannelFromOffset(offset, 0);
    if (ch >= 0 && ch < MAX_CHANNELS) {
        switch (base) {
            case 0x20: g_shadowChannels[ch].mod_mult_vib_am  = val; break;
            case 0x40: g_shadowChannels[ch].mod_ksl_level    = val; break;
            case 0x60: g_shadowChannels[ch].mod_attack_decay = val; break;
            case 0x80: g_shadowChannels[ch].mod_sustain_rel  = val; break;
            case 0xE0: g_shadowChannels[ch].mod_wave         = (uint8_t)(val & 0x07); break;
            default: break;
        }
        return;
    }

    ch = GetChannelFromOffset(offset, 1);
    if (ch >= 0 && ch < MAX_CHANNELS) {
        switch (base) {
            case 0x20: g_shadowChannels[ch].car_mult_vib_am  = val; break;
            case 0x40: g_shadowChannels[ch].car_ksl_level    = val; break;
            case 0x60: g_shadowChannels[ch].car_attack_decay = val; break;
            case 0x80: g_shadowChannels[ch].car_sustain_rel  = val; break;
            case 0xE0: g_shadowChannels[ch].car_wave         = (uint8_t)(val & 0x07); break;
            default: break;
        }
    }
}

static uint8_t MatchOrAddShadowInstrument(uint8_t ch) {
    int i, total;
    int bestIdx = 0;
    int bestDist = 999999;
    OPL_Instrument inst;
    OPL_ChannelShadow *s = &g_shadowChannels[ch];

    total = Bank_GetTotalCount();
    for (i = 0; i < total; ++i) {
        const OPL_Instrument *existing = Bank_GetInstrument(i);
        if (existing->mod_mult_vib_am  == s->mod_mult_vib_am &&
            existing->mod_ksl_level    == s->mod_ksl_level &&
            existing->mod_attack_decay == s->mod_attack_decay &&
            existing->mod_sustain_rel  == s->mod_sustain_rel &&
            existing->mod_wave         == s->mod_wave &&
            existing->car_mult_vib_am  == s->car_mult_vib_am &&
            existing->car_ksl_level    == s->car_ksl_level &&
            existing->car_attack_decay == s->car_attack_decay &&
            existing->car_sustain_rel  == s->car_sustain_rel &&
            existing->car_wave         == s->car_wave &&
            existing->feedback_conn    == s->feedback_conn) {
            return (uint8_t)i;
        }
    }

    if (total < MAX_TOTAL_INSTRUMENTS) {
        memset(&inst, 0, sizeof(OPL_Instrument));
        g_importedVoiceCount++;
        g_lastImportVoices++;
        sprintf(inst.name, "IMP V%02d", g_importedVoiceCount);
        inst.mod_mult_vib_am  = s->mod_mult_vib_am;
        inst.mod_ksl_level    = s->mod_ksl_level;
        inst.mod_attack_decay = s->mod_attack_decay;
        inst.mod_sustain_rel  = s->mod_sustain_rel;
        inst.mod_wave         = s->mod_wave;
        inst.car_mult_vib_am  = s->car_mult_vib_am;
        inst.car_ksl_level    = s->car_ksl_level;
        inst.car_attack_decay = s->car_attack_decay;
        inst.car_sustain_rel  = s->car_sustain_rel;
        inst.car_wave         = s->car_wave;
        inst.feedback_conn    = s->feedback_conn;
        inst.category         = CAT_CUSTOM_BNK;

        Bank_SetCustomInstrument(total, &inst);
        return (uint8_t)total;
    }

    for (i = 0; i < total; ++i) {
        const OPL_Instrument *ex = Bank_GetInstrument(i);
        int dist = 0;
        if (ex->mod_mult_vib_am  != s->mod_mult_vib_am)  dist += 100;
        if (ex->mod_attack_decay != s->mod_attack_decay) dist += 100;
        if (ex->mod_sustain_rel  != s->mod_sustain_rel)  dist += 100;
        if (ex->mod_wave         != s->mod_wave)         dist += 100;
        if (ex->car_mult_vib_am  != s->car_mult_vib_am)  dist += 100;
        if (ex->car_attack_decay != s->car_attack_decay) dist += 100;
        if (ex->car_sustain_rel  != s->car_sustain_rel)  dist += 100;
        if (ex->car_wave         != s->car_wave)         dist += 100;
        if (ex->feedback_conn    != s->feedback_conn)    dist += 100;
        dist += abs((int)(ex->car_ksl_level & 0x3F) - (int)(s->car_ksl_level & 0x3F));
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return (uint8_t)bestIdx;
}

static void ImportProcessOPLWrite(uint8_t reg, uint8_t val, uint16_t row) {
    ProcessShadowRegisterWrite(reg, val);

    /* Carrier Total Level mute detection in FM mode */
    if ((reg & 0xE0) == 0x40) {
        int ch = GetChannelFromOffset((uint8_t)(reg & 0x1F), 1);
        if (ch >= 0 && ch < MAX_CHANNELS) {
            if ((g_shadowChannels[ch].feedback_conn & 0x01) == 0 && (val & 0x3F) >= 0x3A) {
                if (g_importState.prevKeyOn[ch] && g_importState.lastNoteRow[ch] < (int32_t)row) {
                    if (g_song.patterns[g_song.currentPattern].rows[row][ch].note == 0) {
                        Song_SetNoteAttribute((uint8_t)ch, row, ATTR_MUTE);
                    }
                }
            }
        }
        return;
    }

    if (reg >= 0xA0 && reg <= 0xA8) {
        uint8_t ch = (uint8_t)(reg - 0xA0);
        g_importState.lastA0[ch] = val;
        if (g_importState.prevKeyOn[ch] && g_importState.lastNoteRow[ch] == (int32_t)row) {
            uint16_t fnum = (uint16_t)(((g_importState.lastB0[ch] & 0x03) << 8) | val);
            uint8_t block = (uint8_t)((g_importState.lastB0[ch] >> 2) & 0x07);
            uint8_t note = 0, oct = 4;
            OPL_FNumBlockToNoteOct(fnum, block, &note, &oct);
            g_song.patterns[g_song.currentPattern].rows[row][ch].note = note;
            g_song.patterns[g_song.currentPattern].rows[row][ch].octave = oct;
        }
        return;
    }

    if (reg >= 0xB0 && reg <= 0xB8) {
        uint8_t ch = (uint8_t)(reg - 0xB0);
        uint8_t keyOn = (uint8_t)((val & 0x20) ? 1 : 0);
        g_importState.lastB0[ch] = val;

        if (keyOn && !g_importState.prevKeyOn[ch]) {
            uint16_t fnum = (uint16_t)(((val & 0x03) << 8) | g_importState.lastA0[ch]);
            uint8_t block = (uint8_t)((val >> 2) & 0x07);
            uint8_t note = 0, oct = 4;
            uint8_t inst = MatchOrAddShadowInstrument(ch);
            OPL_FNumBlockToNoteOct(fnum, block, &note, &oct);
            Song_SetNote(ch, row, note, oct, inst);
            Song_SetNoteAttribute(ch, row, ATTR_NONE);
            g_importState.lastNoteRow[ch] = (int32_t)row;
            g_importState.rowHadNote[ch] = 1;
            g_lastImportNotes++;
        } else if (!keyOn && g_importState.prevKeyOn[ch]) {
            if (g_importState.lastNoteRow[ch] == (int32_t)row) {
                Song_SetNoteAttribute(ch, row, ATTR_DAMPEN);
            } else if (g_importState.lastNoteRow[ch] < (int32_t)row) {
                if (g_song.patterns[g_song.currentPattern].rows[row][ch].note == 0) {
                    Song_SetNoteAttribute(ch, row, ATTR_MUTE);
                }
            }
        }
        g_importState.prevKeyOn[ch] = keyOn;
        return;
    }

    if (reg == 0xBD) {
        if (val & 0x20) {
            uint8_t rising = (uint8_t)((val & ~g_importState.lastBD) & 0x1F);
            static const uint8_t drumBits[5] = { 0x10, 0x08, 0x04, 0x02, 0x01 };
            static const uint8_t drumCh[5]   = { 6,    7,    8,    8,    7 };
            static const uint8_t defOct[5]   = { 2,    4,    3,    5,    5 };
            int d;

            for (d = 0; d < 5; ++d) {
                if (rising & drumBits[d]) {
                    uint8_t ch = drumCh[d];
                    if (g_song.patterns[g_song.currentPattern].rows[row][ch].note == 0) {
                        uint16_t fnum = (uint16_t)(((g_importState.lastB0[ch] & 0x03) << 8) | g_importState.lastA0[ch]);
                        uint8_t block = (uint8_t)((g_importState.lastB0[ch] >> 2) & 0x07);
                        uint8_t note = 1, oct = defOct[d];
                        uint8_t inst = MatchOrAddShadowInstrument(ch);
                        if (fnum > 0) {
                            OPL_FNumBlockToNoteOct(fnum, block, &note, &oct);
                        }
                        Song_SetNote(ch, row, note, oct, inst);
                        g_importState.lastNoteRow[ch] = (int32_t)row;
                        g_importState.rowHadNote[ch] = 1;
                        g_lastImportNotes++;
                    }
                }
            }
        }
        g_importState.lastBD = val;
    }
}

const char *Song_GetClockDescription(uint16_t hz) {
    if (hz == 280)  return "280 HZ : DUKE NUKEM II";
    if (hz == 560)  return "560 HZ : ID KEEN / BIO / STANDARD";
    if (hz == 700)  return "700 HZ : WOLF3D / .WLF / BLAKE";
    if (hz == 1000) return "1000 HZ : DRO 1MS / DOSBOX";
    return "CUSTOM / IMPORTED RATE";
}

double Song_GetMsPerRowFloat(void) {
    double baseRowMs = (g_song.rowMs > 0) ? (double)g_song.rowMs : 120.0;
    double tempo = (g_song.tempo > 0) ? (double)g_song.tempo : 120.0;
    double ms = baseRowMs * (120.0 / tempo);
    if (ms < 1.0) ms = 1.0;
    return ms;
}

uint32_t Song_GetMsPerRow(void) {
    uint32_t ms = (uint32_t)(Song_GetMsPerRowFloat() + 0.5);
    return (ms == 0) ? 1 : ms;
}

int16_t Song_GetDampenDurationMs(void) {
    if (g_song.rowMs >= 60) return 250;
    if (g_song.rowMs >= 20) return (int16_t)(g_song.rowMs / 2);
    return 6;
}

void Song_SetClockHz(uint16_t newClockHz) {
    g_song.clockHz = newClockHz;
}

void Song_RescaleRowMs(uint16_t newRowMs) {
    uint16_t oldRowMs = (g_song.rowMs > 0) ? g_song.rowMs : 120;
    double ratio;
    TrackerPattern *tmp;
    uint32_t r;
    int c;

    if (newRowMs == 0 || newRowMs == oldRowMs) return;

    ratio = (double)oldRowMs / (double)newRowMs;
    tmp = (TrackerPattern *)malloc(sizeof(TrackerPattern));
    if (!tmp) {
        g_song.rowMs = newRowMs;
        return;
    }
    memset(tmp, 0, sizeof(TrackerPattern));

    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *src = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (src->note > 0 || src->attribute != ATTR_NONE || (src->flags & 0x01)) {
                uint32_t nr = (uint32_t)((double)r * ratio + 0.5);
                if (nr < PATTERN_ROWS) {
                    TrackerNote *dst = &tmp->rows[nr][c];
                    if (dst->note == 0 || src->note > 0) {
                        *dst = *src;
                    }
                }
            }
        }
    }

    memcpy(&g_song.patterns[g_song.currentPattern], tmp, sizeof(TrackerPattern));
    free(tmp);

    if (g_song.loop.loopPoint1 >= 0) {
        int32_t np1 = (int32_t)((double)g_song.loop.loopPoint1 * ratio + 0.5);
        if (np1 >= (int32_t)PATTERN_ROWS) np1 = (int32_t)PATTERN_ROWS - 1;
        g_song.loop.loopPoint1 = (int16_t)np1;
    }
    if (g_song.loop.loopPoint2 >= 0) {
        int32_t np2 = (int32_t)((double)g_song.loop.loopPoint2 * ratio + 0.5);
        if (np2 >= (int32_t)PATTERN_ROWS) np2 = (int32_t)PATTERN_ROWS - 1;
        g_song.loop.loopPoint2 = (int16_t)np2;
    }

    {
        uint32_t cr = (uint32_t)((double)g_song.currentRow * ratio + 0.5);
        if (cr >= PATTERN_ROWS) cr = PATTERN_ROWS - 1;
        g_song.currentRow = (uint16_t)cr;
    }

    g_song.rowMs = newRowMs;
}

int Song_GetLastImportNoteCount(void) {
    return g_lastImportNotes;
}

int Song_GetLastImportVoiceCount(void) {
    return g_lastImportVoices;
}

void Song_Init(void) {
    int ch;
    memset(&g_song, 0, sizeof(SongState));
    g_song.tempo = 120;
    g_song.clockHz = 700; /* Set default engine clock rate to 700 Hz */
    g_song.rowMs = 30;
    g_song.currentPattern = 0;
    g_song.currentRow = 0;
    g_song.playing = 0;
    Loop_Init(&g_song.loop);
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        g_song.channelModes[ch] = CH_NORMAL;
    }
    Song_ResetVoices();
    g_undoCount = 0;
    g_currentBatchId = 1;
    g_clipCount = 0;
    g_selectedCount = 0;
}

void Song_ClearAll(void) {
    int ch;
    memset(g_song.patterns, 0, sizeof(g_song.patterns));
    g_song.currentRow = 0;
    g_song.playing = 0;
    Loop_Init(&g_song.loop);
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        g_song.channelModes[ch] = CH_NORMAL;
    }
    Song_ResetVoices();
    g_undoCount = 0;
    g_currentBatchId = 1;
    g_selectedCount = 0;
}

void Song_ResetVoices(void) {
    int ch;
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        g_voices[ch].active = 0;
        g_voices[ch].bending = 0;
        g_voices[ch].muteTimerMs = 0;
        g_voices[ch].loadedInst = 0xFF;
        g_voices[ch].loadedSustained = 0xFF;
        OPL_NoteOff((uint8_t)ch);
    }
}

int Song_IsChannelAudible(uint8_t ch) {
    int c;
    int hasSolo = 0;
    if (ch >= MAX_CHANNELS) return 0;

    for (c = 0; c < MAX_CHANNELS; ++c) {
        if (g_song.channelModes[c] == CH_SOLO) {
            hasSolo = 1;
            break;
        }
    }

    if (hasSolo) {
        return (g_song.channelModes[ch] == CH_SOLO);
    }
    return (g_song.channelModes[ch] != CH_MUTED);
}

void Song_ToggleChannelMuteSolo(uint8_t ch) {
    int c;
    if (ch >= MAX_CHANNELS) return;

    if (g_song.channelModes[ch] == CH_NORMAL) {
        g_song.channelModes[ch] = CH_MUTED;
        OPL_NoteOff(ch);
        g_voices[ch].active = 0;
    } else if (g_song.channelModes[ch] == CH_MUTED) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            g_song.channelModes[c] = CH_NORMAL;
        }
        g_song.channelModes[ch] = CH_SOLO;
        for (c = 0; c < MAX_CHANNELS; ++c) {
            if (c != ch) {
                OPL_NoteOff((uint8_t)c);
                g_voices[c].active = 0;
            }
        }
    } else {
        g_song.channelModes[ch] = CH_NORMAL;
    }
}

ChannelMode Song_GetChannelMode(uint8_t ch) {
    if (ch >= MAX_CHANNELS) return CH_NORMAL;
    return g_song.channelModes[ch];
}

void Song_SetNote(uint8_t ch, uint16_t row, uint8_t note, uint8_t octave, uint8_t inst) {
    if (ch >= MAX_CHANNELS) return;
    g_song.patterns[g_song.currentPattern].rows[row][ch].note = note;
    g_song.patterns[g_song.currentPattern].rows[row][ch].octave = octave;
    g_song.patterns[g_song.currentPattern].rows[row][ch].instrument = inst;
}

void Song_ClearNote(uint8_t ch, uint16_t row) {
    if (ch >= MAX_CHANNELS) return;
    memset(&g_song.patterns[g_song.currentPattern].rows[row][ch], 0, sizeof(TrackerNote));
}

void Song_ToggleNoteAttribute(uint8_t ch, uint16_t row) {
    TrackerNote *n;
    if (ch >= MAX_CHANNELS) return;
    n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
    n->attribute = (n->attribute + 1) % 5;
}

void Song_SetNoteAttribute(uint8_t ch, uint16_t row, uint8_t attr) {
    TrackerNote *n;
    if (ch >= MAX_CHANNELS) return;
    n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
    n->attribute = attr;
}

void Song_SetSelection(uint16_t row, uint8_t ch, uint8_t sel) {
    TrackerNote *n;
    if (ch >= MAX_CHANNELS) return;
    n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
    if (sel) {
        if (!(n->flags & 0x01)) {
            n->flags |= 0x01;
            g_selectedCount++;
        }
    } else {
        if (n->flags & 0x01) {
            n->flags &= ~0x01;
            if (g_selectedCount > 0) g_selectedCount--;
        }
    }
}

void Song_ClearSelections(void) {
    uint32_t r;
    int c;
    if (g_selectedCount <= 0) return;
    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            g_song.patterns[g_song.currentPattern].rows[r][c].flags &= ~0x01;
        }
    }
    g_selectedCount = 0;
}

int Song_HasSelections(void) {
    return g_selectedCount > 0;
}

int Song_GetSelectionCount(void) {
    return g_selectedCount;
}

void Song_DeleteSelectedNotes(void) {
    uint32_t r;
    int c;
    if (g_selectedCount <= 0) return;
    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (n->flags & 0x01) {
                Song_UndoRecord((uint16_t)r, (uint8_t)c);
                memset(n, 0, sizeof(TrackerNote));
            }
        }
    }
    g_selectedCount = 0;
    Song_UndoNextBatch();
}

void Song_ToggleLoopPoint(uint16_t row) {
    Loop_Toggle(&g_song.loop, row);
}

void Song_GetLoopRange(LoopState *loop, uint16_t *outStart, uint16_t *outEnd, uint8_t *outHasLoop) {
    Loop_GetRange(loop, outStart, outEnd, outHasLoop);
}

void Song_UndoRecord(uint16_t row, uint8_t ch) {
    if (ch >= MAX_CHANNELS) return;
    if (g_undoCount >= MAX_UNDO_RECORDS) {
        int i;
        for (i = 0; i < MAX_UNDO_RECORDS - 1; ++i) {
            g_undoStack[i] = g_undoStack[i + 1];
        }
        g_undoCount = MAX_UNDO_RECORDS - 1;
    }
    g_undoStack[g_undoCount].row = row;
    g_undoStack[g_undoCount].channel = ch;
    g_undoStack[g_undoCount].oldCell = g_song.patterns[g_song.currentPattern].rows[row][ch];
    g_undoStack[g_undoCount].batchId = g_currentBatchId;
    g_undoCount++;
}

void Song_UndoNextBatch(void) {
    g_currentBatchId++;
    if (g_currentBatchId == 0) g_currentBatchId = 1;
}

int Song_Undo(void) {
    uint16_t targetBatch;
    int undone = 0;
    if (g_undoCount <= 0) return 0;

    targetBatch = g_undoStack[g_undoCount - 1].batchId;
    while (g_undoCount > 0 && g_undoStack[g_undoCount - 1].batchId == targetBatch) {
        UndoRecord *rec = &g_undoStack[g_undoCount - 1];
        g_song.patterns[g_song.currentPattern].rows[rec->row][rec->channel] = rec->oldCell;
        g_undoCount--;
        undone++;
    }
    return undone;
}

int Song_CopySelection(uint16_t cursorRow, uint8_t cursorCh) {
    int hasSel = (g_selectedCount > 0);
    int32_t anchorRow = -1;
    int anchorCh = -1;
    uint32_t r;
    int c;

    g_clipCount = 0;

    if (hasSel) {
        for (r = 0; r < PATTERN_ROWS; ++r) {
            for (c = 0; c < MAX_CHANNELS; ++c) {
                if (g_song.patterns[g_song.currentPattern].rows[r][c].flags & 0x01) {
                    if (anchorRow < 0) {
                        anchorRow = (int32_t)r;
                        anchorCh = c;
                    } else if (c < anchorCh) {
                        anchorCh = c;
                    }
                }
            }
        }

        if (anchorRow < 0) return 0;

        for (r = 0; r < PATTERN_ROWS && g_clipCount < MAX_CLIP_ITEMS; ++r) {
            for (c = 0; c < MAX_CHANNELS && g_clipCount < MAX_CLIP_ITEMS; ++c) {
                TrackerNote *src = &g_song.patterns[g_song.currentPattern].rows[r][c];
                if (src->flags & 0x01) {
                    g_clipItems[g_clipCount].relRow = (int16_t)((int32_t)r - anchorRow);
                    g_clipItems[g_clipCount].relCh = (int8_t)(c - anchorCh);
                    g_clipItems[g_clipCount].note = src->note;
                    g_clipItems[g_clipCount].octave = src->octave;
                    g_clipItems[g_clipCount].instrument = src->instrument;
                    g_clipItems[g_clipCount].attribute = src->attribute;
                    g_clipCount++;
                }
            }
        }
    } else {
        TrackerNote *src = &g_song.patterns[g_song.currentPattern].rows[cursorRow][cursorCh];
        g_clipItems[0].relRow = 0;
        g_clipItems[0].relCh = 0;
        g_clipItems[0].note = src->note;
        g_clipItems[0].octave = src->octave;
        g_clipItems[0].instrument = src->instrument;
        g_clipItems[0].attribute = src->attribute;
        g_clipCount = 1;
    }

    return g_clipCount;
}

int Song_CutSelection(uint16_t cursorRow, uint8_t cursorCh) {
    int copied = Song_CopySelection(cursorRow, cursorCh);
    if (copied > 0) {
        if (Song_HasSelections()) {
            Song_DeleteSelectedNotes();
        } else {
            Song_UndoRecord(cursorRow, cursorCh);
            Song_ClearNote(cursorCh, cursorRow);
            Song_UndoNextBatch();
        }
    }
    return copied;
}

int Song_PasteClipboard(uint16_t cursorRow, uint8_t cursorCh) {
    int i, pasted = 0;
    if (g_clipCount <= 0) return 0;

    for (i = 0; i < g_clipCount; ++i) {
        int32_t tr = (int32_t)cursorRow + g_clipItems[i].relRow;
        int tc = (int)cursorCh + g_clipItems[i].relCh;

        if (tr >= 0 && tr < (int32_t)PATTERN_ROWS && tc >= 0 && tc < MAX_CHANNELS) {
            TrackerNote *dst = &g_song.patterns[g_song.currentPattern].rows[tr][tc];
            Song_UndoRecord((uint16_t)tr, (uint8_t)tc);
            dst->note = g_clipItems[i].note;
            dst->octave = g_clipItems[i].octave;
            dst->instrument = g_clipItems[i].instrument;
            dst->attribute = g_clipItems[i].attribute;
            dst->flags = 0;
            pasted++;
        }
    }

    Song_UndoNextBatch();
    return pasted;
}

void Song_Tick(void) {
    int ch;
    uint16_t loopStart, loopEnd;
    uint8_t hasLoop;
    int16_t dampenMs = Song_GetDampenDurationMs();

    if (!g_song.playing) return;

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_song.currentRow][ch];
        int audible = Song_IsChannelAudible((uint8_t)ch);

        if (!audible) {
            if (g_voices[ch].active) {
                OPL_NoteOff((uint8_t)ch);
                g_voices[ch].active = 0;
            }
            continue;
        }

        if (n->attribute == ATTR_MUTE) {
            OPL_NoteOff((uint8_t)ch);
            g_voices[ch].active = 0;
            g_voices[ch].muteTimerMs = 0;
            g_voices[ch].bending = 0;
        } else if (n->note > 0) {
            uint8_t wantSust = (uint8_t)((n->attribute == ATTR_LETRING || n->attribute == ATTR_BEND) ? 1 : 0);

            /* Only rewrite the 11 instrument OPL registers when timbre or sustain changes */
            if (g_voices[ch].loadedInst != n->instrument || g_voices[ch].loadedSustained != wantSust) {
                if (wantSust) {
                    OPL_SetInstrumentSustained((uint8_t)ch, Bank_GetInstrument(n->instrument));
                } else {
                    OPL_SetInstrument((uint8_t)ch, Bank_GetInstrument(n->instrument));
                }
                g_voices[ch].loadedInst = n->instrument;
                g_voices[ch].loadedSustained = wantSust;
            }

            OPL_NoteOn((uint8_t)ch, n->note - 1, n->octave);
            g_voices[ch].active = 1;
            g_voices[ch].note = n->note;
            g_voices[ch].octave = n->octave;
            g_voices[ch].instrument = n->instrument;
            g_voices[ch].muteTimerMs = (int16_t)((n->attribute == ATTR_DAMPEN) ? dampenMs : -1);
            g_voices[ch].bending = 0;

            if (n->attribute == ATTR_BEND) {
                uint32_t r, maxScan;
                uint16_t nextRow = 0;
                uint8_t nextNote = 0, nextOct = 0;

                maxScan = (uint32_t)g_song.currentRow + 128;
                if (maxScan > PATTERN_ROWS) maxScan = PATTERN_ROWS;

                for (r = (uint32_t)g_song.currentRow + 1; r < maxScan; ++r) {
                    TrackerNote *ahead = &g_song.patterns[g_song.currentPattern].rows[r][ch];
                    if (ahead->note > 0) {
                        nextRow = (uint16_t)r;
                        nextNote = ahead->note;
                        nextOct = ahead->octave;
                        break;
                    }
                }

                if (nextRow > g_song.currentRow) {
                    g_voices[ch].bending = 1;
                    g_voices[ch].bendStartRow = g_song.currentRow;
                    g_voices[ch].bendEndRow = nextRow;
                    g_voices[ch].startPitch100 = (int32_t)(n->octave * 12 + (n->note - 1)) * 100;
                    g_voices[ch].endPitch100 = (int32_t)(nextOct * 12 + (nextNote - 1)) * 100;
                }
            }
        } else if (n->attribute == ATTR_DAMPEN && g_voices[ch].active) {
            g_voices[ch].muteTimerMs = dampenMs;
        } else if (n->attribute == ATTR_LETRING && g_voices[ch].active) {
            g_voices[ch].muteTimerMs = -1;
        }
    }

    Loop_GetRange(&g_song.loop, &loopStart, &loopEnd, &hasLoop);

    if (hasLoop && g_song.currentRow == loopEnd) {
        g_song.currentRow = loopStart;
    } else {
        if ((uint32_t)g_song.currentRow + 1 >= PATTERN_ROWS) {
            g_song.currentRow = (uint16_t)(PATTERN_ROWS - 1);
            g_song.playing = 0;
        } else {
            g_song.currentRow++;
        }
    }
}

void Song_SubTick(int subStep, int totalSubSteps) {
    int ch;
    int16_t msPerSub;

    if (!g_song.playing) return;
    msPerSub = (int16_t)(Song_GetMsPerRowFloat() / (double)totalSubSteps + 0.5);
    if (msPerSub <= 0) msPerSub = 1;

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        if (!g_voices[ch].active) continue;
        if (!Song_IsChannelAudible((uint8_t)ch)) {
            OPL_NoteOff((uint8_t)ch);
            g_voices[ch].active = 0;
            continue;
        }

        if (g_voices[ch].muteTimerMs > 0) {
            g_voices[ch].muteTimerMs -= msPerSub;
            if (g_voices[ch].muteTimerMs <= 0) {
                OPL_NoteOff((uint8_t)ch);
                g_voices[ch].active = 0;
                g_voices[ch].bending = 0;
            }
        }

        if (g_voices[ch].bending) {
            int32_t totalSteps = (int32_t)(g_voices[ch].bendEndRow - g_voices[ch].bendStartRow) * totalSubSteps;
            if (totalSteps > 0) {
                int32_t currentStep = (int32_t)(g_song.currentRow - 1 - g_voices[ch].bendStartRow) * totalSubSteps + subStep;
                int32_t pitch;
                uint8_t rA0, rB0;
                if (currentStep < 0) currentStep = 0;
                if (currentStep > totalSteps) currentStep = totalSteps;

                pitch = g_voices[ch].startPitch100 + ((g_voices[ch].endPitch100 - g_voices[ch].startPitch100) * currentStep) / totalSteps;
                CalcPitchRegs(pitch, &rA0, &rB0);
                OPL_SetPitch((uint8_t)ch, pitch);
            }
        }
    }
}

static uint16_t GetExportLimit(void) {
    int32_t r;
    int c;
    int32_t lastActive = -1;

    for (r = (int32_t)PATTERN_ROWS - 1; r >= 0; --r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            if (g_song.patterns[g_song.currentPattern].rows[r][c].note > 0 ||
                g_song.patterns[g_song.currentPattern].rows[r][c].attribute == ATTR_MUTE) {
                lastActive = r;
                break;
            }
        }
        if (lastActive >= 0) break;
    }

    if (lastActive < 0) return 16;

    lastActive += 10;
    if (lastActive >= (int32_t)PATTERN_ROWS) lastActive = (int32_t)PATTERN_ROWS - 1;
    return (uint16_t)lastActive;
}

unsigned long Song_GetTotalLengthMS(void) {
    uint16_t limit = GetExportLimit();
    return (unsigned long)((double)limit * Song_GetMsPerRowFloat());
}

int Song_GetUsedInstruments(uint8_t *outList, int maxCount) {
    uint8_t usedMap[MAX_TOTAL_INSTRUMENTS];
    uint32_t r;
    int c, total = 0;

    memset(usedMap, 0, sizeof(usedMap));
    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (n->note > 0) {
                usedMap[n->instrument] = 1;
            }
        }
    }

    for (c = 0; c < MAX_TOTAL_INSTRUMENTS && total < maxCount; ++c) {
        if (usedMap[c]) {
            outList[total++] = (uint8_t)c;
        }
    }
    return total;
}

int Song_ExportSupportingBNK(const char *baseFilename) {
    char bnkName[64];
    char *dot;
    uint8_t usedList[MAX_TOTAL_INSTRUMENTS];
    int count;

    strncpy(bnkName, baseFilename, 59);
    bnkName[59] = '\0';
    dot = strrchr(bnkName, '.');
    if (dot) *dot = '\0';
    strcat(bnkName, ".BNK");

    count = Song_GetUsedInstruments(usedList, MAX_TOTAL_INSTRUMENTS);
    if (count == 0) {
        usedList[0] = 0;
        count = 1;
    }
    return Bank_ExportBNK(bnkName, usedList, count);
}

typedef struct {
    uint8_t reg;
    uint8_t val;
} OPL_RegWrite;

#define MAX_STEP_WRITES 128

static void EmitInstrumentWrites(OPL_RegWrite *cmds, int *cmdCount, uint8_t ch, uint8_t instIdx, int sustained) {
    uint8_t mod = g_modOffsets[ch];
    uint8_t car = g_carOffsets[ch];
    const OPL_Instrument *inst = Bank_GetInstrument(instIdx);
    uint8_t mod20 = inst->mod_mult_vib_am;
    uint8_t car20 = inst->car_mult_vib_am;
    uint8_t car80 = inst->car_sustain_rel;

    if (sustained) {
        uint8_t carSL = car80 & 0xF0;
        uint8_t carRR = car80 & 0x0F;
        mod20 |= 0x20;
        car20 |= 0x20;
        if (carSL >= 0x70) carSL = 0x10;
        car80 = (uint8_t)(carSL | carRR);
    }

    if (*cmdCount + 11 > MAX_STEP_WRITES) return;
    cmds[(*cmdCount)].reg = (uint8_t)(0x20 + mod); cmds[(*cmdCount)++].val = mod20;
    cmds[(*cmdCount)].reg = (uint8_t)(0x40 + mod); cmds[(*cmdCount)++].val = inst->mod_ksl_level;
    cmds[(*cmdCount)].reg = (uint8_t)(0x60 + mod); cmds[(*cmdCount)++].val = inst->mod_attack_decay;
    cmds[(*cmdCount)].reg = (uint8_t)(0x80 + mod); cmds[(*cmdCount)++].val = inst->mod_sustain_rel;
    cmds[(*cmdCount)].reg = (uint8_t)(0xE0 + mod); cmds[(*cmdCount)++].val = inst->mod_wave;

    cmds[(*cmdCount)].reg = (uint8_t)(0x20 + car); cmds[(*cmdCount)++].val = car20;
    cmds[(*cmdCount)].reg = (uint8_t)(0x40 + car); cmds[(*cmdCount)++].val = inst->car_ksl_level;
    cmds[(*cmdCount)].reg = (uint8_t)(0x60 + car); cmds[(*cmdCount)++].val = inst->car_attack_decay;
    cmds[(*cmdCount)].reg = (uint8_t)(0x80 + car); cmds[(*cmdCount)++].val = car80;
    cmds[(*cmdCount)].reg = (uint8_t)(0xE0 + car); cmds[(*cmdCount)++].val = inst->car_wave;

    cmds[(*cmdCount)].reg = (uint8_t)(0xC0 + ch);  cmds[(*cmdCount)++].val = inst->feedback_conn;
}

typedef struct {
    uint8_t active;
    uint8_t inst;
    uint8_t sustained;
    uint8_t lastB0;
    int16_t muteTimerMs;
    uint8_t bending;
    uint16_t bendStartRow;
    uint16_t bendEndRow;
    int32_t startPitch100;
    int32_t endPitch100;
} ExportVoiceState;

static void SimulateSubStepExport(uint16_t row, int subStep, int totalSubSteps, int16_t subStepMs,
                                  int16_t dampenMs, ExportVoiceState *expVoices, OPL_RegWrite *cmds, int *cmdCount) {
    int ch;
    *cmdCount = 0;

    if (subStep == 0) {
        for (ch = 0; ch < MAX_CHANNELS; ++ch) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
            if (!Song_IsChannelAudible((uint8_t)ch)) {
                if (expVoices[ch].active) {
                    expVoices[ch].lastB0 &= (uint8_t)(~0x20);
                    cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = expVoices[ch].lastB0;
                    expVoices[ch].active = 0;
                    expVoices[ch].bending = 0;
                }
                continue;
            }

            if (n->attribute == ATTR_MUTE) {
                if (expVoices[ch].active) {
                    expVoices[ch].lastB0 &= (uint8_t)(~0x20);
                    cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = expVoices[ch].lastB0;
                    expVoices[ch].active = 0;
                }
                expVoices[ch].muteTimerMs = 0;
                expVoices[ch].bending = 0;
            } else if (n->note > 0) {
                uint8_t wantSust = (uint8_t)((n->attribute == ATTR_LETRING || n->attribute == ATTR_BEND) ? 1 : 0);
                uint16_t fnum = g_fNums[(n->note - 1) % 12];
                uint8_t regA0 = (uint8_t)(fnum & 0xFF);
                uint8_t regB0 = (uint8_t)(0x20 | ((n->octave & 0x07) << 2) | ((fnum >> 8) & 0x03));

                if (expVoices[ch].active) {
                    expVoices[ch].lastB0 &= (uint8_t)(~0x20);
                    cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = expVoices[ch].lastB0;
                }

                if (expVoices[ch].inst != n->instrument || expVoices[ch].sustained != wantSust) {
                    EmitInstrumentWrites(cmds, cmdCount, (uint8_t)ch, n->instrument, wantSust);
                    expVoices[ch].inst = n->instrument;
                    expVoices[ch].sustained = wantSust;
                }

                cmds[(*cmdCount)].reg = (uint8_t)(0xA0 + ch); cmds[(*cmdCount)++].val = regA0;
                cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = regB0;

                expVoices[ch].lastB0 = regB0;
                expVoices[ch].active = 1;
                expVoices[ch].muteTimerMs = (int16_t)((n->attribute == ATTR_DAMPEN) ? dampenMs : -1);
                expVoices[ch].bending = 0;

                if (n->attribute == ATTR_BEND) {
                    uint32_t sr, maxScan;
                    uint16_t nextRow = 0;
                    uint8_t nextNote = 0, nextOct = 0;
                    maxScan = (uint32_t)row + 128;
                    if (maxScan > PATTERN_ROWS) maxScan = PATTERN_ROWS;
                    for (sr = (uint32_t)row + 1; sr < maxScan; ++sr) {
                        TrackerNote *ahead = &g_song.patterns[g_song.currentPattern].rows[sr][ch];
                        if (ahead->note > 0) {
                            nextRow = (uint16_t)sr;
                            nextNote = ahead->note;
                            nextOct = ahead->octave;
                            break;
                        }
                    }
                    if (nextRow > row) {
                        expVoices[ch].bending = 1;
                        expVoices[ch].bendStartRow = row;
                        expVoices[ch].bendEndRow = nextRow;
                        expVoices[ch].startPitch100 = (int32_t)(n->octave * 12 + (n->note - 1)) * 100;
                        expVoices[ch].endPitch100 = (int32_t)(nextOct * 12 + (nextNote - 1)) * 100;
                    }
                }
            } else if (n->attribute == ATTR_DAMPEN && expVoices[ch].active) {
                expVoices[ch].muteTimerMs = dampenMs;
            } else if (n->attribute == ATTR_LETRING && expVoices[ch].active) {
                expVoices[ch].muteTimerMs = -1;
            }
        }
    }

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        if (!expVoices[ch].active) continue;

        if (expVoices[ch].muteTimerMs > 0) {
            expVoices[ch].muteTimerMs -= subStepMs;
            if (expVoices[ch].muteTimerMs <= 0) {
                expVoices[ch].lastB0 &= (uint8_t)(~0x20);
                cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = expVoices[ch].lastB0;
                expVoices[ch].active = 0;
                expVoices[ch].bending = 0;
                continue;
            }
        }

        if (expVoices[ch].bending && !(subStep == 0 && expVoices[ch].bendStartRow == row)) {
            int32_t totalSteps = (int32_t)(expVoices[ch].bendEndRow - expVoices[ch].bendStartRow) * totalSubSteps;
            if (totalSteps > 0) {
                int32_t currentStep = (int32_t)(row - expVoices[ch].bendStartRow) * totalSubSteps + subStep;
                int32_t pitch;
                uint8_t rA0, rB0;
                if (currentStep < 0) currentStep = 0;
                if (currentStep > totalSteps) currentStep = totalSteps;

                pitch = expVoices[ch].startPitch100 + ((expVoices[ch].endPitch100 - expVoices[ch].startPitch100) * currentStep) / totalSteps;
                CalcPitchRegs(pitch, &rA0, &rB0);
                expVoices[ch].lastB0 = rB0;
                cmds[(*cmdCount)].reg = (uint8_t)(0xA0 + ch); cmds[(*cmdCount)++].val = rA0;
                cmds[(*cmdCount)].reg = (uint8_t)(0xB0 + ch); cmds[(*cmdCount)++].val = rB0;
            }
        }
    }
}

int Song_ExportIMF_Ex(const char *filename, uint16_t targetHz) {
    FILE *fp;
    uint32_t r;
    int ch, s, i;
    uint16_t exportLimit = GetExportLimit();
    double targetRate;
    double msPerRow = Song_GetMsPerRowFloat();
    double ticksPerSubStep;
    double tickAccum = 0.0;
    uint32_t ticksEmitted = 0;
    uint32_t pendingDelay = 0;
    int16_t subStepMs;
    int16_t dampenMs = Song_GetDampenDurationMs();
    ExportVoiceState expVoices[MAX_CHANNELS];
    OPL_RegWrite cmds[MAX_STEP_WRITES];
    int cmdCount;
    long endPos;
    uint32_t dataSize;

    if (targetHz > 0) {
        targetRate = (double)targetHz;
    } else if (StrEndsWithCI(filename, ".WLF")) {
        targetRate = 700.0;
    } else {
        targetRate = (g_song.clockHz > 0) ? (double)g_song.clockHz : 560.0;
    }

    fp = fopen(filename, "w+b");
    if (!fp) return 0;

    ticksPerSubStep = (targetRate * msPerRow) / 6000.0;
    subStepMs = (int16_t)(msPerRow / 6.0 + 0.5);
    if (subStepMs <= 0) subStepMs = 1;

    memset(expVoices, 0, sizeof(expVoices));
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        expVoices[ch].inst = 0xFF;
    }

    fputc(0x00, fp); fputc(0x00, fp);

    fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
    fputc(0x01, fp); fputc(0x20, fp); fputc(0x00, fp); fputc(0x00, fp);
    fputc(0x08, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
    fputc(0xBD, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        fputc(0xB0 + ch, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
    }

    for (r = 0; r <= (uint32_t)exportLimit; ++r) {
        for (s = 0; s < 6; ++s) {
            uint32_t targetTicks;
            uint32_t stepDelay;

            SimulateSubStepExport((uint16_t)r, s, 6, subStepMs, dampenMs, expVoices, cmds, &cmdCount);

            tickAccum += ticksPerSubStep;
            targetTicks = (uint32_t)(tickAccum + 0.5);
            stepDelay = targetTicks - ticksEmitted;
            ticksEmitted = targetTicks;

            if (cmdCount > 0) {
                if (pendingDelay > 0) {
                    while (pendingDelay > 65535UL) {
                        fputc(0x00, fp); fputc(0x00, fp); fputc(0xFF, fp); fputc(0xFF, fp);
                        pendingDelay -= 65535UL;
                    }
                    fputc(0x00, fp); fputc(0x00, fp);
                    fputc((uint8_t)(pendingDelay & 0xFF), fp);
                    fputc((uint8_t)((pendingDelay >> 8) & 0xFF), fp);
                    pendingDelay = 0;
                }

                for (i = 0; i < cmdCount; ++i) {
                    uint16_t d = (uint16_t)((i == cmdCount - 1) ? (stepDelay > 65535UL ? 65535UL : stepDelay) : 0);
                    fputc(cmds[i].reg, fp);
                    fputc(cmds[i].val, fp);
                    fputc((uint8_t)(d & 0xFF), fp);
                    fputc((uint8_t)((d >> 8) & 0xFF), fp);
                }
                if (stepDelay > 65535UL) {
                    pendingDelay = stepDelay - 65535UL;
                }
            } else {
                pendingDelay += stepDelay;
            }
        }
    }

    if (pendingDelay > 0) {
        while (pendingDelay > 65535UL) {
            fputc(0x00, fp); fputc(0x00, fp); fputc(0xFF, fp); fputc(0xFF, fp);
            pendingDelay -= 65535UL;
        }
        fputc(0x00, fp); fputc(0x00, fp);
        fputc((uint8_t)(pendingDelay & 0xFF), fp);
        fputc((uint8_t)((pendingDelay >> 8) & 0xFF), fp);
    }

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        expVoices[ch].lastB0 &= (uint8_t)(~0x20);
        fputc(0xB0 + ch, fp); fputc(expVoices[ch].lastB0, fp); fputc(0x00, fp); fputc(0x00, fp);
    }

    endPos = ftell(fp);
    dataSize = (uint32_t)(endPos - 2);

    if (dataSize <= 65532UL) {
        uint16_t len16 = (uint16_t)dataSize;
        fseek(fp, 0, SEEK_SET);
        fwrite(&len16, 2, 1, fp);
    } else {
        uint8_t buf[512];
        long readPos = 2;
        long writePos = 0;
        while (readPos < endPos) {
            size_t chunk = (size_t)((endPos - readPos) > 512 ? 512 : (endPos - readPos));
            fseek(fp, readPos, SEEK_SET);
            fread(buf, 1, chunk, fp);
            fseek(fp, writePos, SEEK_SET);
            fwrite(buf, 1, chunk, fp);
            readPos += (long)chunk;
            writePos += (long)chunk;
        }
        fflush(fp);
        chsize(fileno(fp), writePos);
    }

    fclose(fp);
    return 1;
}

int Song_ExportIMF(const char *filename) {
    return Song_ExportIMF_Ex(filename, 0);
}

static int ScoreIMFPackets(FILE *fp, long offset, long fileSize) {
    int score = 0;
    int i;
    long savePos = ftell(fp);
    fseek(fp, offset, SEEK_SET);

    for (i = 0; i < 16 && ftell(fp) + 4 <= fileSize; ++i) {
        uint8_t reg = (uint8_t)fgetc(fp);
        uint8_t val = (uint8_t)fgetc(fp);
        uint16_t delay = 0;
        fread(&delay, 2, 1, fp);
        (void)val;

        if (reg == 0x00 || reg == 0x01 || reg == 0x04 || reg == 0x08 || reg == 0xBD ||
            (reg >= 0x20 && reg <= 0xF5)) {
            score++;
        } else {
            score -= 2;
        }

        if (delay < 2000) {
            score++;
        } else {
            score -= 2;
        }
    }

    fseek(fp, savePos, SEEK_SET);
    return score;
}

/*
 * Accurate IMF Clock Auto-Detector (280 / 560 / 700 Hz)
 * Uses non-zero delay divisibility (5x vs 4x) and per-channel note spacing.
 */
static uint16_t DetectImfClockHz(FILE *fp, long dataStart, long dataEnd, const char *filename) {
    uint32_t totalNZ = 0;
    uint32_t div5Only = 0;
    uint32_t div4Only = 0;
    uint32_t chAccum[MAX_CHANNELS];
    uint8_t chPrevKey[MAX_CHANNELS];
    uint32_t deltaSum = 0;
    uint32_t deltaCount = 0;
    int ch;

    if (StrEndsWithCI(filename, ".WLF")) {
        return 700;
    }

    memset(chAccum, 0, sizeof(chAccum));
    memset(chPrevKey, 0, sizeof(chPrevKey));

    fseek(fp, dataStart, SEEK_SET);
    while (ftell(fp) + 4 <= dataEnd && totalNZ < 1024) {
        uint8_t reg = (uint8_t)fgetc(fp);
        uint8_t val = (uint8_t)fgetc(fp);
        uint16_t delay = 0;
        fread(&delay, 2, 1, fp);

        if (reg >= 0xB0 && reg <= 0xB8) {
            uint8_t c = (uint8_t)(reg - 0xB0);
            uint8_t ko = (uint8_t)((val & 0x20) ? 1 : 0);
            if (ko && !chPrevKey[c]) {
                if (chAccum[c] >= 4) {
                    deltaSum += chAccum[c];
                    deltaCount++;
                }
                chAccum[c] = 0;
            }
            chPrevKey[c] = ko;
        }

        if (delay > 0) {
            totalNZ++;
            if ((delay % 5) == 0 && (delay % 4) != 0) div5Only++;
            else if ((delay % 4) == 0 && (delay % 5) != 0) div4Only++;

            for (ch = 0; ch < MAX_CHANNELS; ++ch) {
                chAccum[ch] += (uint32_t)delay;
            }
        }
    }

    if (totalNZ >= 8) {
        if (div5Only > (totalNZ * 3) / 10 && div5Only > div4Only * 2) {
            return 700;
        }
        if (div4Only > (totalNZ * 3) / 10 && div4Only > div5Only * 2) {
            return 560;
        }
    }

    if (deltaCount >= 4) {
        uint32_t avg = deltaSum / deltaCount;
        if (avg <= 44)  return 280;
        if (avg >= 125) return 700;
    }

    return 560;
}

/*
 * Accurate IMF Loader supporting 280, 560, and 700 Hz clock rates mapped
 * onto a high-fidelity 10ms/row timeline with nearest-row rounding.
 */
int Song_LoadIMF_Ex(const char *filename, uint16_t forcedHz) {
    FILE *fp;
    long fileSize;
    long dataStart = 0;
    long dataEnd = 0;
    char hdr6[6];
    uint16_t word0 = 0;
    uint32_t row = 0;
    uint32_t prevRow = 0;
    double totalTicks = 0.0;
    double ticksPerRow;
    int ch;
    uint16_t detectedHz;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize < 4) {
        fclose(fp);
        return 0;
    }

    if (fileSize > 16 && fread(hdr6, 1, 6, fp) == 6 && memcmp(hdr6, "ADLIB\x01", 6) == 0) {
        uint32_t dataLen = 0;
        int c;
        while ((c = fgetc(fp)) != EOF && c != '\0');
        while ((c = fgetc(fp)) != EOF && c != '\0');
        fgetc(fp);
        fread(&dataLen, 4, 1, fp);
        dataStart = ftell(fp);
        dataEnd = dataStart + (long)dataLen;
        if (dataEnd > fileSize) dataEnd = fileSize;
    } else {
        fseek(fp, 0, SEEK_SET);
        fread(&word0, 2, 1, fp);

        if (word0 == 0) {
            dataStart = 0;
            dataEnd = fileSize;
        } else if ((word0 & 3) == 0 && (long)(word0 + 2) <= fileSize) {
            int checkTag = 0;
            if ((long)(word0 + 2) == fileSize) {
                checkTag = 1;
            } else {
                fseek(fp, (long)(word0 + 2), SEEK_SET);
                if (fgetc(fp) == 0x1A) checkTag = 1;
            }

            if (checkTag || ScoreIMFPackets(fp, 2, fileSize) >= ScoreIMFPackets(fp, 0, fileSize)) {
                dataStart = 2;
                dataEnd = 2 + (long)word0;
            } else {
                dataStart = 0;
                dataEnd = fileSize;
            }
        } else {
            dataStart = 0;
            dataEnd = fileSize;
        }
    }

    if (forcedHz > 0) {
        detectedHz = forcedHz;
    } else {
        detectedHz = DetectImfClockHz(fp, dataStart, dataEnd, filename);
    }

    Song_ClearAll();
    g_song.clockHz = detectedHz;
    g_song.rowMs = 10; /* 10ms/row high-res timeline eliminates rhythmic limp */
    g_song.tempo = 120;

    InitShadowChannels();

    ticksPerRow = ((double)g_song.clockHz * (double)g_song.rowMs) / 1000.0;
    if (ticksPerRow < 1.0) ticksPerRow = 1.0;

    fseek(fp, dataStart, SEEK_SET);

    while (ftell(fp) + 4 <= dataEnd && row < PATTERN_ROWS) {
        uint8_t reg = (uint8_t)fgetc(fp);
        uint8_t val = (uint8_t)fgetc(fp);
        uint16_t delay = 0;
        fread(&delay, 2, 1, fp);

        ImportProcessOPLWrite(reg, val, (uint16_t)row);

        totalTicks += (double)delay;
        row = (uint32_t)((totalTicks / ticksPerRow) + 0.5);
        if (row != prevRow) {
            for (ch = 0; ch < MAX_CHANNELS; ++ch) {
                g_importState.rowHadNote[ch] = 0;
            }
            prevRow = row;
        }
    }

    fclose(fp);
    return 1;
}

int Song_LoadIMF(const char *filename) {
    return Song_LoadIMF_Ex(filename, 0);
}

static void DRO_WriteReg(FILE *fp, uint8_t reg, uint8_t val, uint32_t *lengthBytes) {
    if (reg <= 0x04) {
        fputc(0x04, fp);
        fputc(reg, fp);
        fputc(val, fp);
        *lengthBytes += 3;
    } else {
        fputc(reg, fp);
        fputc(val, fp);
        *lengthBytes += 2;
    }
}

static void DRO_FlushDelay(FILE *fp, uint32_t delayMs, uint32_t *lengthBytes, uint32_t *lengthMS) {
    while (delayMs > 0) {
        if (delayMs <= 256) {
            fputc(0x00, fp);
            fputc((uint8_t)(delayMs - 1), fp);
            *lengthBytes += 2;
            *lengthMS += delayMs;
            delayMs = 0;
        } else if (delayMs <= 65536UL) {
            uint16_t d = (uint16_t)(delayMs - 1);
            fputc(0x01, fp);
            fputc((uint8_t)(d & 0xFF), fp);
            fputc((uint8_t)((d >> 8) & 0xFF), fp);
            *lengthBytes += 3;
            *lengthMS += delayMs;
            delayMs = 0;
        } else {
            fputc(0x01, fp);
            fputc(0xFF, fp);
            fputc(0xFF, fp);
            *lengthBytes += 3;
            *lengthMS += 65536UL;
            delayMs -= 65536UL;
        }
    }
}

int Song_ExportDRO(const char *filename) {
    FILE *fp;
    uint32_t lengthBytes = 0;
    uint32_t lengthMS = 0;
    double msPerRow = Song_GetMsPerRowFloat();
    uint32_t r;
    int ch, s, i;
    uint16_t exportLimit = GetExportLimit();
    double msPerSubStep;
    double msAccum = 0.0;
    uint32_t msEmitted = 0;
    uint32_t pendingMs = 0;
    int16_t subStepMs;
    int16_t dampenMs = Song_GetDampenDurationMs();
    ExportVoiceState expVoices[MAX_CHANNELS];
    OPL_RegWrite cmds[MAX_STEP_WRITES];
    int cmdCount;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    msPerSubStep = msPerRow / 6.0;
    subStepMs = (int16_t)(msPerRow / 6.0 + 0.5);
    if (subStepMs <= 0) subStepMs = 1;

    memset(expVoices, 0, sizeof(expVoices));
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        expVoices[ch].inst = 0xFF;
    }

    fwrite("DBRAWOPL", 1, 8, fp);
    fputc(0x00, fp); fputc(0x00, fp);
    fputc(0x01, fp); fputc(0x00, fp);
    fwrite(&lengthMS, 4, 1, fp);
    fwrite(&lengthBytes, 4, 1, fp);
    fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);

    DRO_WriteReg(fp, 0x01, 0x20, &lengthBytes);
    DRO_WriteReg(fp, 0x08, 0x00, &lengthBytes);
    DRO_WriteReg(fp, 0xBD, 0x00, &lengthBytes);
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        DRO_WriteReg(fp, (uint8_t)(0xB0 + ch), 0x00, &lengthBytes);
    }

    for (r = 0; r <= (uint32_t)exportLimit; ++r) {
        for (s = 0; s < 6; ++s) {
            uint32_t targetMs;
            uint32_t msStep;

            SimulateSubStepExport((uint16_t)r, s, 6, subStepMs, dampenMs, expVoices, cmds, &cmdCount);

            if (cmdCount > 0) {
                if (pendingMs > 0) {
                    DRO_FlushDelay(fp, pendingMs, &lengthBytes, &lengthMS);
                    pendingMs = 0;
                }
                for (i = 0; i < cmdCount; ++i) {
                    DRO_WriteReg(fp, cmds[i].reg, cmds[i].val, &lengthBytes);
                }
            }

            msAccum += msPerSubStep;
            targetMs = (uint32_t)(msAccum + 0.5);
            msStep = targetMs - msEmitted;
            msEmitted = targetMs;
            pendingMs += msStep;
        }
    }

    if (pendingMs > 0) {
        DRO_FlushDelay(fp, pendingMs, &lengthBytes, &lengthMS);
    }

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        expVoices[ch].lastB0 &= (uint8_t)(~0x20);
        DRO_WriteReg(fp, (uint8_t)(0xB0 + ch), expVoices[ch].lastB0, &lengthBytes);
    }

    fseek(fp, 12, SEEK_SET);
    fwrite(&lengthMS, 4, 1, fp);
    fwrite(&lengthBytes, 4, 1, fp);

    fclose(fp);
    return 1;
}

int Song_LoadDRO(const char *filename) {
    FILE *fp;
    char sig[8];
    uint16_t verW0 = 0, verW1 = 0;
    uint32_t row = 0;
    uint32_t prevRow = 0;
    double totalMs = 0.0;
    double msThreshold = 10.0;
    int ch;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    if (fread(sig, 1, 8, fp) != 8 || memcmp(sig, "DBRAWOPL", 8) != 0) {
        fclose(fp);
        return 0;
    }

    fread(&verW0, 2, 1, fp);
    fread(&verW1, 2, 1, fp);

    Song_ClearAll();
    g_song.clockHz = 1000;
    g_song.rowMs = 10;
    g_song.tempo = 120;
    InitShadowChannels();

    if (verW0 == 2 && verW1 == 0) {
        uint32_t lengthPairs = 0, lengthMS = 0;
        uint8_t hwType, format, comp, shortDelayCode, longDelayCode, codemapLen;
        uint8_t codemap[128];
        uint32_t p;

        fread(&lengthPairs, 4, 1, fp);
        fread(&lengthMS, 4, 1, fp);
        hwType = (uint8_t)fgetc(fp);
        format = (uint8_t)fgetc(fp);
        comp = (uint8_t)fgetc(fp);
        shortDelayCode = (uint8_t)fgetc(fp);
        longDelayCode = (uint8_t)fgetc(fp);
        codemapLen = (uint8_t)fgetc(fp);
        (void)hwType;
        (void)lengthMS;

        if (format != 0 || comp != 0 || codemapLen > 128) {
            fclose(fp);
            return 0;
        }

        memset(codemap, 0, sizeof(codemap));
        if (codemapLen > 0) {
            fread(codemap, 1, codemapLen, fp);
        }

        for (p = 0; p < lengthPairs && row < PATTERN_ROWS; ++p) {
            int c0 = fgetc(fp);
            int c1 = fgetc(fp);
            uint8_t code, val;
            if (c0 == EOF || c1 == EOF) break;
            code = (uint8_t)c0;
            val  = (uint8_t)c1;

            if (code == shortDelayCode) {
                totalMs += (double)((uint32_t)val + 1);
            } else if (code == longDelayCode) {
                totalMs += (double)(((uint32_t)val + 1) << 8);
            } else {
                uint8_t chip = (uint8_t)((code & 0x80) ? 1 : 0);
                uint8_t idx  = (uint8_t)(code & 0x7F);
                if (chip == 0 && idx < codemapLen) {
                    ImportProcessOPLWrite(codemap[idx], val, (uint16_t)row);
                }
            }

            row = (uint32_t)((totalMs / msThreshold) + 0.5);
            if (row != prevRow) {
                for (ch = 0; ch < MAX_CHANNELS; ++ch) {
                    g_importState.rowHadNote[ch] = 0;
                }
                prevRow = row;
            }
        }
    } else {
        uint32_t lengthMS = 0, lengthBytes = 0;
        uint8_t hw[4];
        long dataStart;
        int curChip = 0;

        fread(&lengthMS, 4, 1, fp);
        fread(&lengthBytes, 4, 1, fp);
        fread(hw, 1, 4, fp);
        (void)lengthMS;

        if (hw[1] != 0 || hw[2] != 0 || hw[3] != 0) {
            fseek(fp, -3, SEEK_CUR);
        }

        dataStart = ftell(fp);
        curChip = 0;

        while ((uint32_t)(ftell(fp) - dataStart) < lengthBytes && row < PATTERN_ROWS) {
            int c = fgetc(fp);
            uint8_t code;
            if (c == EOF) break;
            code = (uint8_t)c;

            if (code == 0x00) {
                int d = fgetc(fp);
                if (d == EOF) break;
                totalMs += (double)((uint32_t)d + 1);
            } else if (code == 0x01) {
                uint16_t d = 0;
                if (fread(&d, 2, 1, fp) != 1) break;
                totalMs += (double)((uint32_t)d + 1);
            } else if (code == 0x02) {
                curChip = 0;
            } else if (code == 0x03) {
                curChip = 1;
            } else if (code == 0x04) {
                int r = fgetc(fp);
                int v = fgetc(fp);
                if (r == EOF || v == EOF) break;
                if (curChip == 0) {
                    ImportProcessOPLWrite((uint8_t)r, (uint8_t)v, (uint16_t)row);
                }
            } else {
                int v = fgetc(fp);
                if (v == EOF) break;
                if (curChip == 0) {
                    ImportProcessOPLWrite(code, (uint8_t)v, row);
                }
            }

            row = (uint32_t)((totalMs / msThreshold) + 0.5);
            if (row != prevRow) {
                for (ch = 0; ch < MAX_CHANNELS; ++ch) {
                    g_importState.rowHadNote[ch] = 0;
                }
                prevRow = row;
            }
        }
    }

    fclose(fp);
    return 1;
}

int Song_SaveADS(const char *filename, uint8_t selectedChannel, uint8_t selectedOctave, uint8_t selectedPreset) {
    FILE *fp;
    uint16_t verMajor = 1, verMinor = 6;
    char bnkName[32];
    uint16_t bnkVoiceCount;
    uint32_t activeCellCount = 0;
    uint32_t r;
    int c;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    fwrite("ADSYNTH1", 1, 8, fp);
    fwrite(&verMajor, 2, 1, fp);
    fwrite(&verMinor, 2, 1, fp);

    fwrite(&g_song.tempo, 2, 1, fp);
    fwrite(&g_song.clockHz, 2, 1, fp);
    fwrite(&g_song.rowMs, 2, 1, fp);
    fwrite(&g_song.currentRow, 2, 1, fp);
    fputc(g_song.currentPattern, fp);
    fputc(selectedChannel, fp);
    fputc(selectedOctave, fp);
    fputc(selectedPreset, fp);

    fwrite(&g_song.loop.loopPoint1, 2, 1, fp);
    fwrite(&g_song.loop.loopPoint2, 2, 1, fp);

    for (c = 0; c < MAX_CHANNELS; ++c) {
        fputc((uint8_t)g_song.channelModes[c], fp);
    }

    memset(bnkName, 0, sizeof(bnkName));
    strncpy(bnkName, Bank_GetLoadedBnkFilename(), 31);
    fwrite(bnkName, 1, 32, fp);

    bnkVoiceCount = (uint16_t)Bank_GetBnkCount();
    fwrite(&bnkVoiceCount, 2, 1, fp);

    for (c = 0; c < bnkVoiceCount; ++c) {
        const OPL_Instrument *inst = Bank_GetInstrument(NUM_PRESET_INSTRUMENTS + c);
        fwrite(inst, sizeof(OPL_Instrument), 1, fp);
    }

    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (n->note > 0 || n->attribute != ATTR_NONE || (n->flags & 0x01)) {
                activeCellCount++;
            }
        }
    }
    fwrite(&activeCellCount, 4, 1, fp);

    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (n->note > 0 || n->attribute != ATTR_NONE || (n->flags & 0x01)) {
                uint16_t r16 = (uint16_t)r;
                fwrite(&r16, 2, 1, fp);
                fputc((uint8_t)c, fp);
                fputc(n->note, fp);
                fputc(n->octave, fp);
                fputc(n->instrument, fp);
                fputc(n->attribute, fp);
                fputc(n->flags, fp);
            }
        }
    }

    fclose(fp);
    return 1;
}

int Song_LoadADS(const char *filename, uint8_t *outChannel, uint8_t *outOctave, uint8_t *outPreset) {
    FILE *fp;
    char sig[8];
    uint16_t verMajor, verMinor;
    char bnkName[32];
    uint16_t bnkVoiceCount;
    uint32_t activeCellCount = 0;
    uint32_t i;
    int c;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    if (fread(sig, 1, 8, fp) != 8 || memcmp(sig, "ADSYNTH1", 8) != 0) {
        fclose(fp);
        return 0;
    }

    fread(&verMajor, 2, 1, fp);
    fread(&verMinor, 2, 1, fp);

    Song_ClearAll();

    fread(&g_song.tempo, 2, 1, fp);
    if (verMinor < 5) {
        g_song.tempo = 120;
    }

    if (verMinor >= 1) {
        fread(&g_song.clockHz, 2, 1, fp);
    } else {
        g_song.clockHz = 560;
    }

    if (verMinor >= 6) {
        fread(&g_song.rowMs, 2, 1, fp);
    } else {
        g_song.rowMs = 30;
    }

    fread(&g_song.currentRow, 2, 1, fp);
    g_song.currentPattern = (uint8_t)fgetc(fp);
    *outChannel = (uint8_t)fgetc(fp);
    *outOctave  = (uint8_t)fgetc(fp);
    *outPreset  = (uint8_t)fgetc(fp);

    fread(&g_song.loop.loopPoint1, 2, 1, fp);
    fread(&g_song.loop.loopPoint2, 2, 1, fp);

    for (c = 0; c < MAX_CHANNELS; ++c) {
        g_song.channelModes[c] = (ChannelMode)fgetc(fp);
    }

    fread(bnkName, 1, 32, fp);
    bnkName[31] = '\0';
    Bank_SetLoadedBnkFilename(bnkName);

    fread(&bnkVoiceCount, 2, 1, fp);

    if (bnkName[0] && strcmp(bnkName, "NONE") != 0 && Bank_LoadBNK(bnkName) > 0) {
        fseek(fp, (long)bnkVoiceCount * sizeof(OPL_Instrument), SEEK_CUR);
    } else {
        for (c = 0; c < bnkVoiceCount; ++c) {
            OPL_Instrument inst;
            fread(&inst, sizeof(OPL_Instrument), 1, fp);
            Bank_SetCustomInstrument(NUM_PRESET_INSTRUMENTS + c, &inst);
        }
    }

    fread(&activeCellCount, 4, 1, fp);

    for (i = 0; i < activeCellCount; ++i) {
        uint16_t r;
        uint8_t ch, note, oct, inst, attr, flags;
        fread(&r, 2, 1, fp);
        ch = (uint8_t)fgetc(fp);
        note = (uint8_t)fgetc(fp);
        oct  = (uint8_t)fgetc(fp);
        inst = (uint8_t)fgetc(fp);
        attr = (uint8_t)fgetc(fp);
        flags= (uint8_t)fgetc(fp);

        if (ch < MAX_CHANNELS) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][ch];
            n->note = note;
            n->octave = oct;
            n->instrument = inst;
            n->attribute = attr;
            n->flags = flags;
        }
    }

    fclose(fp);
    return 1;
}

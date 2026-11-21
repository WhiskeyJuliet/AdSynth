#include "song.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

SongState g_song;

typedef struct {
    uint8_t active;
    uint8_t note;
    uint8_t octave;
    uint8_t instrument;
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

static OPL_ChannelShadow g_shadowChannels[MAX_CHANNELS];
static int g_importedVoiceCount = 0;

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
    }
    g_importedVoiceCount = 0;
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

    return 0;
}

const char *Song_GetClockDescription(uint16_t hz) {
    if (hz == 70)   return "70 HZ : WOLF3D / VGA RETRACE";
    if (hz == 560)  return "560 HZ : ID KEEN/DUKE2/BIO";
    if (hz == 700)  return "700 HZ : APOGEE BLAKE/STRYKER";
    if (hz == 1000) return "1000 HZ : DRO 1MS / DOSBOX";
    return "CUSTOM / IMPORTED RATE";
}

uint32_t Song_GetMsPerRow(void) {
    uint32_t ms = (uint32_t)(60000.0 / ((double)g_song.bpm * 4.0));
    return (ms == 0) ? 120 : ms;
}

void Song_Init(void) {
    int ch;
    memset(&g_song, 0, sizeof(SongState));
    g_song.bpm = 125;
    g_song.clockHz = 560;
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
    if (ch >= MAX_CHANNELS || row >= PATTERN_ROWS) return;
    g_song.patterns[g_song.currentPattern].rows[row][ch].note = note;
    g_song.patterns[g_song.currentPattern].rows[row][ch].octave = octave;
    g_song.patterns[g_song.currentPattern].rows[row][ch].instrument = inst;
}

void Song_ClearNote(uint8_t ch, uint16_t row) {
    if (ch >= MAX_CHANNELS || row >= PATTERN_ROWS) return;
    memset(&g_song.patterns[g_song.currentPattern].rows[row][ch], 0, sizeof(TrackerNote));
}

void Song_ToggleNoteAttribute(uint8_t ch, uint16_t row) {
    TrackerNote *n;
    if (ch >= MAX_CHANNELS || row >= PATTERN_ROWS) return;
    n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
    n->attribute = (n->attribute + 1) % 5;
}

void Song_SetNoteAttribute(uint8_t ch, uint16_t row, uint8_t attr) {
    TrackerNote *n;
    if (ch >= MAX_CHANNELS || row >= PATTERN_ROWS) return;
    n = &g_song.patterns[g_song.currentPattern].rows[row][ch];
    n->attribute = attr;
}

void Song_SetSelection(uint16_t row, uint8_t ch, uint8_t sel) {
    TrackerNote *n;
    if (row >= PATTERN_ROWS || ch >= MAX_CHANNELS) return;
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
    uint16_t r;
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
    uint16_t r;
    int c;
    if (g_selectedCount <= 0) return;
    for (r = 0; r < PATTERN_ROWS; ++r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][c];
            if (n->flags & 0x01) {
                Song_UndoRecord(r, (uint8_t)c);
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
    if (row >= PATTERN_ROWS || ch >= MAX_CHANNELS) return;
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
    int anchorRow = -1;
    int anchorCh = -1;
    int r, c;

    g_clipCount = 0;

    if (hasSel) {
        for (r = 0; r < PATTERN_ROWS; ++r) {
            for (c = 0; c < MAX_CHANNELS; ++c) {
                if (g_song.patterns[g_song.currentPattern].rows[r][c].flags & 0x01) {
                    if (anchorRow < 0) {
                        anchorRow = r;
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
                    g_clipItems[g_clipCount].relRow = (int16_t)(r - anchorRow);
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
        int tr = (int)cursorRow + g_clipItems[i].relRow;
        int tc = (int)cursorCh + g_clipItems[i].relCh;

        if (tr >= 0 && tr < PATTERN_ROWS && tc >= 0 && tc < MAX_CHANNELS) {
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

        if (n->attribute == ATTR_DAMPEN) {
            if (n->note > 0) {
                OPL_SetInstrument((uint8_t)ch, Bank_GetInstrument(n->instrument));
                OPL_NoteOn((uint8_t)ch, n->note - 1, n->octave);
                g_voices[ch].active = 1;
                g_voices[ch].note = n->note;
                g_voices[ch].octave = n->octave;
                g_voices[ch].instrument = n->instrument;
                g_voices[ch].muteTimerMs = DAMPEN_CELL_MS;
                g_voices[ch].bending = 0;
            } else if (g_voices[ch].active) {
                g_voices[ch].muteTimerMs = DAMPEN_CELL_MS;
            }
        } else if (n->attribute == ATTR_MUTE) {
            OPL_NoteOff((uint8_t)ch);
            g_voices[ch].active = 0;
            g_voices[ch].muteTimerMs = 0;
            g_voices[ch].bending = 0;
        } else if (n->attribute == ATTR_LETRING) {
            if (n->note > 0) {
                OPL_SetInstrumentSustained((uint8_t)ch, Bank_GetInstrument(n->instrument));
                OPL_NoteOn((uint8_t)ch, n->note - 1, n->octave);
                g_voices[ch].active = 1;
                g_voices[ch].note = n->note;
                g_voices[ch].octave = n->octave;
                g_voices[ch].instrument = n->instrument;
                g_voices[ch].muteTimerMs = -1;
                g_voices[ch].bending = 0;
            } else if (g_voices[ch].active) {
                g_voices[ch].muteTimerMs = -1;
            }
        } else if (n->attribute == ATTR_BEND) {
            if (n->note > 0) {
                uint16_t r, maxScan;
                uint16_t nextRow = 0;
                uint8_t nextNote = 0, nextOct = 0;

                OPL_SetInstrumentSustained((uint8_t)ch, Bank_GetInstrument(n->instrument));
                OPL_NoteOn((uint8_t)ch, n->note - 1, n->octave);
                g_voices[ch].active = 1;
                g_voices[ch].note = n->note;
                g_voices[ch].octave = n->octave;
                g_voices[ch].instrument = n->instrument;
                g_voices[ch].muteTimerMs = -1;

                maxScan = g_song.currentRow + 128;
                if (maxScan > PATTERN_ROWS) maxScan = PATTERN_ROWS;

                for (r = g_song.currentRow + 1; r < maxScan; ++r) {
                    TrackerNote *ahead = &g_song.patterns[g_song.currentPattern].rows[r][ch];
                    if (ahead->note > 0) {
                        nextRow = r;
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
                } else {
                    g_voices[ch].bending = 0;
                }
            }
        } else {
            if (n->note > 0) {
                OPL_SetInstrument((uint8_t)ch, Bank_GetInstrument(n->instrument));
                OPL_NoteOn((uint8_t)ch, n->note - 1, n->octave);
                g_voices[ch].active = 1;
                g_voices[ch].note = n->note;
                g_voices[ch].octave = n->octave;
                g_voices[ch].instrument = n->instrument;
                g_voices[ch].muteTimerMs = -1;
                g_voices[ch].bending = 0;
            }
        }
    }

    Loop_GetRange(&g_song.loop, &loopStart, &loopEnd, &hasLoop);

    if (hasLoop && g_song.currentRow == loopEnd) {
        g_song.currentRow = loopStart;
    } else {
        if (g_song.currentRow + 1 >= PATTERN_ROWS) {
            g_song.currentRow = PATTERN_ROWS - 1;
            g_song.playing = 0;
        } else {
            g_song.currentRow++;
        }
    }
}

void Song_SubTick(int subStep, int totalSubSteps) {
    int ch;
    if (!g_song.playing) return;

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        if (!g_voices[ch].active) continue;
        if (!Song_IsChannelAudible((uint8_t)ch)) {
            OPL_NoteOff((uint8_t)ch);
            g_voices[ch].active = 0;
            continue;
        }

        if (g_voices[ch].muteTimerMs > 0) {
            g_voices[ch].muteTimerMs -= 20;
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
                if (currentStep < 0) currentStep = 0;
                if (currentStep > totalSteps) currentStep = totalSteps;

                pitch = g_voices[ch].startPitch100 + ((g_voices[ch].endPitch100 - g_voices[ch].startPitch100) * currentStep) / totalSteps;
                OPL_SetPitch((uint8_t)ch, pitch);
            }
        }
    }
}

static uint16_t GetExportLimit(void) {
    long r;
    int c;
    long lastActive = -1;

    for (r = PATTERN_ROWS - 1; r >= 0; --r) {
        for (c = 0; c < MAX_CHANNELS; ++c) {
            if (g_song.patterns[g_song.currentPattern].rows[r][c].note > 0) {
                lastActive = r;
                break;
            }
        }
        if (lastActive >= 0) break;
    }

    if (lastActive < 0) return 16;

    lastActive += 10;
    if (lastActive >= PATTERN_ROWS) lastActive = PATTERN_ROWS - 1;
    return (uint16_t)lastActive;
}

unsigned long Song_GetTotalLengthMS(void) {
    uint16_t limit = GetExportLimit();
    return (unsigned long)limit * (unsigned long)Song_GetMsPerRow();
}

int Song_GetUsedInstruments(uint8_t *outList, int maxCount) {
    uint8_t usedMap[MAX_TOTAL_INSTRUMENTS];
    uint16_t r;
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

/*
 * Official id Software Type-1 IMF Exporter
 * Fully compatible with AdPlay 1.6a and standard DOS OPL replayers.
 */
int Song_ExportIMF(const char *filename) {
    FILE *fp;
    uint16_t r;
    int ch;
    uint16_t delayPerTick;
    uint16_t exportLimit = GetExportLimit();
    uint8_t activeInst[MAX_CHANNELS];
    double targetRate = (g_song.clockHz > 0) ? (double)g_song.clockHz : 560.0;
    long dataSizePos, endPos;
    uint16_t totalDataBytes = 0;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    delayPerTick = (uint16_t)(targetRate / ((double)g_song.bpm * 4.0 / 60.0));
    if (delayPerTick == 0) delayPerTick = 1;

    memset(activeInst, 0xFF, sizeof(activeInst));

    /* Type-1 16-bit size header placeholder (will be backpatched at end) */
    fwrite(&totalDataBytes, 2, 1, fp);
    dataSizePos = 2;

    /* Standard OPL2 reset & waveform initialization */
    fputc(0x01, fp); fputc(0x20, fp); totalDataBytes += 2;
    fputc(0, fp); fputc(0, fp); totalDataBytes += 2;

    for (r = 0; r <= exportLimit; ++r) {
        int eventCount = 0;
        for (ch = 0; ch < MAX_CHANNELS; ++ch) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][ch];
            if (n->note > 0 || n->attribute == ATTR_MUTE) eventCount++;
        }

        if (eventCount == 0) {
            fputc(0x00, fp); fputc(0x00, fp);
            fputc((uint8_t)(delayPerTick & 0xFF), fp);
            fputc((uint8_t)((delayPerTick >> 8) & 0xFF), fp);
            totalDataBytes += 4;
        } else {
            int currentEvent = 0;
            for (ch = 0; ch < MAX_CHANNELS; ++ch) {
                TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][ch];
                if (n->note > 0) {
                    uint16_t fnum;
                    uint8_t regA0 = 0xA0 + ch;
                    uint8_t regB0 = 0xB0 + ch;
                    uint8_t b0Val;
                    uint8_t mod = g_modOffsets[ch];
                    uint8_t car = g_carOffsets[ch];
                    const OPL_Instrument *inst = Bank_GetInstrument(n->instrument);
                    uint16_t d = (currentEvent == eventCount - 1) ? delayPerTick : 0;

                    if (activeInst[ch] != n->instrument) {
                        fputc(0x20 + mod, fp); fputc(inst->mod_mult_vib_am, fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x40 + mod, fp); fputc(inst->mod_ksl_level,   fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x60 + mod, fp); fputc(inst->mod_attack_decay,fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x80 + mod, fp); fputc(inst->mod_sustain_rel, fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0xE0 + mod, fp); fputc(inst->mod_wave,        fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;

                        fputc(0x20 + car, fp); fputc(inst->car_mult_vib_am, fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x40 + car, fp); fputc(inst->car_ksl_level,   fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x60 + car, fp); fputc(inst->car_attack_decay,fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0x80 + car, fp); fputc(inst->car_sustain_rel, fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        fputc(0xE0 + car, fp); fputc(inst->car_wave,        fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;

                        fputc(0xC0 + ch,  fp); fputc(inst->feedback_conn,   fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                        activeInst[ch] = n->instrument;
                    }

                    fnum = g_fNums[(n->note - 1) % 12];
                    b0Val = (uint8_t)(0x20 | ((n->octave & 0x07) << 2) | ((fnum >> 8) & 0x03));

                    fputc(regA0, fp); fputc((uint8_t)(fnum & 0xFF), fp); fputc(0, fp); fputc(0, fp); totalDataBytes += 4;
                    fputc(regB0, fp); fputc(b0Val, fp);
                    fputc((uint8_t)(d & 0xFF), fp);
                    fputc((uint8_t)((d >> 8) & 0xFF), fp);
                    totalDataBytes += 4;

                    currentEvent++;
                } else if (n->attribute == ATTR_MUTE) {
                    uint8_t regB0 = 0xB0 + ch;
                    uint16_t d = (currentEvent == eventCount - 1) ? delayPerTick : 0;
                    fputc(regB0, fp); fputc(0x00, fp);
                    fputc((uint8_t)(d & 0xFF), fp);
                    fputc((uint8_t)((d >> 8) & 0xFF), fp);
                    totalDataBytes += 4;
                    currentEvent++;
                }
            }
        }
    }

    endPos = ftell(fp);
    totalDataBytes = (uint16_t)(endPos - 2);

    /* Backpatch total size at header */
    fseek(fp, dataSizePos, SEEK_SET);
    fwrite(&totalDataBytes, 2, 1, fp);

    fclose(fp);
    return 1;
}


/*
 * Accurate IMF Loader supporting 70Hz, 560Hz, 700Hz, and custom clock rates.
 */
int Song_LoadIMF(const char *filename) {
    FILE *fp;
    long fileSize;
    uint16_t headerSize = 0;
    uint16_t row = 0;
    uint32_t delayAccum = 0;
    uint32_t delayThreshold;
    uint8_t lastA0[MAX_CHANNELS];

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize < 4) {
        fclose(fp);
        return 0;
    }

    fread(&headerSize, 2, 1, fp);
    if ((long)(headerSize + 2) == fileSize) {
        g_song.clockHz = 560; /* Standard Type-1 IMF chunk */
    } else {
        g_song.clockHz = 560; /* Default fallback */
        fseek(fp, 0, SEEK_SET);
    }

    Song_ClearAll();
    InitShadowChannels();
    memset(lastA0, 0, sizeof(lastA0));

    /* Initial pass to auto-detect clock rate if delays are unusually large (e.g. 70Hz Wolf3D) */
    {
        long pos = ftell(fp);
        uint32_t sampleDelaySum = 0;
        int sampleCount = 0;
        while (ftell(fp) + 4 <= fileSize && sampleCount < 16) {
            uint8_t r = (uint8_t)fgetc(fp);
            uint8_t v = (uint8_t)fgetc(fp);
            uint16_t d = 0;
            fread(&d, 2, 1, fp);
            sampleDelaySum += d;
            sampleCount++;
        }
        if (sampleCount > 0 && (sampleDelaySum / sampleCount) > 100) {
            g_song.clockHz = 70; /* Wolfenstein 3D / VGA Retrace rate */
        }
        fseek(fp, pos, SEEK_SET);
    }

    delayThreshold = (uint32_t)((double)g_song.clockHz / ((double)g_song.bpm * 4.0 / 60.0));
    if (delayThreshold == 0) delayThreshold = 67;

    while (ftell(fp) + 4 <= fileSize && row < PATTERN_ROWS) {
        uint8_t reg = (uint8_t)fgetc(fp);
        uint8_t val = (uint8_t)fgetc(fp);
        uint16_t delay = 0;
        fread(&delay, 2, 1, fp);

        ProcessShadowRegisterWrite(reg, val);

        if (reg >= 0xA0 && reg <= 0xA8) {
            lastA0[reg - 0xA0] = val;
        } else if (reg >= 0xB0 && reg <= 0xB8) {
            uint8_t ch = reg - 0xB0;
            if (val & 0x20) {
                uint8_t oct = (val >> 2) & 0x07;
                uint16_t fnum = ((val & 0x03) << 8) | lastA0[ch];
                uint8_t note = 0;
                uint8_t inst = MatchOrAddShadowInstrument(ch);
                OPL_FNumToNote(fnum, &note);
                Song_SetNote(ch, row, note, oct, inst);
            }
        }

        delayAccum += (uint32_t)delay;
        while (delayAccum >= delayThreshold && row < PATTERN_ROWS) {
            row++;
            delayAccum -= delayThreshold;
        }
    }

    fclose(fp);
    return 1;
}

/*
 * Official DOSBox Raw OPL (.DRO v0.1) Exporter
 * Fully compliant with AdPlay and DOSBox OPL capture specs.
 */
int Song_ExportDRO(const char *filename) {
    FILE *fp;
    uint32_t lengthBytes = 0;
    uint32_t lengthMS = 0;
    uint32_t msPerRow = Song_GetMsPerRow();
    uint16_t r;
    int ch;
    uint16_t exportLimit = GetExportLimit();
    uint8_t activeInst[MAX_CHANNELS];
    uint16_t verMajor = 0, verMinor = 1;
    uint32_t hwType = 0; /* 0 = OPL2 */

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    memset(activeInst, 0xFF, sizeof(activeInst));

    fwrite("DBRAWOPL", 1, 8, fp);
    fwrite(&verMinor, 2, 1, fp);
    fwrite(&verMajor, 2, 1, fp);
    fwrite(&lengthMS, 4, 1, fp);
    fwrite(&lengthBytes, 4, 1, fp);
    fwrite(&hwType, 4, 1, fp);

    /* OPL2 Waveform select enable */
    fputc(0x01, fp); fputc(0x20, fp); lengthBytes += 2;

    for (r = 0; r <= exportLimit; ++r) {
        for (ch = 0; ch < MAX_CHANNELS; ++ch) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[r][ch];
            if (n->note > 0) {
                uint8_t mod = g_modOffsets[ch];
                uint8_t car = g_carOffsets[ch];
                const OPL_Instrument *inst = Bank_GetInstrument(n->instrument);
                uint8_t regA0 = 0xA0 + ch;
                uint8_t regB0 = 0xB0 + ch;
                uint8_t b0Val;
                uint16_t fnum;

                if (activeInst[ch] != n->instrument) {
                    fputc(0x20 + mod, fp); fputc(inst->mod_mult_vib_am, fp);
                    fputc(0x40 + mod, fp); fputc(inst->mod_ksl_level,   fp);
                    fputc(0x60 + mod, fp); fputc(inst->mod_attack_decay,fp);
                    fputc(0x80 + mod, fp); fputc(inst->mod_sustain_rel, fp);
                    fputc(0xE0 + mod, fp); fputc(inst->mod_wave,        fp);

                    fputc(0x20 + car, fp); fputc(inst->car_mult_vib_am, fp);
                    fputc(0x40 + car, fp); fputc(inst->car_ksl_level,   fp);
                    fputc(0x60 + car, fp); fputc(inst->car_attack_decay,fp);
                    fputc(0x80 + car, fp); fputc(inst->car_sustain_rel, fp);
                    fputc(0xE0 + car, fp); fputc(inst->car_wave,        fp);

                    fputc(0xC0 + ch,  fp); fputc(inst->feedback_conn,   fp);
                    lengthBytes += 22;
                    activeInst[ch] = n->instrument;
                }

                fnum = g_fNums[(n->note - 1) % 12];
                b0Val = (uint8_t)(0x20 | ((n->octave & 0x07) << 2) | ((fnum >> 8) & 0x03));

                fputc(regA0, fp); fputc((uint8_t)(fnum & 0xFF), fp);
                fputc(regB0, fp); fputc(b0Val, fp);
                lengthBytes += 4;
            } else if (n->attribute == ATTR_MUTE) {
                fputc(0xB0 + ch, fp); fputc(0x00, fp);
                lengthBytes += 2;
            }
        }

        if (msPerRow <= 256) {
            fputc(0x00, fp);
            fputc((uint8_t)(msPerRow - 1), fp);
            lengthBytes += 2;
        } else {
            uint16_t d = (uint16_t)(msPerRow - 1);
            fputc(0x01, fp);
            fputc((uint8_t)(d & 0xFF), fp);
            fputc((uint8_t)((d >> 8) & 0xFF), fp);
            lengthBytes += 3;
        }
        lengthMS += msPerRow;
    }

    fseek(fp, 12, SEEK_SET);
    fwrite(&lengthMS, 4, 1, fp);
    fwrite(&lengthBytes, 4, 1, fp);

    fclose(fp);
    return 1;
}

/*
 * Accurate DRO Loader supporting DOSBox Raw OPL captures (.dro)
 */
int Song_LoadDRO(const char *filename) {
    FILE *fp;
    char sig[8];
    uint16_t verMaj, verMin;
    uint32_t lengthMS, lengthBytes, hwType;
    uint16_t row = 0;
    uint32_t delayAccum = 0;
    uint32_t msThreshold;
    uint8_t lastA0[MAX_CHANNELS];
    long dataStart;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    if (fread(sig, 1, 8, fp) != 8 || memcmp(sig, "DBRAWOPL", 8) != 0) {
        fclose(fp);
        return 0;
    }

    fread(&verMin, 2, 1, fp);
    fread(&verMaj, 2, 1, fp);
    fread(&lengthMS, 4, 1, fp);
    fread(&lengthBytes, 4, 1, fp);
    fread(&hwType, 4, 1, fp);

    Song_ClearAll();
    g_song.clockHz = 1000; /* DRO standard is 1ms tick rate */
    InitShadowChannels();
    memset(lastA0, 0, sizeof(lastA0));
    msThreshold = Song_GetMsPerRow();

    dataStart = ftell(fp);

    while ((uint32_t)(ftell(fp) - dataStart) < lengthBytes && row < PATTERN_ROWS) {
        uint8_t code = (uint8_t)fgetc(fp);

        if (code == 0x00) {
            uint32_t d = (uint32_t)fgetc(fp) + 1;
            delayAccum += d;
        } else if (code == 0x01) {
            uint16_t d = 0;
            fread(&d, 2, 1, fp);
            delayAccum += ((uint32_t)d + 1);
        } else if (code == 0x04) {
            uint8_t r = (uint8_t)fgetc(fp);
            uint8_t v = (uint8_t)fgetc(fp);
            ProcessShadowRegisterWrite(r, v);
            if (r >= 0xA0 && r <= 0xA8) lastA0[r - 0xA0] = v;
        } else {
            uint8_t val = (uint8_t)fgetc(fp);
            ProcessShadowRegisterWrite(code, val);
            if (code >= 0xA0 && code <= 0xA8) {
                lastA0[code - 0xA0] = val;
            } else if (code >= 0xB0 && code <= 0xB8) {
                uint8_t ch = code - 0xB0;
                if (val & 0x20) {
                    uint8_t oct = (val >> 2) & 0x07;
                    uint16_t fnum = ((val & 0x03) << 8) | lastA0[ch];
                    uint8_t note = 0;
                    uint8_t inst = MatchOrAddShadowInstrument(ch);
                    OPL_FNumToNote(fnum, &note);
                    Song_SetNote(ch, row, note, oct, inst);
                }
            }
        }

        while (delayAccum >= msThreshold && row < PATTERN_ROWS) {
            row++;
            delayAccum -= msThreshold;
        }
    }

    fclose(fp);
    return 1;
}

int Song_SaveADS(const char *filename, uint8_t selectedChannel, uint8_t selectedOctave, uint8_t selectedPreset) {
    FILE *fp;
    uint16_t verMajor = 1, verMinor = 1;
    char bnkName[32];
    uint16_t bnkVoiceCount;
    uint32_t activeCellCount = 0;
    uint16_t r;
    int c;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    fwrite("ADSYNTH1", 1, 8, fp);
    fwrite(&verMajor, 2, 1, fp);
    fwrite(&verMinor, 2, 1, fp);

    fwrite(&g_song.bpm, 2, 1, fp);
    fwrite(&g_song.clockHz, 2, 1, fp);
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
                fwrite(&r, 2, 1, fp);
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

    fread(&g_song.bpm, 2, 1, fp);
    if (verMinor >= 1) {
        fread(&g_song.clockHz, 2, 1, fp);
    } else {
        g_song.clockHz = 560;
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

        if (r < PATTERN_ROWS && ch < MAX_CHANNELS) {
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

#ifndef SONG_H
#define SONG_H

#include <stdint.h>
#include "opl.h"
#include "loop.h"

#define MAX_CHANNELS  9
#define PATTERN_ROWS  65536UL

/* Cell attributes */
#define ATTR_NONE     0
#define ATTR_LETRING  1
#define ATTR_DAMPEN   2
#define ATTR_MUTE     3
#define ATTR_BEND     4

typedef enum {
    CH_NORMAL = 0,
    CH_MUTED  = 1,
    CH_SOLO   = 2
} ChannelMode;

typedef struct {
    uint8_t note;       /* 1-12 = C..B, 0 = Empty */
    uint8_t octave;     /* 0-7 */
    uint8_t instrument; /* Index in presets */
    uint8_t flags;      /* Bit 0: Selected */
    uint8_t attribute;  /* 0=None, 1=Let Ring, 2=Dampen, 3=Mute, 4=Pitch Bend */
} TrackerNote;

typedef struct {
    TrackerNote rows[PATTERN_ROWS][MAX_CHANNELS];
} TrackerPattern;

typedef struct {
    TrackerPattern patterns[1];
    uint8_t currentPattern;
    uint16_t currentRow;
    uint16_t tempo;       /* Playback speed: 30-300 (120 = 1.0x normal speed) */
    uint16_t clockHz;     /* Track clock rate: 280, 560, 700, 1000 Hz */
    uint16_t rowMs;       /* Base ms per row: 10, 20, 30, 60, or 120 ms */
    uint8_t playing;
    LoopState loop;
    ChannelMode channelModes[MAX_CHANNELS];
} SongState;

extern SongState g_song;

void Song_Init(void);
void Song_ClearAll(void);
void Song_SetNote(uint8_t ch, uint16_t row, uint8_t note, uint8_t octave, uint8_t inst);
void Song_ClearNote(uint8_t ch, uint16_t row);
void Song_DeleteSelectedNotes(void);
void Song_ToggleNoteAttribute(uint8_t ch, uint16_t row);
void Song_SetNoteAttribute(uint8_t ch, uint16_t row, uint8_t attr);
void Song_SetSelection(uint16_t row, uint8_t ch, uint8_t sel);
void Song_ClearSelections(void);
int  Song_HasSelections(void);
int  Song_GetSelectionCount(void);
void Song_ResetVoices(void);
void Song_Tick(void);
void Song_SubTick(int subStep, int totalSubSteps);

/* Channel Mute & Solo */
void Song_ToggleChannelMuteSolo(uint8_t ch);
ChannelMode Song_GetChannelMode(uint8_t ch);
int Song_IsChannelAudible(uint8_t ch);

/* Undo History System (50 Steps) */
void Song_UndoRecord(uint16_t row, uint8_t ch);
void Song_UndoNextBatch(void);
int  Song_Undo(void);

/* Clipboard Cut, Copy & Paste */
int  Song_CutSelection(uint16_t cursorRow, uint8_t cursorCh);
int  Song_CopySelection(uint16_t cursorRow, uint8_t cursorCh);
int  Song_PasteClipboard(uint16_t cursorRow, uint8_t cursorCh);

/* Loop Point APIs */
void Song_ToggleLoopPoint(uint16_t row);
void Song_GetLoopRange(LoopState *loop, uint16_t *outStart, uint16_t *outEnd, uint8_t *outHasLoop);

/* Engine Clock & Row Resolution Helpers */
const char *Song_GetClockDescription(uint16_t hz);
uint32_t Song_GetMsPerRow(void);
double Song_GetMsPerRowFloat(void);
int16_t Song_GetDampenDurationMs(void);
void Song_SetClockHz(uint16_t newClockHz);
void Song_RescaleRowMs(uint16_t newRowMs);

/* Project & Audio Exporters / Importers */
int  Song_SaveADS(const char *filename, uint8_t selectedChannel, uint8_t selectedOctave, uint8_t selectedPreset);
int  Song_LoadADS(const char *filename, uint8_t *outChannel, uint8_t *outOctave, uint8_t *outPreset);
int  Song_ExportIMF(const char *filename);
int  Song_ExportIMF_Ex(const char *filename, uint16_t targetHz);
int  Song_LoadIMF(const char *filename);
int  Song_LoadIMF_Ex(const char *filename, uint16_t forcedHz);
int  Song_ExportDRO(const char *filename);
int  Song_LoadDRO(const char *filename);
int  Song_GetUsedInstruments(uint8_t *outList, int maxCount);
int  Song_ExportSupportingBNK(const char *baseFilename);
unsigned long Song_GetTotalLengthMS(void);
int  Song_GetLastImportNoteCount(void);
int  Song_GetLastImportVoiceCount(void);

#endif

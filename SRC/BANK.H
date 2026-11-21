#ifndef BANK_H
#define BANK_H

#include <stdint.h>

#define MAX_TOTAL_INSTRUMENTS 256
#define NUM_PRESET_INSTRUMENTS 64

typedef enum {
    CAT_KEYS_PLUCKED = 0,
    CAT_LEADS_WINDS,
    CAT_BASS,
    CAT_STRINGS_PADS,
    CAT_PERCUSSION,
    CAT_SFX_MISC,
    CAT_CUSTOM_BNK
} InstCategory;

typedef struct {
    char name[16];
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
    InstCategory category;
} OPL_Instrument;

void Bank_Init(void);
int Bank_GetTotalCount(void);
int Bank_GetBnkCount(void);
const OPL_Instrument *Bank_GetInstrument(int index);
int Bank_LoadBNK(const char *filename);
int Bank_ExportBNK(const char *filename, const uint8_t *voiceIndices, int count);
void Bank_SetCustomInstrument(int index, const OPL_Instrument *inst);
void Bank_SetLoadedBnkFilename(const char *filename);
const char *Bank_GetLoadedBnkFilename(void);

#endif

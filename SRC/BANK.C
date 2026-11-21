#include "bank.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static OPL_Instrument g_instrumentBank[MAX_TOTAL_INSTRUMENTS];
static int g_totalInstruments = NUM_PRESET_INSTRUMENTS;
static int g_loadedBnkCount = 0;
static char g_loadedBnkName[32] = "NONE";

/* 64 Pre-programmed OPL2 Default Voices */
static const OPL_Instrument FACTORY_VOICES[NUM_PRESET_INSTRUMENTS] = {
    /* 1. Keys & Plucked (12) */
    { "Rhodes Piano",  0x21, 0x20, 0xA1, 0x36, 0x00, 0x11, 0x00, 0xF2, 0x48, 0x00, 0x0A, CAT_KEYS_PLUCKED },
    { "DX7 E-Piano",   0x22, 0x24, 0x92, 0x46, 0x00, 0x11, 0x00, 0xF2, 0x58, 0x00, 0x08, CAT_KEYS_PLUCKED },
    { "Honky-Tonk",    0x21, 0x1C, 0xA1, 0x37, 0x01, 0x11, 0x00, 0xE2, 0x58, 0x00, 0x0A, CAT_KEYS_PLUCKED },
    { "Harpsichord",   0x21, 0x08, 0xF1, 0x1F, 0x00, 0x11, 0x00, 0xF1, 0x38, 0x00, 0x0E, CAT_KEYS_PLUCKED },
    { "Clavinet",      0x21, 0x00, 0xF2, 0x38, 0x02, 0x31, 0x00, 0xF1, 0x27, 0x00, 0x08, CAT_KEYS_PLUCKED },
    { "Pipe Organ",    0x21, 0x10, 0x91, 0x11, 0x00, 0x21, 0x00, 0x91, 0x11, 0x00, 0x0A, CAT_KEYS_PLUCKED },
    { "Jazz Organ",    0x21, 0x1A, 0x82, 0x46, 0x00, 0x21, 0x00, 0x92, 0x46, 0x00, 0x0A, CAT_KEYS_PLUCKED },
    { "Rock Organ",    0x61, 0x00, 0xF1, 0x11, 0x01, 0x21, 0x00, 0xF1, 0x11, 0x00, 0x0C, CAT_KEYS_PLUCKED },
    { "Acoustic Gtr",  0x22, 0x18, 0x81, 0x58, 0x00, 0x11, 0x00, 0xA2, 0x69, 0x00, 0x06, CAT_KEYS_PLUCKED },
    { "Steel Guitar",  0x31, 0x14, 0x91, 0x48, 0x00, 0x11, 0x00, 0xA2, 0x69, 0x00, 0x08, CAT_KEYS_PLUCKED },
    { "Concert Harp",  0x21, 0x1E, 0x72, 0x48, 0x00, 0x11, 0x00, 0x92, 0x48, 0x00, 0x04, CAT_KEYS_PLUCKED },
    { "Japanese Koto", 0x31, 0x10, 0xF1, 0x38, 0x01, 0x11, 0x00, 0xF1, 0x49, 0x00, 0x0C, CAT_KEYS_PLUCKED },

    /* 2. Leads & Winds (11) */
    { "Square Lead",   0x31, 0x00, 0xA2, 0x24, 0x02, 0x21, 0x00, 0x91, 0x35, 0x01, 0x06, CAT_LEADS_WINDS },
    { "Saw Lead",      0x21, 0x00, 0xA1, 0x23, 0x01, 0x21, 0x00, 0x81, 0x23, 0x01, 0x08, CAT_LEADS_WINDS },
    { "Sync Lead",     0x05, 0x00, 0xB2, 0x35, 0x00, 0x01, 0x00, 0x81, 0x24, 0x00, 0x0E, CAT_LEADS_WINDS },
    { "Soft Lead",     0x21, 0x20, 0x62, 0x24, 0x00, 0x21, 0x00, 0x62, 0x24, 0x00, 0x06, CAT_LEADS_WINDS },
    { "Trumpet",       0x22, 0x1E, 0x82, 0x47, 0x00, 0x11, 0x00, 0x82, 0x37, 0x00, 0x04, CAT_LEADS_WINDS },
    { "French Horn",   0x21, 0x24, 0x62, 0x36, 0x00, 0x11, 0x00, 0x72, 0x36, 0x00, 0x02, CAT_LEADS_WINDS },
    { "Trombone",      0x21, 0x1A, 0x92, 0x37, 0x00, 0x11, 0x00, 0x82, 0x37, 0x00, 0x04, CAT_LEADS_WINDS },
    { "Alto Sax",      0x61, 0x12, 0x82, 0x46, 0x00, 0x21, 0x00, 0x82, 0x36, 0x00, 0x06, CAT_LEADS_WINDS },
    { "Concert Flute", 0x61, 0x2A, 0x61, 0x36, 0x00, 0x21, 0x00, 0x61, 0x36, 0x00, 0x04, CAT_LEADS_WINDS },
    { "Clarinet",      0x21, 0x20, 0x72, 0x46, 0x00, 0x11, 0x00, 0x72, 0x36, 0x00, 0x04, CAT_LEADS_WINDS },
    { "Oboe",          0x31, 0x18, 0x82, 0x47, 0x00, 0x11, 0x00, 0x82, 0x37, 0x00, 0x08, CAT_LEADS_WINDS },

    /* 3. Bass (10) */
    { "Slap Bass",     0x11, 0x04, 0xF1, 0x48, 0x00, 0x11, 0x00, 0xD2, 0x59, 0x00, 0x0C, CAT_BASS },
    { "Synth Bass 1",  0x01, 0x10, 0xA2, 0x36, 0x00, 0x01, 0x00, 0x91, 0x47, 0x00, 0x0A, CAT_BASS },
    { "Synth Bass 2",  0x31, 0x00, 0x93, 0x47, 0x01, 0x01, 0x00, 0x82, 0x58, 0x00, 0x0C, CAT_BASS },
    { "Fretless Bass", 0x21, 0x18, 0x72, 0x47, 0x00, 0x01, 0x00, 0x82, 0x58, 0x00, 0x04, CAT_BASS },
    { "Upright Bass",  0x01, 0x18, 0x93, 0x75, 0x00, 0x01, 0x00, 0x82, 0x56, 0x00, 0x04, CAT_BASS },
    { "Sub Bass",      0x01, 0x14, 0x82, 0x26, 0x00, 0x01, 0x00, 0x71, 0x26, 0x00, 0x02, CAT_BASS },
    { "Picked Bass",   0x21, 0x08, 0xB1, 0x58, 0x00, 0x01, 0x00, 0x92, 0x69, 0x00, 0x0A, CAT_BASS },
    { "Reso Bass",     0x11, 0x00, 0xB2, 0x47, 0x01, 0x01, 0x00, 0x81, 0x58, 0x00, 0x0E, CAT_BASS },
    { "Acid Bass",     0x31, 0x00, 0xA1, 0x58, 0x02, 0x01, 0x00, 0x91, 0x69, 0x00, 0x0C, CAT_BASS },
    { "Organ Bass",    0x21, 0x16, 0x81, 0x35, 0x00, 0x01, 0x00, 0x81, 0x35, 0x00, 0x06, CAT_BASS },

    /* 4. Strings & Pads (11) */
    { "String Ensemb", 0x61, 0x14, 0x52, 0x45, 0x00, 0x21, 0x00, 0x62, 0x36, 0x00, 0x08, CAT_STRINGS_PADS },
    { "Slow Strings",  0x61, 0x18, 0x42, 0x35, 0x00, 0x21, 0x00, 0x52, 0x26, 0x00, 0x06, CAT_STRINGS_PADS },
    { "Solo Violin",   0x61, 0x12, 0x62, 0x46, 0x00, 0x21, 0x00, 0x72, 0x36, 0x00, 0x08, CAT_STRINGS_PADS },
    { "Solo Cello",    0x61, 0x16, 0x62, 0x46, 0x00, 0x11, 0x00, 0x72, 0x36, 0x00, 0x06, CAT_STRINGS_PADS },
    { "Synth Pad 1",   0x61, 0x16, 0x52, 0x35, 0x01, 0x21, 0x00, 0x52, 0x35, 0x01, 0x0A, CAT_STRINGS_PADS },
    { "Synth Pad 2",   0x71, 0x18, 0x41, 0x34, 0x02, 0x21, 0x00, 0x52, 0x35, 0x00, 0x08, CAT_STRINGS_PADS },
    { "Warm Sweep",    0x41, 0x1E, 0x42, 0x35, 0x00, 0x21, 0x00, 0x52, 0x26, 0x00, 0x06, CAT_STRINGS_PADS },
    { "Choir Voices",  0x61, 0x18, 0x52, 0x36, 0x00, 0x22, 0x00, 0x52, 0x36, 0x00, 0x08, CAT_STRINGS_PADS },
    { "Space Voice",   0x71, 0x14, 0x42, 0x25, 0x03, 0x21, 0x00, 0x52, 0x35, 0x02, 0x0C, CAT_STRINGS_PADS },
    { "Glass Pad",     0x61, 0x08, 0x62, 0x35, 0x01, 0x23, 0x00, 0x62, 0x35, 0x00, 0x08, CAT_STRINGS_PADS },
    { "Bowed Glass",   0x41, 0x0C, 0x42, 0x24, 0x02, 0x21, 0x00, 0x52, 0x35, 0x00, 0x0A, CAT_STRINGS_PADS },

    /* 5. Percussion (10) */
    { "Kick Drum",     0x00, 0x00, 0xF7, 0x7F, 0x00, 0x00, 0x00, 0xF8, 0x8F, 0x00, 0x0E, CAT_PERCUSSION },
    { "Snare Drum",    0x0C, 0x10, 0xF9, 0x99, 0x03, 0x01, 0x00, 0xFA, 0x89, 0x00, 0x08, CAT_PERCUSSION },
    { "Closed Hat",    0x02, 0x30, 0x18, 0x99, 0x01, 0x0E, 0x05, 0x18, 0x99, 0x03, 0x0C, CAT_PERCUSSION },
    { "Open Hat",      0x02, 0x24, 0x16, 0x88, 0x01, 0x0E, 0x05, 0x18, 0x77, 0x03, 0x0C, CAT_PERCUSSION },
    { "Tom-Tom",       0x05, 0x14, 0xA7, 0x58, 0x00, 0x01, 0x00, 0xF6, 0x77, 0x00, 0x06, CAT_PERCUSSION },
    { "Crash Cymbal",  0x07, 0x18, 0xF8, 0x48, 0x03, 0x0B, 0x00, 0xF9, 0x58, 0x03, 0x0C, CAT_PERCUSSION },
    { "Ride Cymbal",   0x03, 0x20, 0xF6, 0x37, 0x03, 0x07, 0x08, 0xF8, 0x47, 0x03, 0x08, CAT_PERCUSSION },
    { "Marimba",       0x21, 0x18, 0xF1, 0x79, 0x00, 0x11, 0x00, 0xF2, 0x7A, 0x00, 0x04, CAT_PERCUSSION },
    { "Vibraphone",    0x61, 0x10, 0xA1, 0x36, 0x00, 0x11, 0x00, 0xB2, 0x47, 0x00, 0x06, CAT_PERCUSSION },
    { "Tubular Bell",  0x01, 0x00, 0x91, 0x23, 0x03, 0x03, 0x00, 0x81, 0x14, 0x00, 0x06, CAT_PERCUSSION },

    /* 6. SFX & Misc (10) */
    { "Laser Zap",     0x0E, 0x00, 0xF1, 0x1F, 0x02, 0x01, 0x00, 0xF1, 0x1F, 0x01, 0x0E, CAT_SFX_MISC },
    { "Explosion",     0x0F, 0x00, 0xF6, 0x8F, 0x03, 0x0F, 0x00, 0xF6, 0x9F, 0x03, 0x0E, CAT_SFX_MISC },
    { "Wind Sweep",    0x6F, 0x10, 0x43, 0x36, 0x03, 0x2F, 0x00, 0x53, 0x26, 0x03, 0x0C, CAT_SFX_MISC },
    { "Siren",         0x72, 0x00, 0x81, 0x11, 0x00, 0x22, 0x00, 0x81, 0x11, 0x00, 0x0E, CAT_SFX_MISC },
    { "Phone Ring",    0x35, 0x00, 0x91, 0x11, 0x02, 0x25, 0x00, 0x91, 0x11, 0x02, 0x0C, CAT_SFX_MISC },
    { "Metal Clang",   0x0B, 0x00, 0xF1, 0x48, 0x03, 0x07, 0x00, 0xF2, 0x49, 0x00, 0x0E, CAT_SFX_MISC },
    { "Bubble Pop",    0x02, 0x08, 0xF1, 0x1F, 0x00, 0x01, 0x00, 0xF1, 0x2F, 0x00, 0x0A, CAT_SFX_MISC },
    { "Alien Chirp",   0x1E, 0x00, 0xA1, 0x25, 0x03, 0x21, 0x00, 0xB1, 0x36, 0x02, 0x0E, CAT_SFX_MISC },
    { "Teleport",      0x7E, 0x00, 0x71, 0x24, 0x01, 0x21, 0x00, 0x82, 0x35, 0x02, 0x0E, CAT_SFX_MISC },
    { "Jet Engine",    0x0F, 0x08, 0x41, 0x11, 0x03, 0x0F, 0x00, 0x51, 0x11, 0x03, 0x0E, CAT_SFX_MISC }
};

void Bank_Init(void) {
    memcpy(g_instrumentBank, FACTORY_VOICES, sizeof(FACTORY_VOICES));
    g_totalInstruments = NUM_PRESET_INSTRUMENTS;
    g_loadedBnkCount = 0;
    strcpy(g_loadedBnkName, "NONE");
}

int Bank_GetTotalCount(void) {
    return g_totalInstruments;
}

int Bank_GetBnkCount(void) {
    return g_loadedBnkCount;
}

const OPL_Instrument *Bank_GetInstrument(int index) {
    if (index < 0 || index >= g_totalInstruments) {
        return &g_instrumentBank[0];
    }
    return &g_instrumentBank[index];
}

void Bank_SetCustomInstrument(int index, const OPL_Instrument *inst) {
    if (index >= NUM_PRESET_INSTRUMENTS && index < MAX_TOTAL_INSTRUMENTS && inst) {
        g_instrumentBank[index] = *inst;
        if (index >= g_totalInstruments) {
            g_totalInstruments = index + 1;
            g_loadedBnkCount = g_totalInstruments - NUM_PRESET_INSTRUMENTS;
        }
    }
}

void Bank_SetLoadedBnkFilename(const char *filename) {
    if (filename) {
        strncpy(g_loadedBnkName, filename, 31);
        g_loadedBnkName[31] = '\0';
    }
}

const char *Bank_GetLoadedBnkFilename(void) {
    return g_loadedBnkName;
}

int Bank_LoadBNK(const char *filename) {
    FILE *fp;
    uint8_t verMajor, verMinor;
    char sig[6];
    uint16_t numUsed, numInstruments;
    uint32_t offsetName, offsetData;
    uint16_t i;
    int loaded = 0;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    verMajor = (uint8_t)fgetc(fp);
    verMinor = (uint8_t)fgetc(fp);
    if (fread(sig, 1, 6, fp) != 6) { fclose(fp); return 0; }

    if (memcmp(sig, "ADLIB-", 6) != 0) {
        fclose(fp);
        return 0;
    }

    fread(&numUsed, 2, 1, fp);
    fread(&numInstruments, 2, 1, fp);
    fread(&offsetName, 4, 1, fp);
    fread(&offsetData, 4, 1, fp);

    if (numInstruments == 0 || offsetData == 0) {
        fclose(fp);
        return 0;
    }

    g_totalInstruments = NUM_PRESET_INSTRUMENTS;
    g_loadedBnkCount = 0;

    for (i = 0; i < numInstruments && g_totalInstruments < MAX_TOTAL_INSTRUMENTS; ++i) {
        uint16_t dataIdx;
        uint8_t flags;
        char name[10];
        long namePos = offsetName + (long)i * 12;
        long dataPos;
        uint8_t iPerc, iVoice;
        uint8_t mod[13], car[13];
        uint8_t modWave, carWave;
        OPL_Instrument inst;

        fseek(fp, namePos, SEEK_SET);
        fread(&dataIdx, 2, 1, fp);
        flags = (uint8_t)fgetc(fp);
        memset(name, 0, sizeof(name));
        fread(name, 1, 9, fp);
        name[9] = '\0';

        if (flags == 0 && name[0] == '\0') continue;

        dataPos = offsetData + (long)dataIdx * 30;
        fseek(fp, dataPos, SEEK_SET);

        iPerc = (uint8_t)fgetc(fp);
        iVoice = (uint8_t)fgetc(fp);
        fread(mod, 1, 13, fp);
        fread(car, 1, 13, fp);
        modWave = (uint8_t)fgetc(fp);
        carWave = (uint8_t)fgetc(fp);

        memset(&inst, 0, sizeof(OPL_Instrument));
        strncpy(inst.name, name, 15);
        inst.name[15] = '\0';

        inst.mod_mult_vib_am = (uint8_t)((mod[9] ? 0x80 : 0) | (mod[10] ? 0x40 : 0) | (mod[5] ? 0x20 : 0) | (mod[11] ? 0x10 : 0) | (mod[1] & 0x0F));
        inst.mod_ksl_level   = (uint8_t)(((mod[0] & 0x03) << 6) | (mod[8] & 0x3F));
        inst.mod_attack_decay= (uint8_t)(((mod[3] & 0x0F) << 4) | (mod[6] & 0x0F));
        inst.mod_sustain_rel = (uint8_t)(((mod[4] & 0x0F) << 4) | (mod[7] & 0x0F));
        inst.mod_wave        = (uint8_t)(modWave & 0x07);

        inst.car_mult_vib_am = (uint8_t)((car[9] ? 0x80 : 0) | (car[10] ? 0x40 : 0) | (car[5] ? 0x20 : 0) | (car[11] ? 0x10 : 0) | (car[1] & 0x0F));
        inst.car_ksl_level   = (uint8_t)(((car[0] & 0x03) << 6) | (car[8] & 0x3F));
        inst.car_attack_decay= (uint8_t)(((car[3] & 0x0F) << 4) | (car[6] & 0x0F));
        inst.car_sustain_rel = (uint8_t)(((car[4] & 0x0F) << 4) | (car[7] & 0x0F));
        inst.car_wave        = (uint8_t)(carWave & 0x07);

        inst.feedback_conn   = (uint8_t)(((mod[2] & 0x07) << 1) | (mod[12] & 0x01));
        inst.category        = CAT_CUSTOM_BNK;

        g_instrumentBank[g_totalInstruments] = inst;
        g_totalInstruments++;
        g_loadedBnkCount++;
        loaded++;
    }

    fclose(fp);
    strncpy(g_loadedBnkName, filename, 31);
    g_loadedBnkName[31] = '\0';
    return loaded;
}

/*
 * Official AdLib Visual Composer .BNK Exporter
 * Exports selected voices into an authentic Shikadi-compliant binary bank.
 */
int Bank_ExportBNK(const char *filename, const uint8_t *voiceIndices, int count) {
    FILE *fp;
    uint8_t verMajor = 1, verMinor = 0;
    uint16_t numUsed, numInstruments;
    uint32_t offsetName, offsetData;
    int i;

    if (!filename || !voiceIndices || count <= 0) return 0;
    if (count > 256) count = 256;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    numUsed = (uint16_t)count;
    numInstruments = (uint16_t)count;
    offsetName = 20;
    offsetData = offsetName + ((uint32_t)count * 12);

    fputc(verMajor, fp);
    fputc(verMinor, fp);
    fwrite("ADLIB-", 1, 6, fp);
    fwrite(&numUsed, 2, 1, fp);
    fwrite(&numInstruments, 2, 1, fp);
    fwrite(&offsetName, 4, 1, fp);
    fwrite(&offsetData, 4, 1, fp);

    /* Write 12-byte Names directory table */
    for (i = 0; i < count; ++i) {
        uint16_t dataIdx = (uint16_t)i;
        uint8_t flags = 1;
        char name[9];
        const OPL_Instrument *inst = Bank_GetInstrument(voiceIndices[i]);

        memset(name, 0, sizeof(name));
        strncpy(name, inst->name, 8);

        fwrite(&dataIdx, 2, 1, fp);
        fputc(flags, fp);
        fwrite(name, 1, 9, fp);
    }

    /* Write 30-byte Timbre records table */
    for (i = 0; i < count; ++i) {
        const OPL_Instrument *inst = Bank_GetInstrument(voiceIndices[i]);
        uint8_t mod[13], car[13];

        fputc(0, fp); /* iPerc: Melodic */
        fputc(0, fp); /* iVoice */

        mod[0]  = (uint8_t)((inst->mod_ksl_level >> 6) & 0x03);
        mod[1]  = (uint8_t)(inst->mod_mult_vib_am & 0x0F);
        mod[2]  = (uint8_t)((inst->feedback_conn >> 1) & 0x07);
        mod[3]  = (uint8_t)((inst->mod_attack_decay >> 4) & 0x0F);
        mod[4]  = (uint8_t)((inst->mod_sustain_rel >> 4) & 0x0F);
        mod[5]  = (uint8_t)((inst->mod_mult_vib_am & 0x20) ? 1 : 0);
        mod[6]  = (uint8_t)(inst->mod_attack_decay & 0x0F);
        mod[7]  = (uint8_t)(inst->mod_sustain_rel & 0x0F);
        mod[8]  = (uint8_t)(inst->mod_ksl_level & 0x3F);
        mod[9]  = (uint8_t)((inst->mod_mult_vib_am & 0x80) ? 1 : 0);
        mod[10] = (uint8_t)((inst->mod_mult_vib_am & 0x40) ? 1 : 0);
        mod[11] = (uint8_t)((inst->mod_mult_vib_am & 0x10) ? 1 : 0);
        mod[12] = (uint8_t)(inst->feedback_conn & 0x01);
        fwrite(mod, 1, 13, fp);

        car[0]  = (uint8_t)((inst->car_ksl_level >> 6) & 0x03);
        car[1]  = (uint8_t)(inst->car_mult_vib_am & 0x0F);
        car[2]  = 0;
        car[3]  = (uint8_t)((inst->car_attack_decay >> 4) & 0x0F);
        car[4]  = (uint8_t)((inst->car_sustain_rel >> 4) & 0x0F);
        car[5]  = (uint8_t)((inst->car_mult_vib_am & 0x20) ? 1 : 0);
        car[6]  = (uint8_t)(inst->car_attack_decay & 0x0F);
        car[7]  = (uint8_t)(inst->car_sustain_rel & 0x0F);
        car[8]  = (uint8_t)(inst->car_ksl_level & 0x3F);
        car[9]  = (uint8_t)((inst->car_mult_vib_am & 0x80) ? 1 : 0);
        car[10] = (uint8_t)((inst->car_mult_vib_am & 0x40) ? 1 : 0);
        car[11] = (uint8_t)((inst->car_mult_vib_am & 0x10) ? 1 : 0);
        car[12] = 0;
        fwrite(car, 1, 13, fp);

        fputc((uint8_t)(inst->mod_wave & 0x07), fp);
        fputc((uint8_t)(inst->car_wave & 0x07), fp);
    }

    fclose(fp);
    return 1;
}

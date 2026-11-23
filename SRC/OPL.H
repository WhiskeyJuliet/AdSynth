#ifndef OPL_H
#define OPL_H

#include <stdint.h>
#include "bank.h"

#define OPL_BASE_PORT 0x388

void OPL_Init(uint16_t basePort);
void OPL_Shutdown(void);
void OPL_Reset(void);
void OPL_Write(uint8_t reg, uint8_t val);

void OPL_SetInstrument(uint8_t channel, const OPL_Instrument *inst);
void OPL_SetInstrumentSustained(uint8_t channel, const OPL_Instrument *inst);
void OPL_NoteOn(uint8_t channel, uint8_t note, uint8_t octave);
void OPL_NoteOff(uint8_t channel);
void OPL_SetPitch(uint8_t channel, int32_t pitch100);

int OPL_FNumToNote(uint16_t fnum, uint8_t *outNote);
void OPL_FNumBlockToNoteOct(uint16_t fnum, uint8_t block, uint8_t *outNote, uint8_t *outOct);

#endif

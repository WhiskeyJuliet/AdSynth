#ifndef LOOP_H
#define LOOP_H

#include <stdint.h>

typedef struct {
    int16_t loopPoint1; /* -1 if disabled, otherwise row index */
    int16_t loopPoint2; /* -1 if disabled, otherwise row index */
} LoopState;

void Loop_Init(LoopState *loop);
void Loop_Toggle(LoopState *loop, uint16_t row);
void Loop_GetRange(const LoopState *loop, uint16_t *outStart, uint16_t *outEnd, uint8_t *outHasLoop);

#endif

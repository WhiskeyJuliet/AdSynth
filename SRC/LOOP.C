#include "loop.h"

void Loop_Init(LoopState *loop) {
    if (!loop) return;
    loop->loopPoint1 = -1;
    loop->loopPoint2 = -1;
}

void Loop_Toggle(LoopState *loop, uint16_t row) {
    if (!loop) return;
    if (loop->loopPoint1 == (int16_t)row) {
        loop->loopPoint1 = -1;
    } else if (loop->loopPoint2 == (int16_t)row) {
        loop->loopPoint2 = -1;
    } else {
        if (loop->loopPoint1 < 0) {
            loop->loopPoint1 = (int16_t)row;
        } else if (loop->loopPoint2 < 0) {
            loop->loopPoint2 = (int16_t)row;
        } else {
            loop->loopPoint2 = (int16_t)row;
        }
    }
}

void Loop_GetRange(const LoopState *loop, uint16_t *outStart, uint16_t *outEnd, uint8_t *outHasLoop) {
    int16_t p1, p2;
    if (!loop) {
        *outHasLoop = 0;
        *outStart = 0;
        *outEnd = 0;
        return;
    }
    p1 = loop->loopPoint1;
    p2 = loop->loopPoint2;

    if (p1 < 0 && p2 < 0) {
        *outHasLoop = 0;
        *outStart = 0;
        *outEnd = 0;
    } else if (p1 >= 0 && p2 < 0) {
        *outHasLoop = 1;
        *outStart = 0;
        *outEnd = (uint16_t)p1;
    } else if (p1 < 0 && p2 >= 0) {
        *outHasLoop = 1;
        *outStart = 0;
        *outEnd = (uint16_t)p2;
    } else {
        *outHasLoop = 1;
        if (p1 < p2) {
            *outStart = (uint16_t)p1;
            *outEnd = (uint16_t)p2;
        } else {
            *outStart = (uint16_t)p2;
            *outEnd = (uint16_t)p1;
        }
    }
}

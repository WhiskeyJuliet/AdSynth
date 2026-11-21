#include "ui.h"
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

uint8_t *g_vgaBuffer = NULL;
static uint8_t *g_vgaPhysical = (uint8_t *)0xA0000;
MouseState g_mouse = {160, 100, 0, 0};

typedef struct {
    int width;
    int height;
    uint8_t *pixels;
} PCX_Image;

static PCX_Image g_titlePcx = {0, 0, NULL};
static PCX_Image g_splashPcx = {0, 0, NULL};
static PCX_Image g_fairyPcx = {0, 0, NULL};
static PCX_Image g_cursorPcx = {0, 0, NULL};

/* 5x7 Font: Covers ASCII 32 (' ') to 126 ('~') */
static const uint8_t FONT5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32: Space */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33: ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34: " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35: # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36: $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37: % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38: & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39: ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40: ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41: ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* 42: * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43: + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44: , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45: - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46: . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47: / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48: 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49: 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50: 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51: 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52: 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53: 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54: 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55: 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56: 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57: 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58: : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59: ; */
    {0x08,0x14,0x22,0x41,0x00}, /* 60: < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61: = */
    {0x00,0x41,0x22,0x14,0x08}, /* 62: > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63: ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64: @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65: A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66: B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67: C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68: D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69: E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70: F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71: G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72: H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73: I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74: J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75: K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76: L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 77: M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78: N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79: O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80: P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81: Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82: R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83: S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84: T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85: U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86: V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 87: W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88: X */
    {0x07,0x08,0x70,0x08,0x07}, /* 89: Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90: Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91: [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92: \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93: ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94: ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95: _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96: ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97: a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98: b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99: c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100: d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101: e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102: f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 103: g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104: h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105: i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106: j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107: k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108: l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109: m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110: n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111: o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112: p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113: q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114: r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115: s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116: t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117: u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118: v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119: w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120: x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121: y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122: z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123: { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124: | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125: } */
    {0x08,0x04,0x08,0x10,0x08}  /* 126: ~ */
};

/* 3x5 Mini Font */
static const uint8_t FONT3x5[][3] = {
    {0x00,0x00,0x00}, {0x00,0x17,0x00}, {0x03,0x00,0x03}, {0x1F,0x0A,0x1F},
    {0x12,0x1F,0x09}, {0x19,0x04,0x13}, {0x0A,0x15,0x0A}, {0x00,0x03,0x00},
    {0x0E,0x11,0x00}, {0x00,0x11,0x0E}, {0x0A,0x04,0x0A}, {0x04,0x0E,0x04},
    {0x10,0x08,0x00}, {0x04,0x04,0x04}, {0x10,0x00,0x00}, {0x18,0x04,0x03},
    {0x1F,0x11,0x1F}, {0x00,0x1F,0x00}, {0x1D,0x15,0x17}, {0x15,0x15,0x1F},
    {0x07,0x04,0x1F}, {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F},
    {0x1F,0x15,0x1F}, {0x17,0x15,0x1F}, {0x00,0x0A,0x00}, {0x10,0x0A,0x00},
    {0x04,0x0A,0x11}, {0x0A,0x0A,0x0A}, {0x11,0x0A,0x04}, {0x01,0x15,0x02},
    {0x0E,0x15,0x0D}, {0x1E,0x05,0x1E}, {0x1F,0x15,0x0A}, {0x0E,0x11,0x11},
    {0x1F,0x11,0x0E}, {0x1F,0x15,0x11}, {0x1F,0x05,0x01}, {0x0E,0x11,0x1D},
    {0x1F,0x04,0x1F}, {0x11,0x1F,0x11}, {0x08,0x10,0x0F}, {0x1F,0x04,0x1B},
    {0x1F,0x10,0x10}, {0x1F,0x02,0x1F}, {0x1F,0x06,0x1F}, {0x0E,0x11,0x0E},
    {0x1F,0x05,0x02}, {0x0E,0x11,0x1E}, {0x1F,0x05,0x1A}, {0x12,0x15,0x09},
    {0x01,0x1F,0x01}, {0x0F,0x10,0x0F}, {0x07,0x18,0x07}, {0x1F,0x08,0x1F},
    {0x1B,0x04,0x1B}, {0x03,0x1C,0x03}, {0x19,0x15,0x13}
};

static void SetVGAMode(uint8_t mode) {
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = mode;
    int386(0x10, &regs, &regs);
}

static void SetupPalette(void) {
    int i;
    outp(0x3C8, 0);
    outp(0x3C9, 4);  outp(0x3C9, 4);  outp(0x3C9, 6);   /* 0: BG */
    outp(0x3C9, 10); outp(0x3C9, 10); outp(0x3C9, 13); /* 1: Panel */
    outp(0x3C9, 18); outp(0x3C9, 18); outp(0x3C9, 22); /* 2: Border */
    outp(0x3C9, 0);  outp(0x3C9, 52); outp(0x3C9, 58); /* 3: Cyan */
    outp(0x3C9, 60); outp(0x3C9, 38); outp(0x3C9, 0);  /* 4: Amber */
    outp(0x3C9, 12); outp(0x3C9, 60); outp(0x3C9, 15); /* 5: Lime */
    outp(0x3C9, 42); outp(0x3C9, 42); outp(0x3C9, 48); /* 6: Dim Text */
    outp(0x3C9, 63); outp(0x3C9, 63); outp(0x3C9, 63); /* 7: White Text */
    outp(0x3C9, 60); outp(0x3C9, 10); outp(0x3C9, 15); /* 8: Red */
    outp(0x3C9, 10); outp(0x3C9, 58); outp(0x3C9, 15); /* 9: CRT Green */
    outp(0x3C9, 60); outp(0x3C9, 28); outp(0x3C9, 0);  /* 10: Orange */
    outp(0x3C9, 63); outp(0x3C9, 60); outp(0x3C9, 10); /* 11: Bright Yellow */
    outp(0x3C9, 4);  outp(0x3C9, 4);  outp(0x3C9, 6);   /* 12: Shadow */

    /* Custom Extended Palette: Index 13 & 14 */
    outp(0x3C9, 8);  outp(0x3C9, 8);  outp(0x3C9, 11); /* 13: Dark BG */
    outp(0x3C9, 13); outp(0x3C9, 13); outp(0x3C9, 16); /* 14: Light BG */

    for (i = 15; i < 32; ++i) {
        uint8_t val = (uint8_t)((i - 15) * 2.5);
        outp(0x3C9, val); outp(0x3C9, val); outp(0x3C9, val);
    }
}

int Mouse_Init(void) {
    union REGS regs;
    regs.w.ax = 0x0000;
    int386(0x33, &regs, &regs);
    return regs.w.ax == 0xFFFF;
}

void Mouse_Update(void) {
    union REGS regs;
    regs.w.ax = 0x0003;
    int386(0x33, &regs, &regs);
    g_mouse.x = regs.w.cx >> 1;
    g_mouse.y = regs.w.dx;
    g_mouse.leftDown = (regs.w.bx & 1) != 0;
    g_mouse.rightDown = (regs.w.bx & 2) != 0;
}

static int LoadGenericPCX(const char *filename, PCX_Image *img) {
    FILE *fp;
    uint8_t hdr[128];
    uint16_t xmin, ymin, xmax, ymax;
    uint16_t bytesPerLine;
    uint8_t nPlanes;
    int width, height;
    int totalBytes;
    uint8_t *scanline;
    uint8_t *buffer;
    int y;

    fp = fopen(filename, "rb");
    if (!fp) return 0;

    if (fread(hdr, 1, 128, fp) != 128) {
        fclose(fp);
        return 0;
    }

    if (hdr[0] != 0x0A) {
        fclose(fp);
        return 0;
    }

    xmin         = (uint16_t)(hdr[4]  | ((uint16_t)hdr[5]  << 8));
    ymin         = (uint16_t)(hdr[6]  | ((uint16_t)hdr[7]  << 8));
    xmax         = (uint16_t)(hdr[8]  | ((uint16_t)hdr[9]  << 8));
    ymax         = (uint16_t)(hdr[10] | ((uint16_t)hdr[11] << 8));
    nPlanes      = hdr[65];
    bytesPerLine = (uint16_t)(hdr[66] | ((uint16_t)hdr[67] << 8));

    if (xmax < xmin || ymax < ymin) {
        fclose(fp);
        return 0;
    }

    width = (int)(xmax - xmin + 1);
    height = (int)(ymax - ymin + 1);

    if (nPlanes == 0) nPlanes = 1;
    totalBytes = (int)bytesPerLine * (int)nPlanes;

    if (width <= 0 || height <= 0 || totalBytes < width) {
        fclose(fp);
        return 0;
    }

    buffer = (uint8_t *)malloc(width * height);
    if (!buffer) {
        fclose(fp);
        return 0;
    }

    scanline = (uint8_t *)malloc(totalBytes);
    if (!scanline) {
        free(buffer);
        fclose(fp);
        return 0;
    }

    for (y = 0; y < height; ++y) {
        int x = 0;
        while (x < totalBytes) {
            int b = fgetc(fp);
            if (b == EOF) break;

            if ((b & 0xC0) == 0xC0) {
                int count = b & 0x3F;
                int val = fgetc(fp);
                if (val == EOF) break;
                while (count > 0 && x < totalBytes) {
                    scanline[x++] = (uint8_t)val;
                    count--;
                }
            } else {
                scanline[x++] = (uint8_t)b;
            }
        }
        memcpy(buffer + (y * width), scanline, width);
    }

    free(scanline);
    fclose(fp);

    if (img->pixels) {
        free(img->pixels);
    }
    img->width = width;
    img->height = height;
    img->pixels = buffer;
    return 1;
}

int UI_LoadTitlePCX(const char *filename) {
    return LoadGenericPCX(filename, &g_titlePcx);
}

int UI_LoadSplashPCX(const char *filename) {
    return LoadGenericPCX(filename, &g_splashPcx);
}

int UI_LoadFairyPCX(const char *filename) {
    return LoadGenericPCX(filename, &g_fairyPcx);
}

int UI_LoadCursorPCX(const char *filename) {
    return LoadGenericPCX(filename, &g_cursorPcx);
}

void UI_Init(void) {
    g_vgaBuffer = (uint8_t *)malloc(SCREEN_W * SCREEN_H);
    SetVGAMode(0x13);
    SetupPalette();
    Mouse_Init();

    if (!UI_LoadTitlePCX("ADSYNTH.PCX")) UI_LoadTitlePCX("BUILD\\ADSYNTH.PCX");
    if (!UI_LoadSplashPCX("SPLASH.PCX")) UI_LoadSplashPCX("BUILD\\SPLASH.PCX");
    if (!UI_LoadFairyPCX("FAIRY.PCX"))   UI_LoadFairyPCX("BUILD\\FAIRY.PCX");
    if (!UI_LoadCursorPCX("CURSOR.PCX")) UI_LoadCursorPCX("BUILD\\CURSOR.PCX");
}

void UI_Shutdown(void) {
    if (g_titlePcx.pixels)  { free(g_titlePcx.pixels);  g_titlePcx.pixels = NULL; }
    if (g_splashPcx.pixels) { free(g_splashPcx.pixels); g_splashPcx.pixels = NULL; }
    if (g_fairyPcx.pixels)  { free(g_fairyPcx.pixels);  g_fairyPcx.pixels = NULL; }
    if (g_cursorPcx.pixels) { free(g_cursorPcx.pixels); g_cursorPcx.pixels = NULL; }
    if (g_vgaBuffer) {
        free(g_vgaBuffer);
        g_vgaBuffer = NULL;
    }
    SetVGAMode(0x03);
}

void UI_Clear(uint8_t color) {
    memset(g_vgaBuffer, color, SCREEN_W * SCREEN_H);
}

void UI_Present(void) {
    uint32_t *d;
    const uint32_t *s;
    int i;

    while ((inp(0x3DA) & 0x08));
    while (!(inp(0x3DA) & 0x08));

    d = (uint32_t *)g_vgaPhysical;
    s = (const uint32_t *)g_vgaBuffer;
    for (i = 0; i < 16000; ++i) {
        d[i] = s[i];
    }
}

void UI_FillRect(int x, int y, int w, int h, uint8_t color) {
    int row, startX, endX, len;
    uint8_t *dst;

    if (x >= SCREEN_W || y >= SCREEN_H) return;
    startX = x < 0 ? 0 : x;
    endX = (x + w) > SCREEN_W ? SCREEN_W : (x + w);
    len = endX - startX;
    if (len <= 0) return;

    for (row = y; row < y + h; ++row) {
        if (row >= 0 && row < SCREEN_H) {
            dst = g_vgaBuffer + (row * SCREEN_W) + startX;
            memset(dst, color, len);
        }
    }
}

void UI_DrawRect(int x, int y, int w, int h, uint8_t color) {
    int i;
    for (i = x; i < x + w; ++i) {
        if (y >= 0 && y < SCREEN_H && i >= 0 && i < SCREEN_W) g_vgaBuffer[y * SCREEN_W + i] = color;
        if (y + h - 1 >= 0 && y + h - 1 < SCREEN_H && i >= 0 && i < SCREEN_W) g_vgaBuffer[(y + h - 1) * SCREEN_W + i] = color;
    }
    for (i = y; i < y + h; ++i) {
        if (x >= 0 && x < SCREEN_W && i >= 0 && i < SCREEN_H) g_vgaBuffer[i * SCREEN_W + x] = color;
        if (x + w - 1 >= 0 && x + w - 1 < SCREEN_W && i >= 0 && i < SCREEN_H) g_vgaBuffer[i * SCREEN_W + (x + w - 1)] = color;
    }
}

void UI_DrawDashedRect(int x, int y, int w, int h, uint8_t color) {
    int i;
    for (i = x; i < x + w; ++i) {
        if ((i & 2) != 0) {
            if (y >= 0 && y < SCREEN_H && i >= 0 && i < SCREEN_W) g_vgaBuffer[y * SCREEN_W + i] = color;
            if (y + h - 1 >= 0 && y + h - 1 < SCREEN_H && i >= 0 && i < SCREEN_W) g_vgaBuffer[(y + h - 1) * SCREEN_W + i] = color;
        }
    }
    for (i = y; i < y + h; ++i) {
        if ((i & 2) != 0) {
            if (x >= 0 && x < SCREEN_W && i >= 0 && i < SCREEN_H) g_vgaBuffer[i * SCREEN_W + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < SCREEN_W && i >= 0 && i < SCREEN_H) g_vgaBuffer[i * SCREEN_W + (x + w - 1)] = color;
        }
    }
}

void UI_DrawDitherOverlay(int x, int y, int w, int h) {
    static const uint8_t bayer[4][4] = {
        { 0,  8,  2, 10 },
        { 12, 4, 14,  6 },
        { 3, 11,  1,  9 },
        { 15, 7, 13,  5 }
    };
    int r, c;
    for (r = y; r < y + h; ++r) {
        if (r >= 0 && r < SCREEN_H) {
            uint8_t *rowPtr = g_vgaBuffer + r * SCREEN_W;
            for (c = x; c < x + w; ++c) {
                if (c >= 0 && c < SCREEN_W) {
                    if (bayer[r & 3][c & 3] < 5) {
                        rowPtr[c] = 2;
                    }
                }
            }
        }
    }
}

void UI_PrintText(int x, int y, const char *text, uint8_t color) {
    int cursorX = x;
    while (*text) {
        unsigned char ch = (unsigned char)*text++;

        if (ch == 1) {
            static const uint8_t logo7x7[7] = {0x10, 0x38, 0x38, 0x1F, 0x15, 0x05, 0x01};
            int col, row;
            for (col = 0; col < 7; ++col) {
                uint8_t bits = logo7x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 8;
        } else if (ch == 2) {
            static const uint8_t play7x7[7] = {0x7E, 0x3C, 0x18, 0x10, 0x00, 0x00, 0x00};
            int col, row;
            for (col = 0; col < 7; ++col) {
                uint8_t bits = play7x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 8;
        } else if (ch == 3) {
            static const uint8_t stop7x7[7] = {0x00, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x00};
            int col, row;
            for (col = 0; col < 7; ++col) {
                uint8_t bits = stop7x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 8;
        } else if (ch == 4) {
            static const uint8_t close8x7[8] = {0x41, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x41};
            int col, row;
            for (col = 0; col < 8; ++col) {
                uint8_t bits = close8x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 9;
        } else if (ch == 5) {
            static const uint8_t leftArrow10x7[10] = {0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x00};
            int col, row;
            for (col = 0; col < 10; ++col) {
                uint8_t bits = leftArrow10x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 11;
        } else if (ch == 6) {
            static const uint8_t rightArrow10x7[10] = {0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C, 0x08};
            int col, row;
            for (col = 0; col < 10; ++col) {
                uint8_t bits = rightArrow10x7[col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        int px = cursorX + col;
                        int py = y + row;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            g_vgaBuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
            cursorX += 11;
        } else {
            int fontIndex = 0;
            if (ch >= 32 && ch <= 126) {
                fontIndex = ch - 32;
            } else {
                fontIndex = 0;
            }

            if (y >= 0 && (y + 7) <= SCREEN_H && cursorX >= 0 && (cursorX + 5) <= SCREEN_W) {
                const uint8_t *glyph = FONT5x7[fontIndex];
                uint8_t *dest = g_vgaBuffer + y * SCREEN_W + cursorX;
                int col;

                for (col = 0; col < 5; ++col) {
                    uint8_t b = glyph[col];
                    if (b & 0x01) dest[0 * 320 + col] = color;
                    if (b & 0x02) dest[1 * 320 + col] = color;
                    if (b & 0x04) dest[2 * 320 + col] = color;
                    if (b & 0x08) dest[3 * 320 + col] = color;
                    if (b & 0x10) dest[4 * 320 + col] = color;
                    if (b & 0x20) dest[5 * 320 + col] = color;
                    if (b & 0x40) dest[6 * 320 + col] = color;
                }
            }
            cursorX += 6;
        }
    }
}

void UI_PrintTextMini(int x, int y, const char *text, uint8_t color) {
    int cursorX = x;
    while (*text) {
        unsigned char ch = (unsigned char)*text++;
        int fontIndex = 0;

        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch >= 32 && ch <= 90) {
            fontIndex = ch - 32;
        } else {
            fontIndex = 0;
        }

        if (y >= 0 && (y + 5) <= SCREEN_H && cursorX >= 0 && (cursorX + 3) <= SCREEN_W) {
            const uint8_t *glyph = FONT3x5[fontIndex];
            uint8_t *dest = g_vgaBuffer + y * SCREEN_W + cursorX;
            int col;

            for (col = 0; col < 3; ++col) {
                uint8_t b = glyph[col];
                if (b & 0x01) dest[0 * 320 + col] = color;
                if (b & 0x02) dest[1 * 320 + col] = color;
                if (b & 0x04) dest[2 * 320 + col] = color;
                if (b & 0x08) dest[3 * 320 + col] = color;
                if (b & 0x10) dest[4 * 320 + col] = color;
            }
        }
        cursorX += 4;
    }
}

static void UI_DrawPianoRoll(void) {
    int key;
    for (key = 0; key < 14; ++key) {
        int kx = 10 + key * 21;
        UI_FillRect(kx, 160, 20, 36, 31);
        UI_DrawRect(kx, 160, 20, 36, 1);
    }
    for (key = 0; key < 13; ++key) {
        int noteInOctave = key % 7;
        if (noteInOctave != 2 && noteInOctave != 6) {
            int kx = 10 + key * 21 + 14;
            UI_FillRect(kx, 160, 12, 22, 1);
            UI_DrawRect(kx, 160, 12, 22, 0);
        }
    }
}

void UI_DrawApp(int selectedChannel, int selectedOctave, int selectedPreset, int cursorRow) {
    char buf[64];
    int r, ch;
    int startRow;
    unsigned long total_ms;
    unsigned long total_secs;
    unsigned int minutes, seconds, hundredths;
    uint16_t loopStart, loopEnd;
    uint8_t hasLoop;
    int logoHover, helpHover;
    int isCursor;
    int py, px, sy;
    uint8_t *dst;
    const uint8_t *src;
    uint8_t col;

    UI_Clear(0);

    /* Top Navigation Bar */
    UI_FillRect(0, 0, SCREEN_W, 14, 1);
    UI_DrawRect(0, 0, SCREEN_W, 14, 2);

    /* AdLib Logo (0..13) */
    logoHover = (g_mouse.x >= 2 && g_mouse.x <= 13 && g_mouse.y >= 2 && g_mouse.y <= 12);
    if (logoHover) {
        UI_PrintText(4, 3, "\x01", 0);
        UI_PrintText(4, 4, "\x01", 7);
    } else {
        UI_PrintText(4, 4, "\x01", 0);
        UI_PrintText(4, 3, "\x01", 7);
    }

    /* Program Title: ADSYNTH.PCX (14..56) */
    if (g_titlePcx.pixels && g_titlePcx.width == 41 && g_titlePcx.height == 8) {
        for (py = 0; py < 8; ++py) {
            sy = 3 + py;
            if (sy >= 0 && sy < SCREEN_H) {
                dst = g_vgaBuffer + sy * SCREEN_W;
                src = g_titlePcx.pixels + py * 41;
                for (px = 0; px < 41; ++px) {
                    col = src[px];
                    if (col != 0) dst[14 + px] = col;
                }
            }
        }
    } else {
        UI_PrintText(14, 4, "ADSYNTH", 3);
    }

    /* Octave (62..94) */
    sprintf(buf, "OCT:%d", selectedOctave);
    UI_PrintText(62, 3, buf, 7);

    /* Instrument Preset (98..142) */
    sprintf(buf, "INST:%02d", selectedPreset + 1);
    UI_PrintText(98, 3, buf, 4);

    /* BPM (148..190) */
    sprintf(buf, "BPM:%d", g_song.bpm);
    UI_PrintText(148, 3, buf, 10);

    /* Timecode (196..244): No brackets to optimize space */
    total_ms = (unsigned long)g_song.currentRow * (unsigned long)Song_GetMsPerRow();
    total_secs = total_ms / 1000UL;
    minutes = (unsigned int)(total_secs / 60UL);
    seconds = (unsigned int)(total_secs % 60UL);
    hundredths = (unsigned int)((total_ms % 1000UL) / 10UL);
    sprintf(buf, "%02u:%02u:%02u", minutes, seconds, hundredths);
    UI_PrintText(198, 3, buf, 9);

    /* Help Button: Clean '?' (288..298) */
    helpHover = (g_mouse.x >= 286 && g_mouse.x <= 298 && g_mouse.y >= 2 && g_mouse.y <= 12);
    UI_PrintText(290, 3, "?", helpHover ? 7 : 6);

    /* Play/Stop Indicator (306..316) */
    UI_PrintText(308, 3, g_song.playing ? "\x02" : "\x03", g_song.playing ? 5 : 8);

    /* Channel Headers */
    UI_FillRect(4, 18, 312, 12, 1);
    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        uint8_t chColor;
        ChannelMode mode = Song_GetChannelMode((uint8_t)ch);
        if (mode == CH_SOLO) {
            chColor = 9;
        } else if (mode == CH_MUTED) {
            chColor = 8;
        } else {
            chColor = (ch == selectedChannel) ? 3 : 6;
        }

        sprintf(buf, "CH%d", ch + 1);
        UI_PrintText(32 + ch * 31, 21, buf, chColor);
    }

    startRow = cursorRow >= 5 ? cursorRow - 5 : 0;
    if (startRow > PATTERN_ROWS - 10) startRow = PATTERN_ROWS - 10;

    Loop_GetRange(&g_song.loop, &loopStart, &loopEnd, &hasLoop);

    for (r = 0; r < 10; ++r) {
        int rowIdx = startRow + r;
        int y = 33 + r * 12;

        if (rowIdx == g_song.currentRow) {
            UI_FillRect(4, y, 312, 11, 2);
        } else if (hasLoop && rowIdx == loopEnd && loopEnd != loopStart) {
            UI_FillRect(4, y, 312, 11, 10);
        } else if (hasLoop && rowIdx == loopStart) {
            UI_FillRect(4, y, 312, 11, 8);
        }

        sprintf(buf, "%03X", rowIdx);
        UI_PrintText(4, y + 2, buf, (rowIdx == cursorRow) ? 3 : 6);

        for (ch = 0; ch < MAX_CHANNELS; ++ch) {
            TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[rowIdx][ch];
            int nx = 30 + ch * 31;
            isCursor = (ch == selectedChannel && rowIdx == cursorRow);

            if (isCursor) {
                UI_FillRect(nx - 2, y, 29, 11, (n->flags & 0x01) ? 4 : 1);
            } else if (n->flags & 0x01) {
                UI_FillRect(nx - 2, y, 29, 11, 4);
            }

            if (n->attribute == ATTR_MUTE) {
                UI_DrawRect(nx - 2, y, 29, 11, 8);
            } else if (n->attribute == ATTR_DAMPEN) {
                UI_DrawDashedRect(nx - 2, y, 29, 11, 10);
            } else if (n->attribute == ATTR_LETRING) {
                UI_DrawDashedRect(nx - 2, y, 29, 11, 9);
            } else if (n->attribute == ATTR_BEND) {
                UI_DrawDashedRect(nx - 2, y, 29, 11, 11);
            }

            if (isCursor) {
                UI_DrawRect(nx - 3, y - 1, 31, 13, 3);
            }

            if (n->note > 0) {
                static const char *names[12] = {
                    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
                };
                sprintf(buf, "%s%d", names[(n->note - 1) % 12], n->octave);
                UI_PrintText(nx, y + 2, buf, (n->flags & 0x01) ? 0 : 7);
            } else {
                UI_PrintText(nx, y + 2, "...", (isCursor) ? 7 : 2);
            }
        }
    }

    UI_DrawDitherOverlay(4, 153, 312, 4);
    UI_DrawPianoRoll();
}

void UI_DrawAboutDialog(void) {
    int x = 40, y = 35, w = 240, h = 130;
    int px, py, dx, dy;
    uint8_t col;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 3);

    if (g_fairyPcx.pixels) {
        int fw = g_fairyPcx.width;
        int fh = g_fairyPcx.height;
        int fx = x + w - fw - 2;
        int fy = y + h - fh - 2;

        for (py = 0; py < fh; ++py) {
            dy = fy + py;
            if (dy >= y + 1 && dy < y + h - 1 && dy < SCREEN_H) {
                for (px = 0; px < fw; ++px) {
                    dx = fx + px;
                    if (dx >= x + 1 && dx < x + w - 1 && dx < SCREEN_W) {
                        col = g_fairyPcx.pixels[py * fw + px];
                        if (col != 0) {
                            g_vgaBuffer[dy * SCREEN_W + dx] = col;
                        }
                    }
                }
            }
        }
    }

    if (g_splashPcx.pixels) {
        int sw = g_splashPcx.width;
        int sh = g_splashPcx.height;
        int sx = x + (w - sw) / 2 - 6;
        int sy = y + 8;

        for (py = 0; py < sh; ++py) {
            dy = sy + py;
            if (dy < SCREEN_H) {
                for (px = 0; px < sw; ++px) {
                    dx = sx + px;
                    if (dx < SCREEN_W) {
                        col = g_splashPcx.pixels[py * sw + px];
                        if (col != 0) {
                            g_vgaBuffer[dy * SCREEN_W + dx] = col;
                        }
                    }
                }
            }
        }
    }

    UI_PrintText(x + 204, y + 10, "V1.0", 3);

    UI_PrintTextMini(x + 10, y + 36, "AdSynth is a free and open source music sequencer for", 6);
    UI_PrintTextMini(x + 10, y + 46, "OPL2 compatible soundcards. With support for", 6);
    UI_PrintTextMini(x + 10, y + 56, "AdLib Visual Composer Soundbank files.", 6);
    UI_PrintTextMini(x + 10, y + 69, "DISCLAIMER AdSynth's code was produced", 7);
    UI_PrintTextMini(x + 10, y + 79, "with the aid of Google AI Studio.", 7);
    UI_PrintTextMini(x + 10, y + 92, "Spritework, UI & UX by Whiskey_Juliet", 6);
    UI_PrintTextMini(x + 10, y + 102, "github.com/whiskeyjuliet/ - 2026", 6);

    UI_PrintText(x + 44, y + 116, "[ ESC / CLICK TO CLOSE ]", 5);
}

void UI_DrawProjectDialog(int activeTab, const char *filename, int autoSaveEnabled, const char *status) {
    int x = 25, y = 38, w = 270, h = 124;
    int textLen = (int)strlen(filename);
    int cursorX = x + 74 + (textLen * 6);
    int blink;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 3);
    UI_DrawDitherOverlay(x + 2, y + 2, w - 4, 14);

    UI_FillRect(x + 8, y + 3, 90, 13, (activeTab == 0) ? 3 : 2);
    UI_PrintText(x + 14, y + 6, "SAVE PROJECT", (activeTab == 0) ? 0 : 7);

    UI_FillRect(x + 104, y + 3, 90, 13, (activeTab == 1) ? 3 : 2);
    UI_PrintText(x + 110, y + 6, "LOAD PROJECT", (activeTab == 1) ? 0 : 7);

    UI_PrintText(x + 12, y + 26, (activeTab == 0) ? "SAVE ADLIBSYNTH PROJECT (.ADS):" : "LOAD ADLIBSYNTH PROJECT (.ADS):", 4);

    UI_PrintText(x + 12, y + 44, "FILENAME:", 7);
    UI_FillRect(x + 70, y + 41, 186, 13, 0);
    UI_DrawRect(x + 70, y + 41, 186, 13, 3);
    UI_PrintText(x + 74, y + 44, filename, 7);

    blink = ((clock() / (CLOCKS_PER_SEC / 3)) & 1);
    if (blink && textLen < 24) {
        UI_FillRect(cursorX, y + 44, 5, 7, 3);
    }

    if (activeTab == 0) {
        UI_FillRect(x + 12, y + 62, 10, 10, 0);
        UI_DrawRect(x + 12, y + 62, 10, 10, 3);
        if (autoSaveEnabled) {
            UI_PrintText(x + 14, y + 63, "X", 9);
        }
        UI_PrintText(x + 28, y + 64, "AUTO-SAVE EVERY 20 MINUTES", autoSaveEnabled ? 9 : 6);
    } else {
        UI_PrintText(x + 12, y + 64, "AUTOSAVE RECOVERY: AUTOSAVE.ADS", 6);
    }

    if (status && status[0]) {
        UI_PrintText(x + 12, y + 80, status, 5);
    } else {
        UI_PrintText(x + 12, y + 80, "ENTER: EXECUTE  |  ESC: CANCEL", 6);
    }

    UI_FillRect(x + 55, y + 98, 65, 15, 2);
    UI_DrawRect(x + 55, y + 98, 65, 15, 3);
    UI_PrintText(x + 68, y + 102, (activeTab == 0 ? "SAVE" : "LOAD"), 7);

    UI_FillRect(x + 145, y + 98, 65, 15, 2);
    UI_DrawRect(x + 145, y + 98, 65, 15, 2);
    UI_PrintText(x + 158, y + 102, "CANCEL", 7);
}

void UI_DrawImportExportDialog(int activeTab, const char *filename, int isDroFormat, int exportBnk, const char *status) {
    int x = 25, y = 35, w = 270, h = 130;
    int textLen = (int)strlen(filename);
    int cursorX = x + 74 + (textLen * 6);
    int blink;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 4);
    UI_DrawDitherOverlay(x + 2, y + 2, w - 4, 14);

    UI_FillRect(x + 8, y + 3, 70, 13, (activeTab == 0) ? 4 : 2);
    UI_PrintText(x + 22, y + 6, "EXPORT", (activeTab == 0) ? 0 : 7);

    UI_FillRect(x + 84, y + 3, 70, 13, (activeTab == 1) ? 4 : 2);
    UI_PrintText(x + 98, y + 6, "IMPORT", (activeTab == 1) ? 0 : 7);

    UI_PrintText(x + 12, y + 24, (activeTab == 0) ? "EXPORT TO AUDIO STREAM:" : "IMPORT RAW AUDIO FILE:", 4);

    UI_PrintText(x + 12, y + 40, "FORMAT:", 7);
    UI_FillRect(x + 70, y + 38, 45, 12, !isDroFormat ? 3 : 2);
    UI_PrintText(x + 80, y + 40, "IMF", !isDroFormat ? 0 : 7);

    UI_FillRect(x + 125, y + 38, 45, 12, isDroFormat ? 3 : 2);
    UI_PrintText(x + 135, y + 40, "DRO", isDroFormat ? 0 : 7);

    UI_PrintText(x + 12, y + 57, "FILENAME:", 7);
    UI_FillRect(x + 70, y + 54, 186, 13, 0);
    UI_DrawRect(x + 70, y + 54, 186, 13, 3);
    UI_PrintText(x + 74, y + 57, filename, 7);

    blink = ((clock() / (CLOCKS_PER_SEC / 3)) & 1);
    if (blink && textLen < 24) {
        UI_FillRect(cursorX, y + 57, 5, 7, 3);
    }

    if (activeTab == 0) {
        UI_FillRect(x + 12, y + 74, 10, 10, 0);
        UI_DrawRect(x + 12, y + 74, 10, 10, 4);
        if (exportBnk) {
            UI_PrintText(x + 14, y + 75, "X", 4);
        }
        UI_PrintText(x + 28, y + 76, "EXPORT SUPPORTING .BNK SOUNDBANK", exportBnk ? 7 : 6);
    } else {
        UI_PrintText(x + 12, y + 76, "EXTRACTS NOTES & SOUNDBANK PATCHES", 6);
    }

    if (status && status[0]) {
        UI_PrintText(x + 12, y + 91, status, 5);
    } else {
        UI_PrintText(x + 12, y + 91, "PRESS ENTER: OK  |  ESC: CANCEL", 6);
    }

    UI_FillRect(x + 55, y + 107, 65, 15, 2);
    UI_DrawRect(x + 55, y + 107, 65, 15, 3);
    UI_PrintText(x + 62, y + 111, (activeTab == 0 ? "EXPORT" : "IMPORT"), 7);

    UI_FillRect(x + 145, y + 107, 65, 15, 2);
    UI_DrawRect(x + 145, y + 107, 65, 15, 2);
    UI_PrintText(x + 158, y + 111, "CANCEL", 7);
}

void UI_DrawBpmPopup(int mouseX, int mouseY, uint16_t bpm, uint16_t clockHz) {
    char buf[48];
    int x = mouseX + 4;
    int y = mouseY + 4;
    int w = 180, h = 34;

    if (x + w > SCREEN_W) x = SCREEN_W - w - 4;
    if (y + h > SCREEN_H) y = SCREEN_H - h - 4;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 10);

    sprintf(buf, "TEMPO: %d BPM", bpm);
    UI_PrintText(x + 6, y + 5, buf, 10);

    sprintf(buf, "CLOCK: %s", Song_GetClockDescription(clockHz));
    UI_PrintTextMini(x + 6, y + 19, buf, 7);
}

void UI_DrawBpmDialog(const char *bpmBuffer, int activeSlot, uint16_t selectedClockHz) {
    int x = 40, y = 30, w = 240, h = 145;
    int textLen = (int)strlen(bpmBuffer);
    int cursorX = x + 62 + (textLen * 6);
    int blink;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 10);
    UI_DrawDitherOverlay(x + 2, y + 2, w - 4, 14);

    UI_PrintText(x + 10, y + 6, "BPM & ENGINE CLOCK RESOLUTION", 10);

    UI_PrintText(x + 12, y + 24, "BPM (30-300):", 7);
    UI_FillRect(x + 60, y + 22, 60, 13, 0);
    UI_DrawRect(x + 60, y + 22, 60, 13, 10);
    UI_PrintText(x + 64, y + 25, bpmBuffer, 10);

    blink = ((clock() / (CLOCKS_PER_SEC / 3)) & 1);
    if (blink && textLen < 3) {
        UI_FillRect(cursorX, y + 25, 5, 7, 10);
    }

    UI_PrintText(x + 12, y + 42, "CLOCK ENGINE PRESETS:", 4);

    /* 70 Hz Preset */
    UI_FillRect(x + 12, y + 54, 216, 12, (selectedClockHz == 70) ? 10 : 2);
    UI_PrintTextMini(x + 16, y + 57, "70 HZ   : WOLFENSTEIN 3D / VGA RETRACE", (selectedClockHz == 70) ? 0 : 7);

    /* 560 Hz Preset */
    UI_FillRect(x + 12, y + 68, 216, 12, (selectedClockHz == 560) ? 10 : 2);
    UI_PrintTextMini(x + 16, y + 71, "560 HZ  : COMMANDER KEEN / DUKE 2 / BIO", (selectedClockHz == 560) ? 0 : 7);

    /* 700 Hz Preset */
    UI_FillRect(x + 12, y + 82, 216, 12, (selectedClockHz == 700) ? 10 : 2);
    UI_PrintTextMini(x + 16, y + 85, "700 HZ  : BLAKE STONE / MAJOR STRYKER", (selectedClockHz == 700) ? 0 : 7);

    /* 1000 Hz Preset */
    UI_FillRect(x + 12, y + 96, 216, 12, (selectedClockHz == 1000) ? 10 : 2);
    UI_PrintTextMini(x + 16, y + 99, "1000 HZ : DOSBOX RAW OPL (.DRO 1MS TICK)", (selectedClockHz == 1000) ? 0 : 7);

    UI_FillRect(x + 40, y + 118, 65, 16, 2);
    UI_DrawRect(x + 40, y + 118, 65, 16, 10);
    UI_PrintText(x + 60, y + 122, "OK", 7);

    UI_FillRect(x + 135, y + 118, 65, 16, 2);
    UI_DrawRect(x + 135, y + 118, 65, 16, 2);
    UI_PrintText(x + 147, y + 122, "CANCEL", 7);
}

void UI_DrawConfirmDialog(const char *title, const char *prompt) {
    int x = 60, y = 60, w = 200, h = 80;
    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 8);
    UI_PrintText(x + 10, y + 8, title, 8);
    UI_PrintText(x + 10, y + 28, prompt, 7);

    UI_FillRect(x + 30, y + 52, 50, 16, 2);
    UI_DrawRect(x + 30, y + 52, 50, 16, 3);
    UI_PrintText(x + 45, y + 56, "YES", 7);

    UI_FillRect(x + 120, y + 52, 50, 16, 2);
    UI_DrawRect(x + 120, y + 52, 50, 16, 2);
    UI_PrintText(x + 137, y + 56, "NO", 7);
}

void UI_DrawInstPopup(int mouseX, int mouseY, int presetIdx) {
    char buf[32];
    int x = mouseX + 4;
    int y = mouseY + 4;
    int w = 120, h = 32;
    const OPL_Instrument *inst = Bank_GetInstrument(presetIdx);

    if (x + w > SCREEN_W) x = SCREEN_W - w - 4;
    if (y + h > SCREEN_H) y = SCREEN_H - h - 4;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 4);

    sprintf(buf, "INST %02d:", presetIdx + 1);
    UI_PrintText(x + 6, y + 6, buf, 4);
    UI_PrintText(x + 6, y + 18, inst->name, 7);
}

void UI_DrawTimePopup(int mouseX, int mouseY, unsigned long totalMS) {
    char buf[32];
    int x = mouseX + 4;
    int y = mouseY + 4;
    int w = 126, h = 32;
    unsigned long secs = totalMS / 1000UL;
    unsigned int mins = (unsigned int)(secs / 60UL);
    unsigned int remSecs = (unsigned int)(secs % 60UL);
    unsigned int hundredths = (unsigned int)((totalMS % 1000UL) / 10UL);

    if (x + w > SCREEN_W) x = SCREEN_W - w - 4;
    if (y + h > SCREEN_H) y = SCREEN_H - h - 4;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 9);

    UI_PrintText(x + 6, y + 6, "TOTAL LENGTH:", 6);
    sprintf(buf, "%02u:%02u:%02u", mins, remSecs, hundredths);
    UI_PrintText(x + 6, y + 18, buf, 9);
}

void UI_DrawJumpDialog(const char *timeStr, int activeSlot, const char *status) {
    int x = 60, y = 60, w = 200, h = 95;
    static const int charIndices[6] = {0, 1, 3, 4, 6, 7};
    int cx;
    int blink;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 9);

    UI_PrintText(x + 10, y + 8, "JUMP TO TIME CODE", 9);
    UI_PrintText(x + 10, y + 26, "TIME (MM:SS:HH):", 7);

    UI_FillRect(x + 10, y + 40, 180, 14, 0);
    UI_DrawRect(x + 10, y + 40, 180, 14, 9);
    UI_PrintText(x + 14, y + 43, timeStr, 9);

    blink = ((clock() / (CLOCKS_PER_SEC / 3)) & 1);
    if (blink && activeSlot >= 0 && activeSlot < 6) {
        cx = x + 14 + (charIndices[activeSlot] * 6);
        UI_FillRect(cx, y + 51, 5, 2, 9);
    }

    if (status && status[0]) {
        UI_PrintText(x + 10, y + 58, status, 8);
    }

    UI_FillRect(x + 30, y + 73, 55, 15, 2);
    UI_DrawRect(x + 30, y + 73, 55, 15, 9);
    UI_PrintText(x + 42, y + 76, "JUMP", 9);

    UI_FillRect(x + 115, y + 73, 55, 15, 2);
    UI_DrawRect(x + 115, y + 73, 55, 15, 2);
    UI_PrintText(x + 122, y + 76, "CANCEL", 7);
}

void UI_DrawHelpDialog(int page) {
    int x = 16, y = 12, w = 288, h = 176;
    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 3);
    UI_DrawDitherOverlay(x + 2, y + 2, w - 4, 12);

    if (page == 0) {
        UI_PrintText(x + 94, y + 4, "HELP & SHORTCUTS", 3);

        UI_PrintText(x + 10, y + 20, "KEYBOARD PIANO INPUT:", 4);
        UI_PrintText(x + 10, y + 32, "LOW OCTAVE : Z S X D C V G B H N J M", 7);
        UI_PrintText(x + 10, y + 44, "HIGH OCTAVE: Q 2 W 3 E R 5 T 6 Y 7 U", 7);

        UI_PrintText(x + 10, y + 62, "TRACKER NAVIGATION & EDITS:", 4);
        UI_PrintText(x + 10, y + 74, "ARROWS     : MOVE CELL EDIT CURSOR", 7);
        UI_PrintText(x + 10, y + 86, "DEL        : DELETE NOTE / SELECTION", 7);
        UI_PrintText(x + 10, y + 98, "SPACE      : PLAY / STOP (RESETS TO 000)", 7);
        UI_PrintText(x + 10, y + 110, "CTRL+SPACE : PAUSE / RESUME PLAYHEAD", 7);
        UI_PrintText(x + 10, y + 122, "+ / -      : OCTAVE UP / DOWN", 7);
        UI_PrintText(x + 10, y + 134, "TAB        : CYCLE PRESET INSTRUMENTS", 7);

        UI_PrintText(x + 10, y + 162, "[CLICK OR PRESS KEY FOR MORE]", 5);
        UI_PrintText(x + 214, y + 162, "PAGE 1 OF 4", 3);
    } else if (page == 1) {
        UI_PrintText(x + 94, y + 4, "HELP & SHORTCUTS", 3);

        UI_PrintText(x + 10, y + 20, "FILE & GLOBAL COMMANDS:", 4);
        UI_PrintText(x + 10, y + 32, "F1         : NEW TRACK (CLEAR CONFIRM)", 7);
        UI_PrintText(x + 10, y + 44, "F2         : SAVE / LOAD PROJECT (.ADS)", 7);
        UI_PrintText(x + 10, y + 56, "F3         : IMPORT / EXPORT (IMF / DRO)", 7);
        UI_PrintText(x + 10, y + 68, "F4         : OPEN HELP GUIDE", 7);
        UI_PrintText(x + 10, y + 80, "ESC        : QUIT CONFIRMATION MODAL", 8);

        UI_PrintText(x + 10, y + 98, "CLIPBOARD & HISTORY:", 4);
        UI_PrintText(x + 10, y + 110, "CTRL+X     : CUT SELECTED CELLS", 7);
        UI_PrintText(x + 10, y + 122, "CTRL+C     : COPY SELECTION / CELL", 7);
        UI_PrintText(x + 10, y + 134, "CTRL+V     : PASTE CLIPBOARD", 7);
        UI_PrintText(x + 10, y + 146, "CTRL+Z     : UNDO (50-STEP HISTORY)", 7);

        UI_PrintText(x + 10, y + 162, "[CLICK OR PRESS KEY FOR MORE]", 5);
        UI_PrintText(x + 214, y + 162, "PAGE 2 OF 4", 3);
    } else if (page == 2) {
        UI_PrintText(x + 94, y + 4, "HELP & SHORTCUTS", 3);

        UI_PrintText(x + 10, y + 20, "TOP BAR MOUSE CONTROLS:", 4);
        UI_PrintText(x + 10, y + 32, "LOGO       : OPEN ABOUT DIALOGUE", 7);
        UI_PrintText(x + 10, y + 44, "OCT / INST / BPM NUMBERS:", 7);
        UI_PrintText(x + 10, y + 56, "  L-CLICK  : ASCEND (+1, +SHIFT = +10)", 6);
        UI_PrintText(x + 10, y + 68, "  R-CLICK  : DESCEND (-1, +SHIFT = -10)", 6);
        UI_PrintText(x + 10, y + 80, "INST LABEL : R-CLICK INFO, DBL-CLICK BROWSER", 7);
        UI_PrintText(x + 10, y + 92, "BPM LABEL  : R-CLICK INFO, DBL-CLICK EDITOR", 7);
        UI_PrintText(x + 10, y + 104, "TIMECODE   : R-CLICK LENGTH, DBL-CLICK JUMP", 7);

        UI_PrintText(x + 10, y + 120, "GRID & CHANNEL CONTROLS:", 4);
        UI_PrintText(x + 10, y + 132, "L-CLICK CH : SELECT CHANNEL COLUMN", 7);
        UI_PrintText(x + 10, y + 144, "R-CLICK CH : TOGGLE MUTE / SOLO", 7);

        UI_PrintText(x + 10, y + 162, "[CLICK OR PRESS KEY FOR MORE]", 5);
        UI_PrintText(x + 214, y + 162, "PAGE 3 OF 4", 3);
    } else {
        UI_PrintText(x + 94, y + 4, "HELP & SHORTCUTS", 3);

        UI_PrintText(x + 10, y + 20, "NOTE ATTRIBUTES:", 4);
        UI_PrintText(x + 10, y + 32, "[ KEY      : LET RING (GREEN DASHED)", 9);
        UI_PrintText(x + 10, y + 44, "] KEY      : DAMPEN (ORANGE DASHED)", 10);
        UI_PrintText(x + 10, y + 56, "\\ KEY      : MUTE (SOLID RED OUTLINE)", 8);
        UI_PrintText(x + 10, y + 68, "# KEY      : PITCH BEND (YELLOW DASHED)", 11);

        UI_PrintText(x + 10, y + 84, "GRID ACTIONS:", 4);
        UI_PrintText(x + 10, y + 96, "R-CLICK CELL: CYCLE ATTRIBUTES", 7);
        UI_PrintText(x + 10, y + 108, "PGUP / PGDN : TRANSPOSE +/- SEMITONE", 7);
        UI_PrintText(x + 10, y + 120, "R-CLICK ROW : TOGGLE LOOP POINTS", 7);
        UI_PrintText(x + 10, y + 132, "SHIFT+DRAG  : PAINT MULTI-CELL SELECTION", 7);

        UI_PrintText(x + 10, y + 162, "[CLICK OR PRESS KEY TO CLOSE]", 5);
        UI_PrintText(x + 214, y + 162, "PAGE 4 OF 4", 3);
    }
}

void UI_DrawInstWindow(int activeTab, int selectedInst, int pageOrScroll, const char *bnkInputFilename, const char *status) {
    int x = 12, y = 10, w = 296, h = 180;
    int bnkLoaded = (Bank_GetBnkCount() > 0);
    char buf[64];
    int i;
    int closeHover;
    int closePressed;

    UI_FillRect(x, y, w, h, 1);
    UI_DrawRect(x, y, w, h, 3);
    UI_DrawDitherOverlay(x + 2, y + 2, w - 4, 14);

    UI_FillRect(x + 6, y + 3, 70, 13, (activeTab == 0) ? 3 : 2);
    UI_PrintText(x + 14, y + 6, "PRESETS", (activeTab == 0) ? 0 : 7);

    UI_FillRect(x + 80, y + 3, 70, 13, (activeTab == 1) ? 3 : 2);
    UI_PrintText(x + 86, y + 6, "LOAD BNK", (activeTab == 1) ? 0 : 7);

    if (bnkLoaded) {
        UI_FillRect(x + 154, y + 3, 80, 13, (activeTab == 2) ? 3 : 2);
        UI_PrintText(x + 160, y + 6, "BANK VOICES", (activeTab == 2) ? 0 : 7);
    }

    closeHover = (g_mouse.x >= 288 && g_mouse.x <= 303 && g_mouse.y >= 12 && g_mouse.y <= 24);
    closePressed = closeHover && g_mouse.leftDown;

    if (closePressed) {
        UI_FillRect(289, 13, 14, 12, 0);
        UI_DrawRect(289, 13, 14, 12, 2);
        UI_PrintText(292, 16, "\x04", 7);
    } else {
        UI_FillRect(288, 12, 14, 12, 2);
        UI_DrawRect(288, 12, 14, 12, closeHover ? 7 : 3);
        UI_PrintText(291, 15, "\x04", closeHover ? 7 : 3);
    }

    if (activeTab == 0) {
        int page = pageOrScroll % 4;
        int startIdx = page * 16;

        sprintf(buf, "FACTORY VOICES (PAGE %d OF 4)", page + 1);
        UI_PrintText(x + 10, y + 22, buf, 4);

        for (i = 0; i < 16; ++i) {
            int instIdx = startIdx + i;
            if (instIdx < NUM_PRESET_INSTRUMENTS) {
                const OPL_Instrument *inst = Bank_GetInstrument(instIdx);
                int col = i / 8;
                int row = i % 8;
                int bx = x + 10 + col * 140;
                int by = y + 36 + row * 14;
                int isSel = (instIdx == selectedInst);

                UI_FillRect(bx, by, 134, 12, isSel ? 4 : 2);
                if (isSel) UI_DrawRect(bx, by, 134, 12, 7);

                sprintf(buf, "%02d:%-12s", instIdx + 1, inst->name);
                UI_PrintText(bx + 4, by + 3, buf, isSel ? 0 : 7);
            }
        }

        UI_FillRect(x + 10, y + 156, 60, 14, 2);
        UI_DrawRect(x + 10, y + 156, 60, 14, 3);
        UI_PrintText(x + 14, y + 160, "\x05 PREV", 7);

        UI_FillRect(x + 80, y + 156, 60, 14, 2);
        UI_DrawRect(x + 80, y + 156, 60, 14, 3);
        UI_PrintText(x + 87, y + 160, "NEXT \x06", 7);

        UI_PrintText(x + 152, y + 160, "[CLICK ITEM TO SELECT]", 5);
    } else if (activeTab == 1) {
        int totalVoices = Bank_GetBnkCount();
        int scroll = pageOrScroll;
        int sx, sy, sw, sh;
        int thumbY, thumbHeight;

        UI_PrintText(x + 10, y + 22, "LOAD ADLIB VISUAL COMPOSER BANK (.BNK):", 4);
        UI_PrintText(x + 10, y + 40, "FILENAME:", 7);
        UI_FillRect(x + 70, y + 37, 160, 13, 0);
        UI_DrawRect(x + 70, y + 37, 160, 13, 3);
        UI_PrintText(x + 74, y + 40, bnkInputFilename, 7);

        UI_FillRect(x + 236, y + 37, 45, 13, 3);
        UI_PrintText(x + 244, y + 40, "LOAD", 0);

        if (status && status[0]) {
            UI_PrintText(x + 10, y + 56, status, 5);
        }

        sprintf(buf, "LOADED BANK: %s (%d VOICES)", Bank_GetLoadedBnkFilename(), totalVoices);
        UI_PrintText(x + 10, y + 70, buf, 6);

        UI_FillRect(x + 10, y + 84, 260, 84, 0);
        UI_DrawRect(x + 10, y + 84, 260, 84, 2);

        sx = x + 272;
        sy = y + 84;
        sw = 12;
        sh = 84;
        UI_FillRect(sx, sy, sw, sh, 0);
        UI_DrawRect(sx, sy, sw, sh, 2);

        if (totalVoices > 0) {
            if (scroll < 0) scroll = 0;
            if (scroll > totalVoices - 10) scroll = totalVoices - 10;
            if (scroll < 0) scroll = 0;

            for (i = 0; i < 10; ++i) {
                int bnkVoiceIdx = scroll + i;
                if (bnkVoiceIdx < totalVoices) {
                    int globalIdx = NUM_PRESET_INSTRUMENTS + bnkVoiceIdx;
                    const OPL_Instrument *inst = Bank_GetInstrument(globalIdx);
                    sprintf(buf, "%03d: %-12s >> INST #%03d", bnkVoiceIdx + 1, inst->name, globalIdx + 1);
                    UI_PrintTextMini(x + 14, y + 87 + i * 8, buf, (globalIdx == selectedInst) ? 4 : 7);
                }
            }

            if (totalVoices <= 10) {
                thumbHeight = sh - 2;
                thumbY = sy + 1;
            } else {
                thumbHeight = (10 * (sh - 2)) / totalVoices;
                if (thumbHeight < 8) thumbHeight = 8;
                thumbY = sy + 1 + (scroll * (sh - 2 - thumbHeight)) / (totalVoices - 10);
            }
            UI_FillRect(sx + 2, thumbY, sw - 4, thumbHeight, 2);
            UI_DrawRect(sx + 2, thumbY, sw - 4, thumbHeight, 3);
        } else {
            UI_PrintTextMini(x + 16, y + 120, "NO .BNK FILE LOADED. ENTER FILENAME ABOVE.", 6);
        }
    } else if (activeTab == 2 && bnkLoaded) {
        int page = pageOrScroll;
        int totalPages = (Bank_GetBnkCount() + 15) / 16;
        int startIdx;
        if (page < 0) page = 0;
        if (page >= totalPages) page = totalPages - 1;
        startIdx = page * 16;

        sprintf(buf, "CUSTOM BANK VOICES (PAGE %d OF %d)", page + 1, totalPages);
        UI_PrintText(x + 10, y + 22, buf, 4);

        for (i = 0; i < 16; ++i) {
            int bnkIdx = startIdx + i;
            if (bnkIdx < Bank_GetBnkCount()) {
                int globalIdx = NUM_PRESET_INSTRUMENTS + bnkIdx;
                const OPL_Instrument *inst = Bank_GetInstrument(globalIdx);
                int col = i / 8;
                int row = i % 8;
                int bx = x + 10 + col * 140;
                int by = y + 36 + row * 14;
                int isSel = (globalIdx == selectedInst);

                UI_FillRect(bx, by, 134, 12, isSel ? 4 : 2);
                if (isSel) UI_DrawRect(bx, by, 134, 12, 7);

                sprintf(buf, "%03d:%-12s", globalIdx + 1, inst->name);
                UI_PrintText(bx + 4, by + 3, buf, isSel ? 0 : 7);
            }
        }

        UI_FillRect(x + 10, y + 156, 60, 14, 2);
        UI_DrawRect(x + 10, y + 156, 60, 14, 3);
        UI_PrintText(x + 14, y + 160, "\x05 PREV", 7);

        UI_FillRect(x + 80, y + 156, 60, 14, 2);
        UI_DrawRect(x + 80, y + 156, 60, 14, 3);
        UI_PrintText(x + 87, y + 160, "NEXT \x06", 7);

        UI_PrintText(x + 152, y + 160, "[CLICK ITEM TO SELECT]", 5);
    }
}

void UI_DrawMouse(void) {
    int mx = g_mouse.x;
    int my = g_mouse.y;
    int i;

    if (g_cursorPcx.pixels && g_cursorPcx.width == 7 && g_cursorPcx.height == 7) {
        int px, py;
        for (py = 0; py < 7; ++py) {
            int dy = my + py;
            if (dy >= 0 && dy < SCREEN_H) {
                for (px = 0; px < 7; ++px) {
                    int dx = mx + px;
                    if (dx >= 0 && dx < SCREEN_W) {
                        uint8_t c = g_cursorPcx.pixels[py * 7 + px];
                        if (c != 0) g_vgaBuffer[dy * SCREEN_W + dx] = c;
                    }
                }
            }
        }
        return;
    }

    for (i = 0; i < 7; ++i) {
        if (mx < SCREEN_W && my + i < SCREEN_H) {
            g_vgaBuffer[(my + i) * SCREEN_W + mx] = 7;
        }
        if (mx + i < SCREEN_W && my + i < SCREEN_H) {
            g_vgaBuffer[(my + i) * SCREEN_W + mx + i] = 7;
        }
    }

    if (mx + 2 < SCREEN_W && my + 2 < SCREEN_H) {
        g_vgaBuffer[(my + 2) * SCREEN_W + (mx + 2)] = 7;
    }
    UI_FillRect(mx + 1, my + 2, 1, 4, 3);
    UI_FillRect(mx + 2, my + 3, 1, 3, 3);
}

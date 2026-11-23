#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <bios.h>
#include <time.h>
#include <stdlib.h>
#include "opl.h"
#include "song.h"
#include "ui.h"
#include "loop.h"
#include "bank.h"

static int g_selectedChannel = 0;
static uint16_t g_cursorRow = 0;
static int g_selectedOctave = 4;
static int g_selectedPreset = 0;

static DialogMode g_dialog = DIALOG_NONE;
static int g_helpPage = 0;

/* F2: Project Save / Load Variables */
static int  g_projectTab = 0;
static char g_projectFilename[32] = "TRACK1.ADS";
static int  g_autoSaveEnabled = 1;
static char g_projectStatusMsg[48] = "";
static clock_t g_lastAutoSaveTick = 0;

/* F3: Import / Export Variables (Tab 0 = IMPORT, Tab 1 = EXPORT) */
static int  g_importExportTab = 0;
static int  g_isDroFormat = 0;
static int  g_exportSupportingBnk = 1;
static uint16_t g_importImfHz = 0;
static uint16_t g_exportImfHz = 700;
static char g_audioFilename[32] = "TRACK1.IMF";
static char g_audioStatusMsg[48] = "";

static char g_timeInputBuffer[16] = "00:00:00";
static int g_timeInputSlot = 0;

/* Tempo & Track Clock Editor */
static char g_tempoInputBuffer[8] = "120";
static uint16_t g_selectedClockPreset = 700;
static uint16_t g_selectedRowMsPreset = 30;

static int g_instWindowTab = 0;
static int g_instWindowPage = 0;
static char g_bnkFilenameBuffer[32] = "STANDARD.BNK";
static char g_bnkStatusMsg[48] = "";

static unsigned long g_lastTimeClickTime = 0;
static int g_lastTimeClickX = 0;
static int g_lastTimeClickY = 0;

static unsigned long g_lastTempoClickTime = 0;
static int g_lastTempoClickX = 0;
static int g_lastTempoClickY = 0;

static unsigned long g_lastClkClickTime = 0;
static int g_lastClkClickX = 0;
static int g_lastClkClickY = 0;

static unsigned long g_lastInstClickTime = 0;
static int g_lastInstClickX = 0;
static int g_lastInstClickY = 0;

static int g_previewTimer = 0;
static int g_scrollbarDragging = 0;
static int g_isShiftDragging = 0;

/* High-Precision 8253/8254 PIT Channel 0 Mode 2 Timer */
static uint32_t g_lastRawPit = 0;
static uint32_t g_lastPitCount = 0;
static double g_subStepAccumMs = 0.0;
static int g_subStepIndex = 0;

static void Timer_Init(void) {
    _disable();
    outp(0x43, 0x34);
    outp(0x40, 0x00);
    outp(0x40, 0x00);
    _enable();
}

static void Timer_Shutdown(void) {
    _disable();
    outp(0x43, 0x36);
    outp(0x40, 0x00);
    outp(0x40, 0x00);
    _enable();
}

static uint32_t Timer_GetPitTicks(void) {
    uint32_t b;
    uint8_t lo, hi;
    uint16_t cnt;
    uint32_t raw;

    _disable();
    outp(0x43, 0x00);
    lo = (uint8_t)inp(0x40);
    hi = (uint8_t)inp(0x40);
    b = *(volatile uint32_t *)0x046C;
    _enable();

    cnt = (uint16_t)(lo | ((uint16_t)hi << 8));
    raw = (b << 16) | (uint32_t)(0xFFFFU - cnt);

    if (raw < g_lastRawPit && (g_lastRawPit - raw) < 0x20000UL) {
        raw += 0x10000UL;
    }
    g_lastRawPit = raw;
    return raw;
}

static void Audio_Poll(void) {
    static int inPoll = 0;
    uint32_t nowPit;
    int32_t deltaPit;
    double deltaMs, subStepMs;
    int steps = 0;

    if (inPoll) return;
    inPoll = 1;

    nowPit = Timer_GetPitTicks();
    deltaPit = (int32_t)(nowPit - g_lastPitCount);
    g_lastPitCount = nowPit;

    if (deltaPit < 0) deltaPit = 0;

    if (g_song.playing) {
        deltaMs = (double)deltaPit / 1193.181667;
        if (deltaMs > 100.0) deltaMs = 100.0;
        g_subStepAccumMs += deltaMs;

        subStepMs = Song_GetMsPerRowFloat() / 6.0;
        if (subStepMs < 0.5) subStepMs = 0.5;

        while (g_song.playing && g_subStepAccumMs >= subStepMs && steps < 16) {
            g_subStepAccumMs -= subStepMs;
            Song_SubTick(g_subStepIndex, 6);
            g_subStepIndex++;
            if (g_subStepIndex >= 6) {
                g_subStepIndex = 0;
                Song_Tick();
                g_cursorRow = g_song.currentRow;
            }
            steps++;
        }
    } else {
        g_subStepAccumMs = 0.0;
    }

    inPoll = 0;
}

static int KeyToNote(char key, int *octaveOffset) {
    *octaveOffset = 0;
    switch (key) {
        case 'z': case 'Z': return 1;
        case 's': case 'S': return 2;
        case 'x': case 'X': return 3;
        case 'd': case 'D': return 4;
        case 'c': case 'C': return 5;
        case 'v': case 'V': return 6;
        case 'g': case 'G': return 7;
        case 'b': case 'B': return 8;
        case 'h': case 'H': return 9;
        case 'n': case 'N': return 10;
        case 'j': case 'J': return 11;
        case 'm': case 'M': return 12;
        case 'q': case 'Q': *octaveOffset = 1; return 1;
        case '2':           *octaveOffset = 1; return 2;
        case 'w': case 'W': *octaveOffset = 1; return 3;
        case '3':           *octaveOffset = 1; return 4;
        case 'e': case 'E': *octaveOffset = 1; return 5;
        case 'r': case 'R': *octaveOffset = 1; return 6;
        case '5':           *octaveOffset = 1; return 7;
        case 't': case 'T': *octaveOffset = 1; return 8;
        case '6':           *octaveOffset = 1; return 9;
        case 'y': case 'Y': *octaveOffset = 1; return 10;
        case '7':           *octaveOffset = 1; return 11;
        case 'u': case 'U': *octaveOffset = 1; return 12;
        case 'i': case 'I': *octaveOffset = 2; return 1;
        default: return 0;
    }
}

static void PreviewInstrument(int instIdx) {
    const OPL_Instrument *inst = Bank_GetInstrument(instIdx);
    OPL_NoteOff(8);
    OPL_SetInstrument(8, inst);
    OPL_NoteOn(8, 0, 4);
    g_previewTimer = 25;
}

static void ParseTimeCodeAndJump(const char *timeStr) {
    unsigned int m, s, h;
    unsigned long total_ms;
    double msPerRow;
    uint32_t targetRow;

    m = 0; s = 0; h = 0;
    if (sscanf(timeStr, "%u:%u:%u", &m, &s, &h) >= 2) {
        total_ms = (m * 60UL * 1000UL) + (s * 1000UL) + (h * 10UL);
        msPerRow = Song_GetMsPerRowFloat();
        targetRow = (uint32_t)((double)total_ms / msPerRow);
        if (targetRow >= PATTERN_ROWS) targetRow = PATTERN_ROWS - 1;
        g_cursorRow = (uint16_t)targetRow;
        g_song.currentRow = (uint16_t)targetRow;
    }
}

static void CycleImportImfHz(void) {
    if (g_importImfHz == 0) g_importImfHz = 280;
    else if (g_importImfHz == 280) g_importImfHz = 560;
    else if (g_importImfHz == 560) g_importImfHz = 700;
    else g_importImfHz = 0;
}

static void CycleExportImfHz(void) {
    if (g_exportImfHz == 280) g_exportImfHz = 560;
    else if (g_exportImfHz == 560) g_exportImfHz = 700;
    else g_exportImfHz = 280;
}

static void ExecuteImportExport(void) {
    if (g_importExportTab == 0) {
        int ok = g_isDroFormat ? Song_LoadDRO(g_audioFilename) : Song_LoadIMF_Ex(g_audioFilename, g_importImfHz);
        if (ok) {
            g_cursorRow = 0;
            g_song.currentRow = 0;
            sprintf(g_audioStatusMsg, "OK: %d NOTES, %d VOICES", Song_GetLastImportNoteCount(), Song_GetLastImportVoiceCount());
            g_dialog = DIALOG_NONE;
        } else {
            strcpy(g_audioStatusMsg, "IMPORT ERROR!");
        }
    } else {
        int ok = g_isDroFormat ? Song_ExportDRO(g_audioFilename) : Song_ExportIMF_Ex(g_audioFilename, g_exportImfHz);
        if (ok && g_exportSupportingBnk) {
            Song_ExportSupportingBNK(g_audioFilename);
        }
        strcpy(g_audioStatusMsg, ok ? "EXPORT COMPLETE!" : "EXPORT FAILED!");
        if (ok) g_dialog = DIALOG_NONE;
    }
}

static void HandleDialogMouse(int *outRunning) {
    int i;
    int totalVoices;

    if (g_dialog == DIALOG_HELP) {
        if (g_helpPage < 3) {
            g_helpPage++;
        } else {
            g_dialog = DIALOG_NONE;
            g_helpPage = 0;
        }
        return;
    }

    if (g_dialog == DIALOG_ABOUT || g_dialog == DIALOG_INST_INFO || g_dialog == DIALOG_TIME_INFO || g_dialog == DIALOG_TEMPO_INFO) {
        g_dialog = DIALOG_NONE;
        return;
    }

    if (g_dialog == DIALOG_CLEAR_CONFIRM) {
        if (g_mouse.x >= 90 && g_mouse.x <= 140 && g_mouse.y >= 112 && g_mouse.y <= 128) {
            Song_ClearAll();
            g_cursorRow = 0;
            g_song.currentRow = 0;
        }
        g_dialog = DIALOG_NONE;
        return;
    }

    if (g_dialog == DIALOG_EXIT_CONFIRM) {
        if (g_mouse.x >= 90 && g_mouse.x <= 140 && g_mouse.y >= 112 && g_mouse.y <= 128) {
            *outRunning = 0;
        }
        g_dialog = DIALOG_NONE;
        return;
    }

    if (g_dialog == DIALOG_JUMP_TIME) {
        if (g_mouse.x >= 90 && g_mouse.x <= 145 && g_mouse.y >= 133 && g_mouse.y <= 151) {
            ParseTimeCodeAndJump(g_timeInputBuffer);
        }
        g_dialog = DIALOG_NONE;
        return;
    }

    if (g_dialog == DIALOG_TEMPO_WINDOW) {
        if (g_mouse.x >= 85 && g_mouse.x <= 145 && g_mouse.y >= 115 && g_mouse.y <= 131) {
            int newTempo = atoi(g_tempoInputBuffer);
            if (newTempo >= 30 && newTempo <= 300) g_song.tempo = (uint16_t)newTempo;
            g_dialog = DIALOG_NONE;
            return;
        }
        if (g_mouse.x >= 175 && g_mouse.x <= 235 && g_mouse.y >= 115 && g_mouse.y <= 131) {
            g_dialog = DIALOG_NONE;
            return;
        }
        return;
    }

    if (g_dialog == DIALOG_TRACK_WINDOW) {
        if (g_mouse.x >= 42 && g_mouse.x <= 278) {
            if (g_mouse.y >= 61 && g_mouse.y <= 72)  g_selectedClockPreset = 280;
            if (g_mouse.y >= 74 && g_mouse.y <= 85)  g_selectedClockPreset = 560;
            if (g_mouse.y >= 87 && g_mouse.y <= 98)  g_selectedClockPreset = 700;
            if (g_mouse.y >= 100 && g_mouse.y <= 111) g_selectedClockPreset = 1000;
        }
        if (g_mouse.y >= 127 && g_mouse.y <= 141) {
            if (g_mouse.x >= 42 && g_mouse.x <= 86)   g_selectedRowMsPreset = 10;
            if (g_mouse.x >= 90 && g_mouse.x <= 134)  g_selectedRowMsPreset = 20;
            if (g_mouse.x >= 138 && g_mouse.x <= 182) g_selectedRowMsPreset = 30;
            if (g_mouse.x >= 186 && g_mouse.x <= 230) g_selectedRowMsPreset = 60;
            if (g_mouse.x >= 234 && g_mouse.x <= 278) g_selectedRowMsPreset = 120;
        }
        if (g_mouse.x >= 75 && g_mouse.x <= 140 && g_mouse.y >= 154 && g_mouse.y <= 170) {
            Song_SetClockHz(g_selectedClockPreset);
            if (g_selectedRowMsPreset != g_song.rowMs) {
                Song_RescaleRowMs(g_selectedRowMsPreset);
                g_cursorRow = g_song.currentRow;
            }
            g_dialog = DIALOG_NONE;
            return;
        }
        if (g_mouse.x >= 180 && g_mouse.x <= 245 && g_mouse.y >= 154 && g_mouse.y <= 170) {
            g_dialog = DIALOG_NONE;
            return;
        }
        return;
    }

    if (g_dialog == DIALOG_PROJECT_WINDOW) {
        if (g_mouse.y >= 41 && g_mouse.y <= 54) {
            if (g_mouse.x >= 33 && g_mouse.x <= 123) {
                g_projectTab = 0;
                return;
            } else if (g_mouse.x >= 129 && g_mouse.x <= 219) {
                g_projectTab = 1;
                return;
            }
        }
        if (g_projectTab == 0 && g_mouse.x >= 37 && g_mouse.x <= 180 && g_mouse.y >= 98 && g_mouse.y <= 112) {
            g_autoSaveEnabled = !g_autoSaveEnabled;
            return;
        }
        if (g_mouse.x >= 80 && g_mouse.x <= 145 && g_mouse.y >= 136 && g_mouse.y <= 151) {
            if (g_projectTab == 0) {
                int ok = Song_SaveADS(g_projectFilename, (uint8_t)g_selectedChannel, (uint8_t)g_selectedOctave, (uint8_t)g_selectedPreset);
                strcpy(g_projectStatusMsg, ok ? "PROJECT SAVED!" : "SAVE FAILED!");
                if (ok) g_dialog = DIALOG_NONE;
            } else {
                uint8_t ch = 0, oct = 4, inst = 0;
                int ok = Song_LoadADS(g_projectFilename, &ch, &oct, &inst);
                if (ok) {
                    g_selectedChannel = ch;
                    g_selectedOctave  = oct;
                    g_selectedPreset  = inst;
                    g_cursorRow       = g_song.currentRow;
                    strcpy(g_projectStatusMsg, "PROJECT LOADED!");
                    g_dialog = DIALOG_NONE;
                } else {
                    strcpy(g_projectStatusMsg, "LOAD FAILED!");
                }
            }
            return;
        }
        if (g_mouse.x >= 170 && g_mouse.x <= 235 && g_mouse.y >= 136 && g_mouse.y <= 151) {
            g_dialog = DIALOG_NONE;
            return;
        }
        return;
    }

    if (g_dialog == DIALOG_IMPORT_EXPORT_WINDOW) {
        if (g_mouse.y >= 38 && g_mouse.y <= 51) {
            if (g_mouse.x >= 33 && g_mouse.x <= 103) {
                g_importExportTab = 0;
                return;
            } else if (g_mouse.x >= 109 && g_mouse.x <= 179) {
                g_importExportTab = 1;
                return;
            }
        }
        if (g_mouse.y >= 73 && g_mouse.y <= 85) {
            if (g_mouse.x >= 95 && g_mouse.x <= 140) {
                g_isDroFormat = 0;
                strcpy(g_audioFilename, "TRACK1.IMF");
                return;
            } else if (g_mouse.x >= 150 && g_mouse.x <= 195) {
                g_isDroFormat = 1;
                strcpy(g_audioFilename, "TRACK1.DRO");
                return;
            } else if (!g_isDroFormat && g_mouse.x >= 205 && g_mouse.x <= 281) {
                if (g_importExportTab == 0) {
                    CycleImportImfHz();
                } else {
                    CycleExportImfHz();
                }
                return;
            }
        }
        if (g_importExportTab == 1 && g_mouse.x >= 37 && g_mouse.x <= 220 && g_mouse.y >= 108 && g_mouse.y <= 122) {
            g_exportSupportingBnk = !g_exportSupportingBnk;
            return;
        }
        if (g_mouse.x >= 80 && g_mouse.x <= 145 && g_mouse.y >= 142 && g_mouse.y <= 157) {
            ExecuteImportExport();
            return;
        }
        if (g_mouse.x >= 170 && g_mouse.x <= 235 && g_mouse.y >= 142 && g_mouse.y <= 157) {
            g_dialog = DIALOG_NONE;
            return;
        }
        return;
    }

    if (g_dialog == DIALOG_INST_WINDOW) {
        if (g_mouse.x >= 288 && g_mouse.x <= 303 && g_mouse.y >= 11 && g_mouse.y <= 25) {
            g_dialog = DIALOG_NONE;
            g_scrollbarDragging = 0;
            return;
        }
        if (g_mouse.x < 12 || g_mouse.x > 308 || g_mouse.y < 10 || g_mouse.y > 190) {
            g_dialog = DIALOG_NONE;
            g_scrollbarDragging = 0;
            return;
        }

        if (g_mouse.x >= 18 && g_mouse.x <= 88 && g_mouse.y >= 12 && g_mouse.y <= 25) {
            g_instWindowTab = 0;
            g_instWindowPage = 0;
            g_scrollbarDragging = 0;
            return;
        }
        if (g_mouse.x >= 92 && g_mouse.x <= 162 && g_mouse.y >= 12 && g_mouse.y <= 25) {
            g_instWindowTab = 1;
            g_scrollbarDragging = 0;
            return;
        }
        if (Bank_GetBnkCount() > 0 && g_mouse.x >= 166 && g_mouse.x <= 246 && g_mouse.y >= 12 && g_mouse.y <= 25) {
            g_instWindowTab = 2;
            g_instWindowPage = 0;
            g_scrollbarDragging = 0;
            return;
        }

        if (g_instWindowTab == 0) {
            for (i = 0; i < 16; ++i) {
                int col = i / 8;
                int row = i % 8;
                int bx = 22 + col * 140;
                int by = 46 + row * 14;
                if (g_mouse.x >= bx && g_mouse.x <= bx + 134 && g_mouse.y >= by && g_mouse.y <= by + 12) {
                    g_selectedPreset = g_instWindowPage * 16 + i;
                    if (g_selectedPreset >= NUM_PRESET_INSTRUMENTS) g_selectedPreset = NUM_PRESET_INSTRUMENTS - 1;
                    PreviewInstrument(g_selectedPreset);
                    return;
                }
            }
            if (g_mouse.x >= 22 && g_mouse.x <= 82 && g_mouse.y >= 166 && g_mouse.y <= 180) {
                if (g_instWindowPage > 0) g_instWindowPage--;
                return;
            }
            if (g_mouse.x >= 92 && g_mouse.x <= 152 && g_mouse.y >= 166 && g_mouse.y <= 180) {
                if (g_instWindowPage < 3) g_instWindowPage++;
                return;
            }
        } else if (g_instWindowTab == 1) {
            totalVoices = Bank_GetBnkCount();

            if (g_mouse.x >= 248 && g_mouse.x <= 293 && g_mouse.y >= 47 && g_mouse.y <= 60) {
                int count = Bank_LoadBNK(g_bnkFilenameBuffer);
                if (count > 0) {
                    sprintf(g_bnkStatusMsg, "LOADED %d VOICES FROM BNK!", count);
                    g_instWindowTab = 2;
                    g_instWindowPage = 0;
                    PreviewInstrument(NUM_PRESET_INSTRUMENTS);
                } else {
                    strcpy(g_bnkStatusMsg, "FAILED TO LOAD BNK FILE!");
                }
                return;
            }

            if (totalVoices > 10 && g_mouse.x >= 284 && g_mouse.x <= 296 && g_mouse.y >= 94 && g_mouse.y <= 178) {
                g_scrollbarDragging = 1;
                g_instWindowPage = ((g_mouse.y - 94) * (totalVoices - 10)) / 84;
                if (g_instWindowPage < 0) g_instWindowPage = 0;
                if (g_instWindowPage > totalVoices - 10) g_instWindowPage = totalVoices - 10;
                return;
            }
        } else if (g_instWindowTab == 2 && Bank_GetBnkCount() > 0) {
            int totalPages = (Bank_GetBnkCount() + 15) / 16;
            for (i = 0; i < 16; ++i) {
                int col = i / 8;
                int row = i % 8;
                int bx = 22 + col * 140;
                int by = 46 + row * 14;
                if (g_mouse.x >= bx && g_mouse.x <= bx + 134 && g_mouse.y >= by && g_mouse.y <= by + 12) {
                    int bnkVoiceIdx = g_instWindowPage * 16 + i;
                    if (bnkVoiceIdx < Bank_GetBnkCount()) {
                        g_selectedPreset = NUM_PRESET_INSTRUMENTS + bnkVoiceIdx;
                        PreviewInstrument(g_selectedPreset);
                    }
                    return;
                }
            }
            if (g_mouse.x >= 22 && g_mouse.x <= 82 && g_mouse.y >= 166 && g_mouse.y <= 180) {
                if (g_instWindowPage > 0) g_instWindowPage--;
                return;
            }
            if (g_mouse.x >= 92 && g_mouse.x <= 152 && g_mouse.y >= 166 && g_mouse.y <= 180) {
                if (g_instWindowPage < totalPages - 1) g_instWindowPage++;
                return;
            }
        }
        return;
    }
}

static void HandleTrackerMouse(int shiftPressed, int ctrlPressed) {
    int keyIndex, note, oct, rowSlot, clickedChannel;
    int32_t startRow;
    uint16_t targetRow;
    unsigned long now;
    int totalInst;

    totalInst = Bank_GetTotalCount();

    if (g_mouse.y >= 160 && g_mouse.y <= 196) {
        keyIndex = (g_mouse.x - 10) / 21;
        if (keyIndex >= 0 && keyIndex < 14) {
            static const uint8_t whiteNotes[7] = {1, 3, 5, 6, 8, 10, 12};
            note = whiteNotes[keyIndex % 7];
            oct = g_selectedOctave + (keyIndex / 7);
            OPL_SetInstrument((uint8_t)g_selectedChannel, Bank_GetInstrument(g_selectedPreset));
            OPL_NoteOn((uint8_t)g_selectedChannel, (uint8_t)(note - 1), (uint8_t)oct);
            Song_SetNote((uint8_t)g_selectedChannel, g_cursorRow, (uint8_t)note, (uint8_t)oct, (uint8_t)g_selectedPreset);
        }
        return;
    }

    if (g_mouse.y >= 18 && g_mouse.y <= 30) {
        if (g_mouse.x >= 31) {
            clickedChannel = (g_mouse.x - 36) / 31;
            if (clickedChannel >= 0 && clickedChannel < MAX_CHANNELS) {
                g_selectedChannel = clickedChannel;
            }
        }
        return;
    }

    if (g_mouse.y >= 33 && g_mouse.y <= 150) {
        startRow = (int32_t)g_cursorRow >= 5 ? (int32_t)g_cursorRow - 5 : 0;
        if (startRow > (int32_t)PATTERN_ROWS - 10) startRow = (int32_t)PATTERN_ROWS - 10;
        rowSlot = (g_mouse.y - 33) / 12;

        if (g_mouse.x >= 4 && g_mouse.x <= 32) {
            targetRow = (uint16_t)(startRow + rowSlot);
            g_cursorRow = targetRow;
            g_song.currentRow = targetRow;
        } else if (g_mouse.x >= 33) {
            clickedChannel = (g_mouse.x - 36) / 31;
            targetRow = (uint16_t)(startRow + rowSlot);
            if (clickedChannel >= 0 && clickedChannel < MAX_CHANNELS) {
                TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[targetRow][clickedChannel];
                if (shiftPressed) {
                    Song_SetSelection(targetRow, (uint8_t)clickedChannel, !(n->flags & 0x01));
                    g_isShiftDragging = 1;
                } else {
                    Song_ClearSelections();
                    g_selectedChannel = clickedChannel;
                    g_cursorRow = targetRow;
                }
            }
        }
        return;
    }

    /* 4. Top Navigation Bar (Matches Updated Positions in UI_DrawApp) */
    if (g_mouse.y < 14) {
        /* LOGO (2..13) */
        if (g_mouse.x >= 2 && g_mouse.x <= 13) {
            g_dialog = DIALOG_ABOUT;
            return;
        }

        /* OCT Number (88..97) */
        if (g_mouse.x >= 88 && g_mouse.x <= 97) {
            if (shiftPressed) {
                g_selectedOctave = (g_selectedOctave + 10 > 7) ? 7 : g_selectedOctave + 10;
            } else {
                g_selectedOctave = (g_selectedOctave % 7) + 1;
            }
            return;
        }

        /* INST Number (124..138) */
        if (g_mouse.x >= 124 && g_mouse.x <= 138) {
            if (shiftPressed) {
                g_selectedPreset = (g_selectedPreset + 10) % totalInst;
            } else if (ctrlPressed) {
                g_selectedPreset = (g_selectedPreset - 1 + totalInst) % totalInst;
            } else {
                g_selectedPreset = (g_selectedPreset + 1) % totalInst;
            }
            return;
        }

        /* INST Label (103..123): Double-click opens Instrument Browser */
        if (g_mouse.x >= 103 && g_mouse.x < 124) {
            now = (unsigned long)clock();
            if ((now - g_lastInstClickTime < (CLOCKS_PER_SEC * 0.25)) &&
                abs(g_mouse.x - g_lastInstClickX) <= 3 &&
                abs(g_mouse.y - g_lastInstClickY) <= 3) {
                g_dialog = DIALOG_INST_WINDOW;
                g_instWindowTab = 0;
                g_instWindowPage = g_selectedPreset / 16;
                if (g_selectedPreset >= NUM_PRESET_INSTRUMENTS && Bank_GetBnkCount() > 0) {
                    g_instWindowTab = 2;
                    g_instWindowPage = (g_selectedPreset - NUM_PRESET_INSTRUMENTS) / 16;
                }
                g_lastInstClickTime = 0;
            } else {
                g_lastInstClickTime = now;
                g_lastInstClickX = g_mouse.x;
                g_lastInstClickY = g_mouse.y;
            }
            return;
        }

        /* BPM Number (158..174) */
        if (g_mouse.x >= 158 && g_mouse.x <= 174) {
            if (shiftPressed) {
                g_song.tempo = (g_song.tempo + 10 > 300) ? 300 : g_song.tempo + 10;
            } else if (ctrlPressed) {
                g_song.tempo = (g_song.tempo <= 31) ? 30 : g_song.tempo - 1;
            } else {
                g_song.tempo = (g_song.tempo + 1 > 300) ? 300 : g_song.tempo + 1;
            }
            return;
        }

        /* BPM Label (141..157): Double-click opens Set Playback Tempo */
        if (g_mouse.x >= 141 && g_mouse.x < 158) {
            now = (unsigned long)clock();
            if ((now - g_lastTempoClickTime < (CLOCKS_PER_SEC * 0.25)) &&
                abs(g_mouse.x - g_lastTempoClickX) <= 3 &&
                abs(g_mouse.y - g_lastTempoClickY) <= 3) {
                g_dialog = DIALOG_TEMPO_WINDOW;
                sprintf(g_tempoInputBuffer, "%d", g_song.tempo);
                g_lastTempoClickTime = 0;
            } else {
                g_lastTempoClickTime = now;
                g_lastTempoClickX = g_mouse.x;
                g_lastTempoClickY = g_mouse.y;
            }
            return;
        }

        /* CLK (180..218): Double-click opens Track Engine & Resolution */
        if (g_mouse.x >= 180 && g_mouse.x <= 218) {
            now = (unsigned long)clock();
            if ((now - g_lastClkClickTime < (CLOCKS_PER_SEC * 0.25)) &&
                abs(g_mouse.x - g_lastClkClickX) <= 3 &&
                abs(g_mouse.y - g_lastClkClickY) <= 3) {
                g_dialog = DIALOG_TRACK_WINDOW;
                g_selectedClockPreset = g_song.clockHz;
                g_selectedRowMsPreset = g_song.rowMs;
                g_lastClkClickTime = 0;
            } else {
                g_lastClkClickTime = now;
                g_lastClkClickX = g_mouse.x;
                g_lastClkClickY = g_mouse.y;
            }
            return;
        }

        /* TIMECODE (227..282) */
        if (g_mouse.x >= 227 && g_mouse.x <= 282) {
            now = (unsigned long)clock();
            if ((now - g_lastTimeClickTime < (CLOCKS_PER_SEC * 0.25)) &&
                abs(g_mouse.x - g_lastTimeClickX) <= 3 &&
                abs(g_mouse.y - g_lastTimeClickY) <= 3) {
                g_dialog = DIALOG_JUMP_TIME;
                strcpy(g_timeInputBuffer, "00:00:00");
                g_timeInputSlot = 0;
                g_lastTimeClickTime = 0;
            } else {
                g_lastTimeClickTime = now;
                g_lastTimeClickX = g_mouse.x;
                g_lastTimeClickY = g_mouse.y;
            }
            return;
        }

        /* '?' Help (286..298) */
        if (g_mouse.x >= 286 && g_mouse.x <= 298) {
            g_dialog = DIALOG_HELP;
            g_helpPage = 0;
            return;
        }
    }
}

static void HandleTrackerRightClick(void) {
    int32_t startRow;
    int rowSlot;
    uint16_t targetRow;
    int clickedChannel;
    unsigned int shift = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
    int shiftPressed = (shift & 0x03) != 0;
    int totalInst = Bank_GetTotalCount();

    if (g_dialog == DIALOG_ABOUT || g_dialog == DIALOG_INST_INFO || g_dialog == DIALOG_TIME_INFO || g_dialog == DIALOG_TEMPO_INFO) {
        g_dialog = DIALOG_NONE;
        return;
    }

    if (g_dialog != DIALOG_NONE) return;

    if (g_mouse.y < 14) {
        /* INST Label (103..123) */
        if (g_mouse.x >= 103 && g_mouse.x < 124) {
            g_dialog = DIALOG_INST_INFO;
            return;
        }

        /* INST Number (124..138) */
        if (g_mouse.x >= 124 && g_mouse.x <= 138) {
            if (shiftPressed) {
                g_selectedPreset = (g_selectedPreset - 10 + totalInst) % totalInst;
            } else {
                g_selectedPreset = (g_selectedPreset - 1 + totalInst) % totalInst;
            }
            return;
        }

        /* BPM Label (141..157) */
        if (g_mouse.x >= 141 && g_mouse.x < 158) {
            g_dialog = DIALOG_TEMPO_INFO;
            return;
        }

        /* BPM Number (158..174) */
        if (g_mouse.x >= 158 && g_mouse.x <= 174) {
            if (shiftPressed) {
                g_song.tempo = (g_song.tempo < 40) ? 30 : g_song.tempo - 10;
            } else {
                g_song.tempo = (g_song.tempo <= 31) ? 30 : g_song.tempo - 1;
            }
            return;
        }

        /* CLK (180..218) */
        if (g_mouse.x >= 180 && g_mouse.x <= 218) {
            g_dialog = DIALOG_TEMPO_INFO;
            return;
        }

        /* OCT Number (88..97) */
        if (g_mouse.x >= 88 && g_mouse.x <= 97) {
            if (shiftPressed) {
                g_selectedOctave = (g_selectedOctave <= 4) ? 1 : g_selectedOctave - 3;
            } else {
                g_selectedOctave = (g_selectedOctave <= 1) ? 7 : g_selectedOctave - 1;
            }
            return;
        }

        /* TIMECODE (227..282) */
        if (g_mouse.x >= 227 && g_mouse.x <= 282) {
            g_dialog = DIALOG_TIME_INFO;
            return;
        }
        return;
    }

    if (g_mouse.y >= 18 && g_mouse.y <= 30) {
        if (g_mouse.x >= 31) {
            clickedChannel = (g_mouse.x - 36) / 31;
            if (clickedChannel >= 0 && clickedChannel < MAX_CHANNELS) {
                Song_ToggleChannelMuteSolo((uint8_t)clickedChannel);
            }
        }
        return;
    }

    if (g_mouse.y >= 33 && g_mouse.y <= 150) {
        startRow = (int32_t)g_cursorRow >= 5 ? (int32_t)g_cursorRow - 5 : 0;
        if (startRow > (int32_t)PATTERN_ROWS - 10) startRow = (int32_t)PATTERN_ROWS - 10;
        rowSlot = (g_mouse.y - 33) / 12;
        targetRow = (uint16_t)(startRow + rowSlot);

        if (g_mouse.x >= 4 && g_mouse.x <= 32) {
            Song_ToggleLoopPoint(targetRow);
        } else if (g_mouse.x >= 33) {
            clickedChannel = (g_mouse.x - 36) / 31;
            if (clickedChannel >= 0 && clickedChannel < MAX_CHANNELS) {
                Song_UndoRecord(targetRow, (uint8_t)clickedChannel);
                Song_ToggleNoteAttribute((uint8_t)clickedChannel, targetRow);
                Song_UndoNextBatch();
            }
        }
    }
}

int main(void) {
    int running = 1;
    int lastLeftMouse = 0;
    int lastRightMouse = 0;
    int ch;
    unsigned int key;
    unsigned char ascii, scan;
    unsigned int shift;
    int ctrlPressed, shiftPressed;
    int isSpaceKey, isCtrlPause;
    uint16_t loopStart, loopEnd;
    uint8_t hasLoop;
    static const int slotMap[6] = {0, 1, 3, 4, 6, 7};
    int totalInst;
    int isCtrl;
    int note;
    clock_t nowClock;
    clock_t autoSaveInterval;

    Timer_Init();
    Bank_Init();
    OPL_Init(OPL_BASE_PORT);
    Song_Init();
    UI_Init();
    UI_SetIdleCallback(Audio_Poll);

    for (ch = 0; ch < MAX_CHANNELS; ++ch) {
        OPL_SetInstrument((uint8_t)ch, Bank_GetInstrument(0));
    }

    g_lastRawPit = Timer_GetPitTicks();
    g_lastPitCount = g_lastRawPit;
    autoSaveInterval = (clock_t)CLOCKS_PER_SEC * 1200L;
    g_lastAutoSaveTick = clock();

    while (running) {
        Audio_Poll();

        shift = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
        shiftPressed = (shift & 0x03) != 0;
        ctrlPressed  = (shift & 0x04) != 0;

        if (g_dialog == DIALOG_INST_WINDOW && g_instWindowTab == 1 && g_scrollbarDragging) {
            if (g_mouse.leftDown) {
                int totalVoices = Bank_GetBnkCount();
                if (totalVoices > 10) {
                    g_instWindowPage = ((g_mouse.y - 94) * (totalVoices - 10)) / 84;
                    if (g_instWindowPage < 0) g_instWindowPage = 0;
                    if (g_instWindowPage > totalVoices - 10) g_instWindowPage = totalVoices - 10;
                }
            } else {
                g_scrollbarDragging = 0;
            }
        }

        if (g_dialog == DIALOG_NONE && g_mouse.leftDown && shiftPressed && g_isShiftDragging) {
            if (g_mouse.y >= 33 && g_mouse.y <= 150 && g_mouse.x >= 33) {
                int32_t startRow = (int32_t)g_cursorRow >= 5 ? (int32_t)g_cursorRow - 5 : 0;
                int rowSlot = (g_mouse.y - 33) / 12;
                int clickedCh = (g_mouse.x - 36) / 31;
                uint16_t targetRow;
                if (startRow > (int32_t)PATTERN_ROWS - 10) startRow = (int32_t)PATTERN_ROWS - 10;
                targetRow = (uint16_t)(startRow + rowSlot);
                if (clickedCh >= 0 && clickedCh < MAX_CHANNELS) {
                    Song_SetSelection(targetRow, (uint8_t)clickedCh, 1);
                }
            }
        }
        if (!g_mouse.leftDown) {
            g_isShiftDragging = 0;
        }

        Mouse_Update();

        if (g_mouse.leftDown && !lastLeftMouse) {
            if (g_dialog != DIALOG_NONE) {
                HandleDialogMouse(&running);
            } else {
                HandleTrackerMouse(shiftPressed, ctrlPressed);
            }
        }
        lastLeftMouse = g_mouse.leftDown;

        if (g_mouse.rightDown && !lastRightMouse) {
            HandleTrackerRightClick();
        }
        lastRightMouse = g_mouse.rightDown;

        if (_bios_keybrd(_KEYBRD_READY)) {
            key = _bios_keybrd(_KEYBRD_READ);
            ascii = (unsigned char)(key & 0xFF);
            scan = (unsigned char)((key >> 8) & 0xFF);
            shift = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
            ctrlPressed = (shift & 0x04) != 0;
            shiftPressed = (shift & 0x03) != 0;
            totalInst = Bank_GetTotalCount();
            isCtrl = ctrlPressed || (((*(volatile uint8_t *)0x417) & 0x04) != 0);

            isSpaceKey = (ascii == ' ' || scan == 0x39);
            isCtrlPause = (isSpaceKey && isCtrl);

            if (g_dialog != DIALOG_NONE) {
                if (ascii == 27 || scan == 0x01) {
                    g_dialog = DIALOG_NONE;
                    g_helpPage = 0;
                    g_scrollbarDragging = 0;
                } else if (g_dialog == DIALOG_HELP) {
                    if (g_helpPage < 3) {
                        g_helpPage++;
                    } else {
                        g_dialog = DIALOG_NONE;
                        g_helpPage = 0;
                    }
                } else if (g_dialog == DIALOG_ABOUT) {
                    g_dialog = DIALOG_NONE;
                } else if (g_dialog == DIALOG_CLEAR_CONFIRM) {
                    if (ascii == 'y' || ascii == 'Y' || ascii == 13) {
                        Song_ClearAll();
                        g_cursorRow = 0;
                        g_song.currentRow = 0;
                    }
                    g_dialog = DIALOG_NONE;
                } else if (g_dialog == DIALOG_EXIT_CONFIRM) {
                    if (ascii == 'y' || ascii == 'Y' || ascii == 13) {
                        running = 0;
                    }
                    g_dialog = DIALOG_NONE;
                } else if (g_dialog == DIALOG_TEMPO_WINDOW) {
                    if (ascii == 13) {
                        int newTempo = atoi(g_tempoInputBuffer);
                        if (newTempo >= 30 && newTempo <= 300) g_song.tempo = (uint16_t)newTempo;
                        g_dialog = DIALOG_NONE;
                    } else if (ascii == 8) {
                        int len = (int)strlen(g_tempoInputBuffer);
                        if (len > 0) g_tempoInputBuffer[len - 1] = '\0';
                    } else if (ascii >= '0' && ascii <= '9') {
                        int len = (int)strlen(g_tempoInputBuffer);
                        if (len < 3) {
                            g_tempoInputBuffer[len] = (char)ascii;
                            g_tempoInputBuffer[len + 1] = '\0';
                        }
                    }
                } else if (g_dialog == DIALOG_TRACK_WINDOW) {
                    if (ascii == 13) {
                        Song_SetClockHz(g_selectedClockPreset);
                        if (g_selectedRowMsPreset != g_song.rowMs) {
                            Song_RescaleRowMs(g_selectedRowMsPreset);
                            g_cursorRow = g_song.currentRow;
                        }
                        g_dialog = DIALOG_NONE;
                    }
                } else if (g_dialog == DIALOG_JUMP_TIME) {
                    if (ascii == 13) {
                        ParseTimeCodeAndJump(g_timeInputBuffer);
                        g_dialog = DIALOG_NONE;
                    } else if (ascii == 8) {
                        if (g_timeInputSlot > 0) g_timeInputSlot--;
                        g_timeInputBuffer[slotMap[g_timeInputSlot]] = '0';
                    } else if (ascii >= '0' && ascii <= '9') {
                        g_timeInputBuffer[slotMap[g_timeInputSlot]] = (char)ascii;
                        if (g_timeInputSlot < 5) g_timeInputSlot++;
                    }
                } else if (g_dialog == DIALOG_PROJECT_WINDOW) {
                    if (ascii == 13) {
                        if (g_projectTab == 0) {
                            int ok = Song_SaveADS(g_projectFilename, (uint8_t)g_selectedChannel, (uint8_t)g_selectedOctave, (uint8_t)g_selectedPreset);
                            strcpy(g_projectStatusMsg, ok ? "PROJECT SAVED!" : "SAVE FAILED!");
                            if (ok) g_dialog = DIALOG_NONE;
                        } else {
                            uint8_t ch = 0, oct = 4, inst = 0;
                            int ok = Song_LoadADS(g_projectFilename, &ch, &oct, &inst);
                            if (ok) {
                                g_selectedChannel = ch;
                                g_selectedOctave  = oct;
                                g_selectedPreset  = inst;
                                g_cursorRow       = g_song.currentRow;
                                strcpy(g_projectStatusMsg, "PROJECT LOADED!");
                                g_dialog = DIALOG_NONE;
                            } else {
                                strcpy(g_projectStatusMsg, "LOAD FAILED!");
                            }
                        }
                    } else if (ascii == '\t') {
                        g_projectTab = !g_projectTab;
                    } else if (isSpaceKey && g_projectTab == 0) {
                        g_autoSaveEnabled = !g_autoSaveEnabled;
                    } else if (ascii == 8) {
                        int len = (int)strlen(g_projectFilename);
                        if (len > 0) g_projectFilename[len - 1] = '\0';
                    } else if (ascii >= 32 && ascii <= 126) {
                        int len = (int)strlen(g_projectFilename);
                        if (len < 24) {
                            g_projectFilename[len] = (char)ascii;
                            g_projectFilename[len + 1] = '\0';
                        }
                    }
                } else if (g_dialog == DIALOG_IMPORT_EXPORT_WINDOW) {
                    if (ascii == 13) {
                        ExecuteImportExport();
                    } else if (ascii == '\t') {
                        g_isDroFormat = !g_isDroFormat;
                        strcpy(g_audioFilename, g_isDroFormat ? "TRACK1.DRO" : "TRACK1.IMF");
                    } else if (isSpaceKey && g_importExportTab == 1) {
                        g_exportSupportingBnk = !g_exportSupportingBnk;
                    } else if (ascii == 8) {
                        int len = (int)strlen(g_audioFilename);
                        if (len > 0) g_audioFilename[len - 1] = '\0';
                    } else if (ascii >= 32 && ascii <= 126) {
                        int len = (int)strlen(g_audioFilename);
                        if (len < 24) {
                            g_audioFilename[len] = (char)ascii;
                            g_audioFilename[len + 1] = '\0';
                        }
                    }
                } else if (g_dialog == DIALOG_INST_WINDOW) {
                    if (g_instWindowTab == 1) {
                        if (ascii == 13) {
                            int count = Bank_LoadBNK(g_bnkFilenameBuffer);
                            if (count > 0) {
                                sprintf(g_bnkStatusMsg, "LOADED %d VOICES FROM BNK!", count);
                                g_instWindowTab = 2;
                                g_instWindowPage = 0;
                                PreviewInstrument(NUM_PRESET_INSTRUMENTS);
                            } else {
                                strcpy(g_bnkStatusMsg, "FAILED TO LOAD BNK FILE!");
                            }
                        } else if (ascii == 8) {
                            int len = (int)strlen(g_bnkFilenameBuffer);
                            if (len > 0) g_bnkFilenameBuffer[len - 1] = '\0';
                        } else if (ascii >= 32 && ascii <= 126) {
                            int len = (int)strlen(g_bnkFilenameBuffer);
                            if (len < 24) {
                                g_bnkFilenameBuffer[len] = (char)ascii;
                                g_bnkFilenameBuffer[len + 1] = '\0';
                            }
                        }
                    }
                } else if (g_dialog == DIALOG_INST_INFO || g_dialog == DIALOG_TIME_INFO || g_dialog == DIALOG_TEMPO_INFO) {
                    g_dialog = DIALOG_NONE;
                }
            } else {
                if (ascii == 27 || scan == 0x01) {
                    g_dialog = DIALOG_EXIT_CONFIRM;
                } else if (ascii == 24 || (isCtrl && (scan == 0x2D || ascii == 'x' || ascii == 'X'))) {
                    Song_CutSelection(g_cursorRow, (uint8_t)g_selectedChannel);
                } else if (ascii == 3 || (isCtrl && (scan == 0x2E || ascii == 'c' || ascii == 'C'))) {
                    Song_CopySelection(g_cursorRow, (uint8_t)g_selectedChannel);
                } else if (ascii == 22 || (isCtrl && (scan == 0x2F || ascii == 'v' || ascii == 'V'))) {
                    Song_PasteClipboard(g_cursorRow, (uint8_t)g_selectedChannel);
                } else if (ascii == 26 || (isCtrl && (scan == 0x2C || ascii == 'z' || ascii == 'Z'))) {
                    Song_Undo();
                } else if (scan == 0x49) {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    if (n->note > 0) {
                        int semis = (n->octave * 12) + (n->note - 1);
                        if (semis < 7 * 12 + 11) {
                            Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                            semis++;
                            n->octave = (uint8_t)(semis / 12);
                            n->note = (uint8_t)((semis % 12) + 1);
                            Song_UndoNextBatch();
                            OPL_SetInstrument((uint8_t)g_selectedChannel, Bank_GetInstrument(n->instrument));
                            OPL_NoteOn((uint8_t)g_selectedChannel, (uint8_t)(n->note - 1), (uint8_t)n->octave);
                        }
                    }
                } else if (scan == 0x51) {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    if (n->note > 0) {
                        int semis = (n->octave * 12) + (n->note - 1);
                        if (semis > 12) {
                            Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                            semis--;
                            n->octave = (uint8_t)(semis / 12);
                            n->note = (uint8_t)((semis % 12) + 1);
                            Song_UndoNextBatch();
                            OPL_SetInstrument((uint8_t)g_selectedChannel, Bank_GetInstrument(n->instrument));
                            OPL_NoteOn((uint8_t)g_selectedChannel, (uint8_t)(n->note - 1), (uint8_t)n->octave);
                        }
                    }
                } else if (scan == 0x3B) {
                    g_dialog = DIALOG_CLEAR_CONFIRM;
                } else if (scan == 0x3C) {
                    g_dialog = DIALOG_PROJECT_WINDOW;
                    g_projectStatusMsg[0] = '\0';
                } else if (scan == 0x3D) {
                    g_dialog = DIALOG_IMPORT_EXPORT_WINDOW;
                    g_audioStatusMsg[0] = '\0';
                } else if (scan == 0x3E) {
                    g_dialog = DIALOG_HELP;
                    g_helpPage = 0;
                } else if (scan == 0x53) {
                    if (Song_HasSelections()) {
                        Song_DeleteSelectedNotes();
                    } else {
                        Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                        Song_ClearNote((uint8_t)g_selectedChannel, g_cursorRow);
                        Song_UndoNextBatch();
                    }
                } else if (scan == 0x4B) {
                    if (g_selectedChannel > 0) g_selectedChannel--;
                } else if (scan == 0x4D) {
                    if (g_selectedChannel < MAX_CHANNELS - 1) g_selectedChannel++;
                } else if (scan == 0x48) {
                    if (g_cursorRow > 0) g_cursorRow--;
                } else if (scan == 0x50) {
                    if ((uint32_t)g_cursorRow < PATTERN_ROWS - 1) g_cursorRow++;
                } else if (scan == 0x1A || ascii == '[') {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                    n->attribute = (n->attribute == ATTR_LETRING) ? ATTR_NONE : ATTR_LETRING;
                    Song_UndoNextBatch();
                } else if (scan == 0x1B || ascii == ']') {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                    n->attribute = (n->attribute == ATTR_DAMPEN) ? ATTR_NONE : ATTR_DAMPEN;
                    Song_UndoNextBatch();
                } else if (scan == 0x2B || scan == 0x56 || ascii == '\\') {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                    n->attribute = (n->attribute == ATTR_MUTE) ? ATTR_NONE : ATTR_MUTE;
                    Song_UndoNextBatch();
                } else if (ascii == '#') {
                    TrackerNote *n = &g_song.patterns[g_song.currentPattern].rows[g_cursorRow][g_selectedChannel];
                    Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                    n->attribute = (n->attribute == ATTR_BEND) ? ATTR_NONE : ATTR_BEND;
                    Song_UndoNextBatch();
                } else if (isSpaceKey) {
                    if (isCtrlPause) {
                        if (g_song.playing) {
                            g_song.playing = 0;
                            Song_ResetVoices();
                        } else {
                            g_song.playing = 1;
                            g_subStepIndex = 0;
                            g_subStepAccumMs = 0.0;
                            g_lastPitCount = Timer_GetPitTicks();
                        }
                    } else {
                        if (g_song.playing) {
                            g_song.playing = 0;
                            g_song.currentRow = 0;
                            g_cursorRow = 0;
                            Song_ResetVoices();
                        } else {
                            Song_GetLoopRange(&g_song.loop, &loopStart, &loopEnd, &hasLoop);
                            if (hasLoop) {
                                g_song.currentRow = loopStart;
                                g_cursorRow = loopStart;
                            } else {
                                g_song.currentRow = g_cursorRow;
                            }
                            g_song.playing = 1;
                            g_subStepIndex = 0;
                            g_subStepAccumMs = 0.0;
                            g_lastPitCount = Timer_GetPitTicks();
                        }
                    }
                } else if (ascii == '\t') {
                    totalInst = Bank_GetTotalCount();
                    g_selectedPreset = (g_selectedPreset + 1) % totalInst;
                } else if (ascii == '+' || ascii == '=') {
                    if (g_selectedOctave < 7) g_selectedOctave++;
                } else if (ascii == '-' || ascii == '_') {
                    if (g_selectedOctave > 1) g_selectedOctave--;
                } else {
                    int octOffset = 0;
                    note = KeyToNote((char)ascii, &octOffset);
                    if (note > 0) {
                        int finalOct = g_selectedOctave + octOffset;
                        if (finalOct > 7) finalOct = 7;
                        Song_UndoRecord(g_cursorRow, (uint8_t)g_selectedChannel);
                        OPL_SetInstrument((uint8_t)g_selectedChannel, Bank_GetInstrument(g_selectedPreset));
                        OPL_NoteOn((uint8_t)g_selectedChannel, (uint8_t)(note - 1), (uint8_t)finalOct);
                        Song_SetNote((uint8_t)g_selectedChannel, g_cursorRow, (uint8_t)note, (uint8_t)finalOct, (uint8_t)g_selectedPreset);
                        Song_UndoNextBatch();
                        if ((uint32_t)g_cursorRow < PATTERN_ROWS - 1) g_cursorRow++;
                    }
                }
            }
        }

        if (g_previewTimer > 0) {
            g_previewTimer--;
            if (g_previewTimer == 0) {
                OPL_NoteOff(8);
            }
        }

        if (g_autoSaveEnabled) {
            nowClock = clock();
            if ((nowClock - g_lastAutoSaveTick) >= autoSaveInterval) {
                Song_SaveADS("AUTOSAVE.ADS", (uint8_t)g_selectedChannel, (uint8_t)g_selectedOctave, (uint8_t)g_selectedPreset);
                g_lastAutoSaveTick = nowClock;
            }
        }

        Audio_Poll();
        UI_DrawApp(g_selectedChannel, g_selectedOctave, g_selectedPreset, (int)g_cursorRow);

        if (g_dialog == DIALOG_HELP) UI_DrawHelpDialog(g_helpPage);
        else if (g_dialog == DIALOG_ABOUT) UI_DrawAboutDialog();
        else if (g_dialog == DIALOG_PROJECT_WINDOW) UI_DrawProjectDialog(g_projectTab, g_projectFilename, g_autoSaveEnabled, g_projectStatusMsg);
        else if (g_dialog == DIALOG_IMPORT_EXPORT_WINDOW) UI_DrawImportExportDialog(g_importExportTab, g_audioFilename, g_isDroFormat, g_exportSupportingBnk, g_importImfHz, g_exportImfHz, g_audioStatusMsg);
        else if (g_dialog == DIALOG_CLEAR_CONFIRM) UI_DrawConfirmDialog("NEW TRACK", "CLEAR ALL NOTES? (YES/NO)");
        else if (g_dialog == DIALOG_EXIT_CONFIRM) UI_DrawConfirmDialog("QUIT ADSYNTH", "ARE YOU SURE? (YES/NO)");
        else if (g_dialog == DIALOG_INST_INFO) UI_DrawInstPopup(g_mouse.x, g_mouse.y, g_selectedPreset);
        else if (g_dialog == DIALOG_TIME_INFO) UI_DrawTimePopup(g_mouse.x, g_mouse.y, Song_GetTotalLengthMS());
        else if (g_dialog == DIALOG_TEMPO_INFO) UI_DrawTempoPopup(g_mouse.x, g_mouse.y, g_song.tempo, g_song.clockHz);
        else if (g_dialog == DIALOG_TEMPO_WINDOW) UI_DrawTempoDialog(g_tempoInputBuffer);
        else if (g_dialog == DIALOG_TRACK_WINDOW) UI_DrawTrackDialog(g_selectedClockPreset, g_song.rowMs);
        else if (g_dialog == DIALOG_JUMP_TIME) UI_DrawJumpDialog(g_timeInputBuffer, g_timeInputSlot, "ENTER TIME & PRESS JUMP");
        else if (g_dialog == DIALOG_INST_WINDOW) UI_DrawInstWindow(g_instWindowTab, g_selectedPreset, g_instWindowPage, g_bnkFilenameBuffer, g_bnkStatusMsg);

        UI_DrawMouse();
        UI_Present();
    }

    UI_SetIdleCallback(NULL);
    UI_Shutdown();
    OPL_Shutdown();
    Timer_Shutdown();
    printf("AdLib Synthesizer safely shut down.\n");
    return 0;
}

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "song.h"
#include "bank.h"

#define SCREEN_W 320
#define SCREEN_H 200

typedef enum {
    DIALOG_NONE = 0,
    DIALOG_HELP,
    DIALOG_PROJECT_WINDOW,        /* F2: Save & Load .ADS project files + AutoSave toggle */
    DIALOG_IMPORT_EXPORT_WINDOW,  /* F3: Import & Export IMF/DRO files + Supporting BNK */
    DIALOG_CLEAR_CONFIRM,
    DIALOG_EXIT_CONFIRM,
    DIALOG_ABOUT,
    DIALOG_INST_INFO,
    DIALOG_TIME_INFO,
    DIALOG_TEMPO_INFO,            /* Right-click TMP / CLK tooltip */
    DIALOG_TEMPO_WINDOW,          /* Double-click TMP editor */
    DIALOG_TRACK_WINDOW,          /* Double-click CLK Track Engine Clock & Row Resolution setup */
    DIALOG_JUMP_TIME,
    DIALOG_INST_WINDOW
} DialogMode;

typedef struct {
    int x, y;
    int leftDown;
    int rightDown;
} MouseState;

extern uint8_t *g_vgaBuffer;
extern MouseState g_mouse;

void UI_Init(void);
void UI_Shutdown(void);
void UI_Clear(uint8_t color);
void UI_Present(void);
void UI_SetIdleCallback(void (*cb)(void));

int UI_LoadTitlePCX(const char *filename);
int UI_LoadSplashPCX(const char *filename);
int UI_LoadFairyPCX(const char *filename);
int UI_LoadCursorPCX(const char *filename);

void UI_DrawRect(int x, int y, int w, int h, uint8_t color);
void UI_DrawDashedRect(int x, int y, int w, int h, uint8_t color);
void UI_FillRect(int x, int y, int w, int h, uint8_t color);
void UI_DrawDitherOverlay(int x, int y, int w, int h);
void UI_PrintText(int x, int y, const char *text, uint8_t color);
void UI_PrintTextMini(int x, int y, const char *text, uint8_t color);

void UI_DrawApp(int selectedChannel, int selectedOctave, int selectedPreset, int cursorRow);
void UI_DrawMouse(void);

void UI_DrawHelpDialog(int page);
void UI_DrawProjectDialog(int activeTab, const char *filename, int autoSaveEnabled, const char *status);
void UI_DrawImportExportDialog(int activeTab, const char *filename, int isDroFormat, int exportBnk, uint16_t importImfHz, uint16_t exportImfHz, const char *status);
void UI_DrawConfirmDialog(const char *title, const char *prompt);
void UI_DrawAboutDialog(void);
void UI_DrawInstPopup(int mouseX, int mouseY, int presetIdx);
void UI_DrawTimePopup(int mouseX, int mouseY, unsigned long totalMS);
void UI_DrawTempoPopup(int mouseX, int mouseY, uint16_t tempo, uint16_t clockHz);
void UI_DrawTempoDialog(const char *tempoBuffer);
void UI_DrawTrackDialog(uint16_t selectedClockHz, uint16_t selectedRowMs);
void UI_DrawJumpDialog(const char *timeStr, int activeSlot, const char *status);
void UI_DrawInstWindow(int activeTab, int selectedInst, int pageOrScroll, const char *bnkInputFilename, const char *status);

int Mouse_Init(void);
void Mouse_Update(void);

#endif

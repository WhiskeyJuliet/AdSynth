# ADSYNTH: DOS FM Music Tracker

ADSYNTH is a 9-channel FM music tracker for MS-DOS, designed for the YM3812 (OPL2) sound chip. It provides an environment for composing, editing, and exporting OPL2 music while supporting legacy AdLib file formats.

## Core Features

*   **OPL2 Synthesis**: Authentic 9-channel FM synthesis using the YM3812 chip.
*   **Sequencing**: 4,096-row, 9-channel tracker grid with support for multi-cell selection, clipboard operations (Cut, Copy, Paste), and a 50-step undo/redo history.
*   **Voice & Instrument Management**:
    *   **Bank Support**: Imports and exports standard AdLib Visual Composer (.BNK) banks, STANDARD.BNK the stock soundbank from AVC can be found here (https://www.vgmpf.com/Wiki/images/3/3c/Ad_Lib_Visual_Composer_%28DOS%29.zip)
    *   **Dynamic Patch Capture**: During IMF/DRO imports, the engine extracts distinct FM timbres from raw register streams, adding them to a custom internal bank for accurate voice reconstruction.
*   **Project Files**:
    *   **ADS Format**: A self-contained project format storing tracker data, loop markers, and embedded custom instrument patches.
    *   **Auto-Save**: Automatic 20-minute interval saving to `AUTOSAVE.ADS`.
*   **Compatibility**:
    *   Exports IMF and DRO files with proper OPL initialization and timbre register support for compatibility with external DOS players like AdPlay.
    *   Adjustable clock rates (70 Hz, 560 Hz, 700 Hz, 1000 Hz) to match original game engine requirements.

## Technical Specifications

*   **Compiler**: Open Watcom C/C++ 1.9 (ANSI C89 compliance).
*   **Platform**: 32-bit DOS (DOS/4GW).
*   **Display**: VGA Mode 13h (320x200, 256 colors) with a double-buffered rendering engine.
*   **Timing**: Non-blocking audio engine using `clock()` with a 50Hz sub-tick resolution for precise pitch bends and envelope management.

## Keyboard Shortcuts

| Action | Shortcut |
| :--- | :--- |
| **Piano Input** | `Z-M` (Low), `Q-U` (High) |
| **Navigation** | Arrow Keys |
| **Delete** | `DEL` |
| **Play/Stop** | `Space` |
| **Octave Shift** | `+` / `-` |
| **Attribute Toggle** | `[`, `]`, `\`, `#` |
| **Clipboard** | `Ctrl+C`, `Ctrl+X`, `Ctrl+V` |
| **Undo** | `Ctrl+Z` |
| **Menus** | `F2` (Project), `F3` (Import/Export), `F4` (Help) |

## Compilation

To compile the project, use the provided `BUILD.BAT` script with the Open Watcom 1.9 toolchain. The build system compiles the source units into `BUILD\*.OBJ` and links them to generate `ADLIBSYN.EXE`.

## License

ADSYNTH is released as a project for historical and educational interest in DOS-based FM synthesis. For details on dependencies or individual source files, please review the repository headers.

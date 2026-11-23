![Alt text](github.com/WhiskeyJuliet/AdSynth/blob/main/Supporting Files/SPLASH.png?raw=true "AdSynth")

# ADSYNTH: A MS-DOS AdLib FM Music Sequencer

ADSYNTH is a AdLib9-channel FM music AdLib Sequencer for MS-DOS, designed for the YM3812 (OPL2) sound chip. It provides an environment for composing, editing, and exporting OPL2 music while supporting legacy AdLib file formats.

## Core Features

*   **OPL2 Synthesis**: Authentic 9-channel FM synthesis using the YM3812 chip.
*   **Sequencing**: 4,096-row, 9-channel tracker grid with support for multi-cell selection, clipboard operations (Cut, Copy, Paste), and a 50-step undo/redo history.
*   **Voice & Instrument Management**:
    *   **Bank Support**: Imports and exports standard AdLib Visual Composer (.BNK) banks, STANDARD.BNK the stock soundbank from AVC can be found here (https://www.vgmpf.com/Wiki/images/3/37/AdLib_Visual_Composer_v1.51.zip).
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
Or download the currently available Pre-Release from the Releases page (https://github.com/WhiskeyJuliet/AdSynth/releases).

## Notes

The .DRO and .IMF support is still very WIP and while you can import some files and exports may work in some other applications it is by no means complete at this point.
I plan to improve compatibility with these formats in the future along with the option to use AdSynth's ADS project format for your own games with a library option. 

## License

ADSYNTH is released as a project for historical and educational interest in DOS-based FM synthesis. For details on dependencies or individual source files, please review the repository headers.
The code in the project was made with the aid of Google AI Studio. Sprite-work, UI & UX design by Whiskey_Juliet.

## Screenshots

![Alt text](Supporting Files/screenshot1.png?raw=true "AdSynth Splash") ![Alt text](Supporting Files/screenshot2.png?raw=true "AdSynth Banks") ![Alt text](Supporting Files/screenshot3.png?raw=true "AdSynth Timeline")

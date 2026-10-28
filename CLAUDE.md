# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Rules

- Never commit to this repository (no `git commit`, `git push`, amend, merge, or rebase), even when a task seems finished. The user reviews every change and commits it themselves, so leave all changes uncommitted in the working tree.

## What this is

PSG Tools is a Windows-only C++17 console app that plays and converts chiptunes for AY-3-8910 / YM2149 / AY8930 sound chips. It reads ZX Spectrum tracker modules (PT2, PT3, ASC, STC, ...), register-dump formats (PSG, RSF, VTX, YM) and VGM/VGZ (including SN76489 and NES APU tunes, which it converts to AY). It can play them through a software emulator or stream them to "AYM Streamer" hardware over a COM port, and it can convert them to register-stream formats.

## Build

Visual Studio 2022 (toolset v143) solution: `psg-tools/psg-tools.sln`. Configurations are `Debug`/`Release`; solution platforms are `x86`/`x64` (the projects call x86 `Win32`). No CMake, tests, or linter.

```bash
"/c/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe" psg-tools/psg-tools.sln -p:Configuration=Release -p:Platform=x64 -m
```

- Intermediates and `.lib` files go to `psg-tools/.vsbuild/<Config>_<Platform>/` (gitignored).
- The executable is written to the **repo root**: `psg-tools-x64.exe` / `psg-tools-x86.exe` for Release, `psg-tools-x64d.exe` / `psg-tools-x86d.exe` for Debug. The Release executables are committed, so a Release build changes tracked files.
- Every `.vcxproj` lists its source files explicitly. A new `.cpp`/`.h` must be added to the project's `.vcxproj` (and to `.vcxproj.filters` so it shows up in the right VS folder), or it won't build.
- Include roots are `psg-tools/libraries/sources` and the project's own `sources/`, so includes look like `"stream/Stream.h"` or `"argparse/argparse.hpp"`.
- The executables must run on Windows 7. That is why all projects link the static CRT (`/MT`, `/MTd`), which means no redistributable is needed, and define `_WIN32_WINNT=0x0601`, so newer APIs do not compile. Keep the v143 toolset: newer MSVC runtimes drop Windows 7. Keep sources ASCII-only, using escapes such as `wchar_t(0x2800)`, because MSVC reads them as cp1252.

## Running

Run from the repo root (the VS debugger also uses the repo root as its working directory):

```bash
./psg-tools-x64.exe sample.pt3                        # play with the software emulator
./psg-tools-x64.exe chiptunes/Mmcm -o out/*.rsf       # convert; '*' is replaced by each input's stem
./psg-tools-x64.exe sample.pt3 -o psg                 # bare type: writes sample.psg to the current directory
./psg-tools-x64.exe chiptunes/Davos -o list.m3u       # .m3u/.ayl output exports a playlist instead
./psg-tools-x64.exe sample.pt3 -p 3                   # stream to AYM Streamer on COM port index 3
./psg-tools-x64.exe chiptunes/Mmcm -w rec.wav          # play silently, recording everything played to one WAV file
```

Other options: `--chip-model <M> [<M2>]` (names from `Chip::Model`: `AY8910`, `YM2149`, `AY8930`, `Compatible`; two values mean TurboSound), `--chip-clock <MHz>`, `--chip-output Mono|Stereo [ABC|ACB|...]`, `--shuffle`, `--favorites <path>`, `--ui-test` (draws a fake playlist to check console rendering, no audio). With no input, it plays `favorites.m3u`. `shortcuts/*.bat` show typical invocations. Sample tunes are in `chiptunes/` and `sample.pt3`.

## Architecture

Three projects in the solution:
- **libraries** (static lib): vendored third-party code: zlib (VGZ), `lha/lh5` + `lha/lzh` (LH5 compressor/decompressor and LZH container for YM/VTX, from ar002), argparse, glob, magic_enum.
- **psg_audio** (static lib): all format, processing and playback logic.
- **psg_console** (exe): CLI parsing in `PSGTools.cpp`, interactive TUI player (`FilelistPlayer`, `ConsoleGUI`), batch converter (`FilelistConverter`).

### Console output (`psg_console/sources/`)

All output goes through the classic Win32 Console API, with no VT sequences, so it stays compatible with Windows 7. Under ConPTY (Windows Terminal), conhost translates these calls. The code never changes the console window size and lays everything out for the current size.
- `Console`: thin wrappers over the API (own screen buffer, cell writes, line output, input mode, title).
- `Screen`: while the program runs, it draws the whole window into a screen buffer of its own (`CreateConsoleScreenBuffer`). On its own, under ConPTY, that buffer is drawn on the terminal's main screen, and the terminal reflows it into its scrollback on resize. So where the console takes VT (Windows 10 and later), the buffer also sends `ESC[?1049h` for the alternate screen. If the process is killed, the console closes the buffer, and the terminal gets its main screen back by itself (checked in Windows Terminal). The history is at the top, and the live view of the current file is right below it. Every frame the program redraws the whole window in its own buffer. Do not go back to redrawing a region in the standard output buffer: under ConPTY conhost keeps only the window rows, the terminal keeps the scrollback, and each reflows its own copy on resize, which garbled the history.
  - **History.** The program keeps the history itself as entries: one row each. Text lines are cut at the window edge, and rows such as headers and rulers are drawn anew for each width. The wheel and PgUp/PgDn scroll it, and End goes to its end; a page is the history rows on screen, less the "[ N rows below ]" line shown in the last history row while it is scrolled. In the player the live view is pinned to the bottom of the window. The converter unpins it (`SetLivePinned(false)`): the live view is then the last rows of the history, seen only at its end, and a history scrolled up stays still while files are added. `Close()` switches back to the standard output buffer and prints the history there as plain lines, so after the program ends it stays in the console scrollback.
  - **Checking changes.** After changing it, check `--ui-test` in Windows Terminal and `conhost.exe`: resize the window and scroll the history in between.
- `ConsoleGUI`: lays out the current file (header, properties, register dump, progress) for the window size. `CommitFile` adds the header and the properties to the history. The player reads the input through `gui::Update` every frame; the converter calls `gui::UpdateBusy` from its progress callbacks, which reads it at most every 20 ms, so the history scrolls during a conversion too.
- `KeyboardInput`: key state built from console input events (`ReadConsoleInput`), so keys work in any console host and only while it has focus. It also counts mouse wheel notches. The mouse input mode is on, so the terminal sends the wheel to the program instead of turning it into arrow keys. To select text in Windows Terminal while the program runs, hold Shift.
- `gui::Shutdown` closes the screen, which prints the history. It also restores the input mode and the title and drops unread keys. It runs on abnormal exits too: `exit()` from a library, `abort`/`std::terminate` in any thread, and crashes. If the process is killed outright, the console switches back to the standard output buffer by itself, but the history is lost. Ctrl+C sets the termination flag, which the player and the converter check, including during decoding through `IsAbortRequested`.
- Colors set no background: `console::MapAttributes` takes the console's own background and default text color, so themes and non-black consoles work.
- When stdout is redirected, there is no screen, and the history (banner, file headers with full properties, errors) goes out as plain text.

### Core data model (`psg_audio/sources/stream/`)

- `Stream` is the whole song: a vector of `Frame`s (one per interrupt, usually 50 Hz), `info` (title/artist/...), `play` (frame rate, loop), plus two chip configs. `schip` is the source chip as the file describes it; `dchip` is the destination chip the user asked for on the command line.
- `Frame` holds registers for up to two chips (TurboSound). Each chip has 25 registers: bank A `0x00–0x0D` is the standard AY set, bank B `0x10–0x1A` is the AY8930 expanded mode (see `Register.h`). A frame tracks both the register data and a per-register "changed" diff. Encoders and outputs only emit changed registers, so `ResetChanges()`, `operator+=` and `SetChanges()` matter.
- `Property<T>` / `ReadOnlyProperty<T>` (`utils/Property.h`) are C#-style properties that wrap private members through getter/setter lambdas. They can't be copied, so classes holding them (such as `Chip`) define their own copy constructor and assignment operator.

### Pipeline

1. **Decode**: `FileDecoder::Decode` in `PSGAudio.cpp` tries each registered decoder in order. The first whose `Open()` accepts the file (by extension and signature) yields frames one at a time into `Stream::AddFrame`.
   - `decoders/modules/`: tracker-module decoders built on `ModuleDecoder` (`Init`/`Play`/`Loop`). They are ports of the original Z80 player routines and compute AY registers every tick.
   - `decoders/streams/`: register-dump formats. `DecodeVGM` feeds non-AY chips' writes through the simulators in `simulation/` (`SimSN76489`, `SimRP2A03`, `SimAY8910`), which convert another chip's state into AY frames with clock-ratio period scaling.
2. **Process**: `FrameProcessor` subclasses in `processing/` turn source-chip frames into destination-chip frames: `ChannelsOutputEnable`, `ChipClockRateConvert`, `ChannelsLayoutChange` (stereo layout), `AY8930EnvelopeFix`. Playback chains them in a `ProcessingChain` inside `Output::Init`. Encoding builds its own array in `FileEncoder::Encode`, without `ChannelsOutputEnable`.
3. **Output**, one of:
   - **Encode**: `FileEncoder::Encode` selects an encoder by output extension (`Encoder::CheckFileExt` in `Open`).
   - **Play**: `Player` runs a thread that steps frames into an `Output`. `Emulator` is the software output (`emulation/AY89XXPsg` plus `WaveAudio` over winmm, 44.1 kHz); `Streamer` writes register pairs to the serial port; `WaveFile` (`-w`) renders each frame to a 16-bit stereo WAV as it plays. `Emulator` and `WaveFile` share the chip emulation through `PCMOutput`. The WAV file covers the whole player session, and its header sizes are updated about every second, so a killed process still leaves a playable file. `Output::Write` handles AY8930 bank switching through bit `0x10` of `Mode_Bank` (R13).

### Adding or enabling a format

Register the class in the decoder or encoder array in `PSGAudio.cpp`. For decoders, also add the extension to `FileDecoder::FileTypes`; that pipe-separated list filters files during directory and playlist traversal. Several classes exist but aren't registered yet (work in progress): `DecodeAYM`, `DecodeMID`, `EncodeVGM`, `EncodeWAV`. `emulation/machines/` (`AYMachine`, `NSFMachine`, `VGMMachine` on top of `Z80Cpu`/`RP2A03Cpu`) compiles but isn't wired into anything.

AYM is the project's own compressed streaming format (`EncodeAYM`, with selectable compression profiles and LZ78). It is under active development. `documents/output_file_formats.md` has the RSF spec and a feature/compression comparison of the output formats.

### Debug output

`debug/DebugConfig.h` holds compile-time switches (`DBG_DECODING`, `DBG_DECODE_VGM`, `DBG_PROCESSING`, `DBG_ENCODE_AYM`). `DebugOutput<flag>` writes `dbg_<tag>.txt` to the working directory, and only in `_DEBUG` builds. `Stream` and `Frame` implement `DebugPayload` to dump their state.

@echo off
rem Renders mono WAV files of one loudness for A/B listening of the NES -> PSG conversion
rem into psg-compare\<folder> at the repo root, for each track of tracks.txt:
rem   "<name> [1 APU].wav"                 the VGM through the RP2A03 emulator of psg-tools
rem   "<name> [2 YM2149 before].wav"       converted by psg-tools-x64.exe of git revision BEFORE
rem   "<name> [3 YM2149 after].wav"        converted by psg-tools-x64.exe of the working tree
rem   "<name> [4 TurboSound before].wav", "<name> [5 TurboSound after].wav"
rem   "<name> [6 AY8930 before].wav",     "<name> [7 AY8930 after].wav"
rem
rem   render-compare.bat <folder>                    renders the tracks of tracks.txt
rem   render-compare.bat <folder> <file.vgz> <name>  renders one track
rem
rem The exe of BEFORE (HEAD unless set, e.g. set BEFORE=HEAD~1) is taken from git, so it is
rem the last committed exe, not a build of the committed sources; BEFORE_EXE gives any exe
rem instead, e.g. a copy of psg-tools-x64.exe made before the changes, AFTER_EXE replaces
rem the exe of the working tree; TRACKS gives another list, e.g. set TRACKS=dmc-tracks.txt;
rem NES_DMC=1 renders the APU with the DMC; PSG_OPTIONS adds options to all conversions,
rem e.g. set PSG_OPTIONS=--chip-clock 2
setlocal
if "%~1"=="" (
    echo usage: render-compare.bat ^<folder^> [^<file.vgz^> ^<name^>]
    exit /b 1
)
set ROOT=%~dp0..\..
set OUT=%ROOT%\psg-compare\%~1\
set NES=%ROOT%\chiptunes\NES
set NES2WAV=%~dp0nes2wav.exe
set FROM_GIT=
if not defined BEFORE_EXE set FROM_GIT=1
if defined FROM_GIT set BEFORE_EXE=%TEMP%\psg-tools-before.exe
if not defined BEFORE set BEFORE=HEAD
if not defined AFTER_EXE set AFTER_EXE=%ROOT%\psg-tools-x64.exe
if not defined TRACKS set TRACKS=tracks.txt
set APU_MODE=
if "%NES_DMC%"=="1" set APU_MODE=--dmc
if not exist "%OUT%" mkdir "%OUT%"

if exist "%NES2WAV%" goto :ready
call "%~dp0build-nes2wav.bat" || exit /b 1

:ready
if not defined FROM_GIT goto :list
git -C "%ROOT%" cat-file blob %BEFORE%:psg-tools-x64.exe > "%BEFORE_EXE%" || exit /b 1

:list
if not "%~3"=="" goto :single

for /f "usebackq tokens=1,2 delims=|" %%a in ("%~dp0%TRACKS%") do call :render "%NES%\%%a" "%%b"
goto :end

:single
call :render "%~2" "%~3"

:end
if defined FROM_GIT del "%BEFORE_EXE%"
exit /b

:render
"%NES2WAV%" render %APU_MODE% "%~1" "%OUT%%~2 [1 APU].wav"
call :convert "%BEFORE_EXE%" "%~1" "%OUT%%~2 [2 YM2149 before].wav" YM2149
call :convert "%AFTER_EXE%"  "%~1" "%OUT%%~2 [3 YM2149 after].wav" YM2149
call :convert "%BEFORE_EXE%" "%~1" "%OUT%%~2 [4 TurboSound before].wav" YM2149 YM2149
call :convert "%AFTER_EXE%"  "%~1" "%OUT%%~2 [5 TurboSound after].wav" YM2149 YM2149
call :convert "%BEFORE_EXE%" "%~1" "%OUT%%~2 [6 AY8930 before].wav" AY8930
call :convert "%AFTER_EXE%"  "%~1" "%OUT%%~2 [7 AY8930 after].wav" AY8930
echo done: %~2
exit /b

:convert
"%~1" "%~2" -o "%~3" --duration Original --chip-model %4 %5 --chip-output Mono %PSG_OPTIONS% >nul 2>&1
"%NES2WAV%" level --mono "%~3"
exit /b

@echo off
rem Renders the NES triangle alone into psg-compare\triangle, for listening to the ways
rem the PSG can play it: "[T0 2A03]" is the APU, "[T1 ...]".."[T7 ...]" are YM2149 variants
rem (see TriangleVariants in nes2wav.cpp). All files are mono at one loudness.
rem
rem   render-triangle.bat                    renders the default set of tracks
rem   render-triangle.bat <file.vgz> <name>  renders one track
setlocal
set ROOT=%~dp0..\..
set OUT=%ROOT%\psg-compare\triangle\
set NES2WAV=%~dp0nes2wav.exe
if not exist "%OUT%" mkdir "%OUT%"

if exist "%NES2WAV%" goto :ready
call "%~dp0build-nes2wav.bat" || exit /b 1

:ready
if not "%~2"=="" goto :single

set NES=%ROOT%\chiptunes\NES
call :render "%~dp0triangle-scale.vgm" "Scale C2-C5"
call :render "%NES%\Super_Mario_Bros._3_(NES)\02 Overworld BGM.vgz" "SMB3 - Overworld BGM"
call :render "%NES%\Super_Mario_Bros._(NES)\01 Running About.vgz" "SMB - Running About"
call :render "%NES%\DuckTales_(NES)\05 African Mines.vgz" "DuckTales - African Mines"
call :render "%NES%\Darkwing_Duck_(NES)\01 Darkwing Duck Theme.vgz" "Darkwing Duck - Theme"
call :render "%NES%\Contra_(NES)\03 Jungle Battle (Stage 1, 7).vgz" "Contra - Jungle Battle"
call :render "%NES%\RoboCop_3_(NES)\01 Title Screen.vgz" "RoboCop 3 - Title Screen"
exit /b

:single
call :render "%~1" "%~2"
exit /b

:render
"%NES2WAV%" tri "%~1" "%OUT%%~2"
echo done: %~2
exit /b

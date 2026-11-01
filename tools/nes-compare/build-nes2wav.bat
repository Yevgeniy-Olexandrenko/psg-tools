@echo off
rem Builds nes2wav.exe next to this script. It links the psg_audio and libraries
rem libs of the Release x64 build, so build the solution in Release x64 first.
setlocal
set ROOT=%~dp0..\..
set SRC=%ROOT%\psg-tools
set LIBS=%SRC%\.vsbuild\Release_x64

if exist "%LIBS%\psg_audio.lib" goto :find_vs
echo psg_audio.lib not found: build the solution in Release x64 first
exit /b 1

:find_vs
for %%e in (Enterprise Professional Community BuildTools) do (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat" set VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat
)
if defined VCVARS goto :build
echo Visual Studio 2022 not found
exit /b 1

:build
call "%VCVARS%" >nul 2>&1
cl /nologo /O2 /EHsc /std:c++17 /MT /D_WIN32_WINNT=0x0601 /D_CRT_SECURE_NO_WARNINGS ^
   /I "%SRC%\psg_audio\sources" /I "%SRC%\libraries\sources" ^
   "%~dp0nes2wav.cpp" "%LIBS%\psg_audio.lib" "%LIBS%\libraries.lib" ^
   /Fo"%TEMP%\nes2wav.obj" /Fe"%~dp0nes2wav.exe" /link /LTCG
exit /b %errorlevel%

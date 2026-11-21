@echo off
setlocal

set WATCOM=C:\WATCOM
set PATH=%WATCOM%\BINNT;%WATCOM%\BINW;%PATH%
set EDPATH=%WATCOM%\EDDAT
set INCLUDE=%WATCOM%\H;%WATCOM%\H\DOS;%CD%\SRC
set LIB=%WATCOM%\LIB386;%WATCOM%\LIB386\DOS

if not exist BUILD mkdir BUILD

echo ========================================================
echo  Compiling ADLIBSYN
echo ========================================================

wcc386 SRC\BANK.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\BANK.OBJ
if errorlevel 1 exit /b 1

wcc386 SRC\OPL.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\OPL.OBJ
if errorlevel 1 exit /b 1

wcc386 SRC\LOOP.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\LOOP.OBJ
if errorlevel 1 exit /b 1

wcc386 SRC\SONG.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\SONG.OBJ
if errorlevel 1 exit /b 1

wcc386 SRC\UI.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\UI.OBJ
if errorlevel 1 exit /b 1

wcc386 SRC\MAIN.C -bt=dos -mf -5r -fp5 -fpi87 -oneatx -zp4 -fo=BUILD\MAIN.OBJ
if errorlevel 1 exit /b 1

echo.
echo ========================================================
echo  Linking BUILD\ADSYNTH.EXE...
echo ========================================================
wlink system dos4g name BUILD\ADSYNTH.EXE file BUILD\MAIN.OBJ,BUILD\OPL.OBJ,BUILD\SONG.OBJ,BUILD\UI.OBJ,BUILD\LOOP.OBJ,BUILD\BANK.OBJ
if errorlevel 1 exit /b 1

if not exist BUILD\dos4gw.exe (
    if exist %WATCOM%\BINW\dos4gw.exe (
        copy %WATCOM%\BINW\dos4gw.exe BUILD\ > NUL
    )
)

echo ========================================================
echo  BUILD SUCCESSFUL: BUILD\ADSYNTH.EXE is ready!
echo ========================================================
@echo off
setlocal
set "ROOT=%~dp0.."
set "OUTDIR=%ROOT%\bin"
set "LIBDIR=%ROOT%\lib"
set "SRCDIR=%ROOT%\src\parser"
set "INCDIR=%ROOT%\src\parser"
set "OUT_DLL=%OUTDIR%\parser.dll"
set "OUT_LIB=%LIBDIR%\libparser.a"
set "STD=-std=c++17 -static -static-libgcc -static-libstdc++"
set "WARN=-Wall -Wextra"
set "DEFS=-DNEXUS_PARSER_EXPORTS"
if "%1"=="debug" (
    set "FLAGS=%STD% %WARN% %DEFS% -g -DDEBUG"
) else (
    set "FLAGS=%STD% %WARN% %DEFS% -O2"
)
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
if not exist "%LIBDIR%" mkdir "%LIBDIR%"
g++ %FLAGS% -I"%INCDIR%" -shared -o "%OUT_DLL%" "%SRCDIR%\OIR_Generator.cpp" -Wl,--out-implib,"%OUT_LIB%"
if errorlevel 1 exit /b %ERRORLEVEL%
echo [OK] Legacy parser DLL built: %OUT_DLL%
endlocal

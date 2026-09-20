@echo off
REM build.bat — NexusFlow Parser DLL build script (MinGW g++)
REM Usage: build.bat [debug]

set OUTDIR=bin
set LIBDIR=lib
set SRCDIR=src\parser
set INCDIR=src\parser
set OUT_DLL=%OUTDIR%\parser.dll
set OUT_LIB=%LIBDIR%\libparser.a
set STD=-std=c++17 -static -static-libgcc -static-libstdc++
set WARN=-Wall -Wextra
set DEFS=-DNEXUS_PARSER_EXPORTS

if "%1"=="debug" (
    set FLAGS=%STD% %WARN% %DEFS% -g -DDEBUG
    echo [BUILD] Debug mode
) else (
    set FLAGS=%STD% %WARN% %DEFS% -O2
    echo [BUILD] Release mode
)

if not exist %OUTDIR% mkdir %OUTDIR%
if not exist %LIBDIR% mkdir %LIBDIR%

echo [BUILD] Compiling parser DLL...
g++ %FLAGS% -I%INCDIR% -shared -o %OUT_DLL% %SRCDIR%\OIR_Generator.cpp -Wl,--out-implib,%OUT_LIB%

if %ERRORLEVEL% == 0 (
    echo [OK] parser.dll built successfully: %OUT_DLL%
) else (
    echo [FAIL] Build failed with error %ERRORLEVEL%
    exit /b 1
)

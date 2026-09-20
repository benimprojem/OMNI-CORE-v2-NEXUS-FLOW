@echo off
setlocal enabledelayedexpansion
cls
echo [BUILD] Compiling Analyzer DLL...

if not exist "bin" mkdir bin

g++ -std=c++17 -Wall -Wextra -shared -fPIC ^
    -I"src" ^
    "src/analyzer/analyzer_main.cpp" ^
    "src/analyzer/Analyzer.cpp" ^
    -o "bin/analyzer.dll" ^
    -static -static-libgcc -static-libstdc++ 

if %ERRORLEVEL% equ 0 (
    echo [OK] analyzer.dll built successfully: bin\analyzer.dll
    exit /b 0
) else (
    echo [FAIL] Analyzer Build failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

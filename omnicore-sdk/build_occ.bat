@echo off
REM build_occ.bat — occ.exe derleme scripti
set OUT_DIR=bin
if not exist %OUT_DIR% mkdir %OUT_DIR%
echo [BUILD] Compiling occ.exe...
g++ -std=c++17 -static -static-libgcc -static-libstdc++ -Wall -Wextra src/occ/main.cpp -o %OUT_DIR%/occ.exe
if %ERRORLEVEL% == 0 (
    echo [OK] bin\occ.exe built successfully.
) else (
    echo [FAIL] Build failed with error %ERRORLEVEL%.
    exit /b 1
)

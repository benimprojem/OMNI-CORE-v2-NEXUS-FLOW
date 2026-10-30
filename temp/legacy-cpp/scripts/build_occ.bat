@echo off
setlocal
set "ROOT=%~dp0.."
set "OUT_DIR=%ROOT%\bin"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
echo [LEGACY] Building archived C++ OCC...
g++ -std=c++17 -static -static-libgcc -static-libstdc++ -Wall -Wextra "%ROOT%\src\occ\main.cpp" -o "%OUT_DIR%\occ.exe"
if errorlevel 1 exit /b %ERRORLEVEL%
echo [OK] Legacy C++ OCC built: %OUT_DIR%\occ.exe
endlocal

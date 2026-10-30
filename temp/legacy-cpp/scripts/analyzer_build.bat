@echo off
setlocal
set "ROOT=%~dp0.."
set "OUTDIR=%ROOT%\bin"
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
g++ -std=c++17 -Wall -Wextra -shared -fPIC -I"%ROOT%\src" "%ROOT%\src\analyzer\analyzer_main.cpp" "%ROOT%\src\analyzer\Analyzer.cpp" -o "%OUTDIR%\analyzer.dll" -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b %ERRORLEVEL%
echo [OK] Legacy analyzer DLL built: %OUTDIR%\analyzer.dll
endlocal

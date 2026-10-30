@echo off
setlocal

set "SDK_DIR=%~dp0"
set "BUILD_DIR=%SDK_DIR%build"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [FAIL] CMake bulunamadi. Lutfen CMake kurun ve PATH'e ekleyin.
    exit /b 1
)

cmake -S "%SDK_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %ERRORLEVEL%

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b %ERRORLEVEL%

ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 exit /b %ERRORLEVEL%

echo [OK] C OCC build ve test komutlari tamamlandi.
endlocal

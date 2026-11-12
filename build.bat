@echo off
setlocal
cd /d "%~dp0"

set CC=gcc
set CFLAGS=-std=c11 -O2 -Wall -Wextra -Wpedantic

%CC% %CFLAGS% -o chronos.exe src\main.c src\platform.c src\screen.c src\ui.c src\history.c src\stopwatch.c src\countdown.c -lshell32 -luser32
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build OK: chronos.exe
endlocal
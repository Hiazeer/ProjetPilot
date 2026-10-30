@echo off
rem Compile ProjetPilot.dll (64 bits) avec MinGW-w64 (g++).
setlocal
cd /d "%~dp0"
where g++ >nul 2>nul || (echo g++ introuvable : installez MinGW-w64, par ex. "winget install BrechtSanders.WinLibs.POSIX.UCRT" & exit /b 1)
if not exist build mkdir build

set FLAGS=-O2 -std=c++17 -Wall -DUNICODE -D_UNICODE -static -s

g++ %FLAGS% -shared -o build\ProjetPilot.dll src\Plugin.cpp src\Projet.cpp src\Gantt.cpp -lcomctl32 -lshell32 -luser32 -lgdi32 || exit /b 1
g++ %FLAGS% -o build\test_projet.exe tests\test_projet.cpp src\Projet.cpp src\Gantt.cpp || exit /b 1

build\test_projet.exe exemple.projet build\gantt_exemple.html || exit /b 1
echo.
echo OK : build\ProjetPilot.dll

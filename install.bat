@echo off
rem Copie ProjetPilot.dll dans Notepad++ (demande les droits administrateur). Fermez Notepad++ avant.
setlocal
cd /d "%~dp0"
net session >nul 2>nul || (powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs" & exit /b)

set DEST=%ProgramFiles%\Notepad++\plugins\ProjetPilot
if not exist "build\ProjetPilot.dll" (echo Lancez d'abord build.bat & pause & exit /b 1)
tasklist /fi "imagename eq notepad++.exe" | find /i "notepad++.exe" >nul && (echo Fermez Notepad++ puis relancez install.bat & pause & exit /b 1)
if not exist "%DEST%" mkdir "%DEST%"
copy /y "build\ProjetPilot.dll" "%DEST%\" >nul || (echo Copie impossible & pause & exit /b 1)
echo Installe dans %DEST%
echo Ouvrez Notepad++ : menu Modules ^> Projet Pilot
pause

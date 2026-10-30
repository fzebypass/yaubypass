@echo off
setlocal EnableExtensions EnableDelayedExpansion
title yau bypass
color 0B

set "REPO=fzebypass/yaubypass"
set "ASSET=yaubypass.exe"
set "DLURL=https://github.com/%REPO%/releases/latest/download/%ASSET%"
set "DIR=%LOCALAPPDATA%\YauBypass"
set "EXE=%DIR%\%ASSET%"
set "BAK=%DIR%\yaubypass.dat"
set "BOOT=%DIR%\boot.cmd"
set "TASK=yaubypass"

echo(
echo   yau bypass
echo   ---------------------------------------------
echo(

rem ---------- Adminrechte ----------
fltmc >nul 2>&1
if errorlevel 1 (
    echo   [*] Administratorrechte werden angefordert
    echo       Bestaetige die Windows-Abfrage.
    mshta vbscript:CreateObject("Shell.Application").ShellExecute("cmd","/c """"%~f0""""","","runas",1)(window.close)
    exit /b 0
)

rem ---------- Installieren ----------
echo   [*] Installiere nach %DIR%
if not exist "%DIR%" md "%DIR%" >nul 2>&1
if not exist "%DIR%" goto fail_copy

taskkill /f /im "%ASSET%" >nul 2>&1
taskkill /f /im "WizardExternal.exe" >nul 2>&1
timeout /t 1 >nul
if exist "%EXE%" del /f /q "%EXE%" >nul 2>&1

echo   [*] Aktuelle Version wird geladen ...
curl -L --fail --silent --show-error --retry 2 -o "%EXE%" "%DLURL%" 2>nul

set "SIZE="
if exist "%EXE%" for %%A in ("%EXE%") do set "SIZE=%%~zA"
if not defined SIZE goto fail_dl
if %SIZE% LSS 500000 goto fail_dl

rem Sicherheitskopie - der Cheat loescht sich beim Destruct selbst
copy /y "%EXE%" "%BAK%" >nul 2>&1

rem ---------- Start-Helfer ----------
> "%BOOT%" echo @echo off
>> "%BOOT%" echo cd /d "%%~dp0"
>> "%BOOT%" echo tasklist /fi "imagename eq %ASSET%" ^| find /i "%ASSET%" ^>nul ^&^& exit /b
>> "%BOOT%" echo if not exist "%ASSET%" if exist "yaubypass.dat" copy /y "yaubypass.dat" "%ASSET%" ^>nul 2^>^&1
>> "%BOOT%" echo if not exist "%ASSET%" curl -L --fail -s -o "%ASSET%" "%DLURL%"
>> "%BOOT%" echo if exist "%ASSET%" start "" "%ASSET%"
>> "%BOOT%" echo exit /b

rem ---------- Autostart ----------
schtasks /create /tn "%TASK%" /tr "\"%BOOT%\"" /sc onlogon /rl highest /f >nul 2>&1
if errorlevel 1 goto fail_task
schtasks /query /tn "%TASK%" /xml 2>nul | find /i "boot.cmd" >nul
if errorlevel 1 goto fail_task

rem ---------- Alte Reste ----------
if exist "%TEMP%\wz" rd /s /q "%TEMP%\wz" >nul 2>&1

echo   [*] %SIZE% Bytes - OK
echo   [*] Autostart eingerichtet
echo   ---------------------------------------------
echo(
echo   [!] Starte jetzt den PC neu.
echo       Danach startet yau bypass automatisch.
echo(
echo   [*] Neustart in 15 Sekunden.
echo       Eine beliebige Taste startet yau bypass sofort.
timeout /t 15

if errorlevel 1 goto start_now

shutdown /r /t 10 /c "yau bypass" >nul 2>&1
echo(
echo   [*] PC startet neu ...
exit /b 0

:start_now
echo(
start "" "%EXE%"
echo   [*] yau bypass gestartet - Taste F1 oeffnet das Menue.
exit /b 0

:fail_dl
echo   [!] Download fehlgeschlagen oder Datei blockiert.
echo       Antivirus pruefen, Internet pruefen, dann erneut starten.
echo       %DLURL%
echo   [!] Fenster schliesst in 15 Sekunden.
timeout /t 15 >nul
exit /b 1

:fail_copy
echo   [!] Ordner konnte nicht erstellt werden: %DIR%
echo   [!] Fenster schliesst in 15 Sekunden.
timeout /t 15 >nul
exit /b 1

:fail_task
echo   [!] Autostart konnte nicht eingerichtet werden.
echo       cmd als Administrator oeffnen und den Befehl erneut ausfuehren.
echo   [!] Fenster schliesst in 15 Sekunden.
timeout /t 15 >nul
exit /b 1

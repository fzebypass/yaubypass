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

echo(
echo   yau bypass
echo   ---------------------------------------------
echo(

rem ---------- Adminrechte ----------
if /i "%~1"=="elev" goto admin_ok

fltmc >nul 2>&1
if not errorlevel 1 goto admin_ok

set "TOK=%~2"
if "%TOK%"=="" set "TOK=%RANDOM%%RANDOM%"
set "OKF=%TEMP%\yb_%TOK%.ok"
del /f /q "%OKF%" >nul 2>&1

echo   [*] Administratorrechte werden angefordert
echo       Klicke im Windows-Fenster auf "Ja".
echo(

powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -ArgumentList 'elev %TOK%' -Verb RunAs" >nul 2>&1

set "N=0"
:wait_elev
if exist "%OKF%" goto elev_ok
set /a N+=1
if %N% GEQ 30 goto elev_denied
ping -n 2 127.0.0.1 >nul
goto wait_elev

:elev_ok
del /f /q "%OKF%" >nul 2>&1
exit /b 0

:elev_denied
echo   [!] Die Administratorrechte wurden nicht erteilt.
echo       Starte die Eingabeaufforderung mit Rechtsklick -^> "Als Administrator
echo       ausfuehren" und fuege den Befehl dort noch einmal ein.
echo   [!] Fenster schliesst in 20 Sekunden.
timeout /t 20 >nul
exit /b 1

:admin_ok
if not "%~2"=="" > "%TEMP%\yb_%~2.ok" echo ok

rem ---------- Installieren ----------
echo   [*] Installiere nach %DIR%
if not exist "%DIR%" md "%DIR%" >nul 2>&1
if not exist "%DIR%" goto fail_copy

taskkill /f /im "%ASSET%" >nul 2>&1
taskkill /f /im "WizardExternal.exe" >nul 2>&1
ping -n 2 127.0.0.1 >nul
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
set "AUTOSTART="
for %%N in (NVOverlayHost NvOverlayHost2 NvOverlayTask) do (
    if not defined AUTOSTART (
        schtasks /create /tn "%%N" /tr "\"%BOOT%\"" /sc onlogon /rl highest /f >nul 2>&1
        if not errorlevel 1 (
            schtasks /query /tn "%%N" >nul 2>&1
            if not errorlevel 1 set "AUTOSTART=%%N"
        )
    )
)
if not defined AUTOSTART (
    reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "NVOverlayHost" /t REG_SZ /d "\"%BOOT%\"" /f >nul 2>&1
    if errorlevel 1 goto fail_task
    set "AUTOSTART=Run-Key"
)

rem ---------- Alte Reste ----------
if exist "%TEMP%\wz" rd /s /q "%TEMP%\wz" >nul 2>&1

echo   [*] %SIZE% Bytes - OK
echo   [*] Autostart eingerichtet (%AUTOSTART%)
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
echo       Windows-Taste, "Eingabeaufforderung", Rechtsklick,
echo       "Als Administrator ausfuehren", Befehl erneut einfuegen.
echo   [!] Fenster schliesst in 15 Sekunden.
timeout /t 15 >nul
exit /b 1

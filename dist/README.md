# yau bypass — Start per cmd

Der User lädt nichts herunter und klickt nichts an. Er pastet **eine Zeile in cmd**:

```
curl -L -o "%TEMP%\yau.cmd" https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/install.cmd && "%TEMP%\yau.cmd"
```

Danach:

1. Das cmd-Skript fordert Administratorrechte an (Windows-Abfrage bestätigen).
2. Es lädt das neueste Release von `yaubypass.exe` nach `%LOCALAPPDATA%\YauBypass\`.
3. Größe und Datei werden geprüft — ein kaputter Build kommt nie durch.
4. Es legt `boot.cmd` daneben und registriert die Aufgabe **`yaubypass`**
   (Aufgabenplanung, *Bei Anmeldung*, *mit höchsten Privilegien*, also ohne UAC-Fenster).
5. Ansage: **„Starte jetzt den PC neu"** — nach 15 Sekunden startet der PC neu.
   Eine beliebige Taste bricht den Neustart ab und startet `yau bypass` sofort.
6. Nach dem Neustart startet `yau bypass` automatisch. **F1** öffnet das Menü.

Kein PowerShell: der Befehl, `install.cmd` und `boot.cmd` sind reines cmd.

---

## Warum die Aufgabe und nicht der Run-Key

`yaubypass.exe` verlangt Administratorrechte (Manifest). Über
`HKCU\...\Run` käme bei jeder Anmeldung ein UAC-Fenster — per Aufgabenplanung mit
*höchsten Privilegien* startet die EXE still.

## Nach dem Destruct

Der Cheat löscht sich beim Destruct selbst. `boot.cmd` prüft deshalb bei jeder
Anmeldung: fehlt `yaubypass.exe`, wird erst `yaubypass.dat` kopiert, sonst frisch aus
dem Release geladen — und dann gestartet. `boot.cmd` startet nie eine zweite Instanz.

## Update veröffentlichen

```
git tag v1.0.2
git push origin v1.0.2
```

Der Workflow [.github/workflows/build.yml](../.github/workflows/build.yml) baut die EXE,
benennt sie in `yaubypass.exe` um und hängt sie an das Release. Der Launcher holt sich
immer `/releases/latest/download/yaubypass.exe` — der User muss nichts tun.

## Wenn mal was klemmt

| Symptom | Ursache |
|---|---|
| `Download fehlgeschlagen oder Datei blockiert` | Antivirus hat die EXE gesperrt, oder das Repo ist privat/404 |
| `Autostart konnte nicht eingerichtet werden` | cmd wurde nicht als Administrator gestartet |
| Menü kommt nach dem Neustart nicht | Aufgabe `yaubypass` in der Aufgabenplanung prüfen |
| SmartScreen beim ersten Start | EXE ist nicht signiert (nur mit Zertifikat weg) |

Deinstallieren: Aufgabe `yaubypass` in der Aufgabenplanung löschen, Ordner
`%LOCALAPPDATA%\YauBypass` entfernen.

Der Quellcode wird nicht mehr automatisch aus `%TEMP%` gestartet — es gibt nur noch
diesen einen Installationsordner.

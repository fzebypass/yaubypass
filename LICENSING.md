# PRUDA — eine EXE, erst Key, dann Cheat

Loader und Cheat sind **eine Datei**: `Wizard External.exe`.
Start → goldes Key-Fenster mit dem Logo → Key wird geprüft → Cheat läuft → **F1** öffnet das Menü.
Ohne gültigen Key startet der Cheat nicht (das Fenster ist der einzige Weg weiter).

| Teil | Datei | Aufgabe |
|---|---|---|
| **Cheat + Gate** | `x64\Release\Wizard External.exe` | Key-Fenster, danach der Cheat |
| **Admin-Panel** | [admin/bin/admin.exe](admin/bin/admin.exe) | Keys erzeugen (1–100 am Stück) |
| **API** | [server/](server) | PHP + MySQL, hält die Keys |

Das alte Standalone-Loader-Projekt (`spoofer_imgui`) wird dafür **nicht mehr gebraucht** —
es liegt nur noch als Reserve da.

---

## Ablauf

```
Admin-Panel                  Server (PHP)                 Cheat-EXE
-----------                  ------------                 ---------
Tage + Anzahl wählen   →   INSERT licenses          ←   Key eintippen
Keys anzeigen/kopieren ←   OK|K1,K2,K3|LIFETIME         Server prüft
                                                         Cheat startet
                                                         F1 = Menü
```

- Der Key allein ist die Lizenz — keine Discord-ID, kein Account.
- Schreibweise egal: `nflgqsp lzl76 svcz` = `NFLG-QSPL-ZL76-SVCZ`.
- Falscher/abgelaufener/gesperrter Key → Fenster bleibt offen, rote Meldung,
  der Cheat startet nicht.

---

## Server einrichten (einmal)

1. Datenbank anlegen, [server/schema.sql](server/schema.sql) im SQL-Tab ausführen.
2. [server/config.php](server/config.php): DB-Zugang + `ADMIN_TOKEN` (langer Zufallswert).
3. `config.php`, `login.php`, `admin.php` nach `https://fivem.ovh/goonen/license/` hochladen.
4. [server/keys_lifetime_50.sql](server/keys_lifetime_50.sql) ausführen, wenn die 50
   Lifetime-Keys rein sollen (lesbare Liste: [server/keys_lifetime_50.txt](server/keys_lifetime_50.txt)).

Test:
```
curl -d "key=NFLG-QSPL-ZL76-SVCZ" https://fivem.ovh/goonen/license/login.php
```
Erwartet `OK|LIFETIME|0`.

Anderer Server? Datei **`api.txt`** neben die EXE (gilt für Cheat und Admin-Panel):

```
https://dein-host.de/license/
```

---

## Admin-Panel

1. **`admin.txt`** neben `admin.exe` mit dem `ADMIN_TOKEN` → Feld ist vorausgefüllt.
2. **Tage**: `0` = Lifetime, Buttons 30 Tage / 1 Monat / Lifetime.
3. **Anzahl**: 1–100.
4. **KEYS ERSTELLEN** → Keys in der Liste + in der Zwischenablage.

Das Admin-Panel wird **nicht** mit veröffentlicht — es bleibt bei dir.

---

## Was getestet ist

Client ([src/Core/Features/license_client.hpp](src/Core/Features/license_client.hpp)) gegen einen
Mock-Server mit denselben Regeln:

| Fall | Ergebnis |
|---|---|
| Lifetime-Key | `OK\|LIFETIME\|0` |
| Key klein/ohne Striche | `OK` (Normalisierung) |
| 30-Tage-Key | `OK\|2026-11-28\|30` |
| abgelaufen / gesperrt / unbekannt / leer | `Key abgelaufen`, `Key gesperrt`, `Falscher Key`, `Key-Format ungueltig` |
| 50 Keys auf einmal | `OK`, 50 Keys |
| falsches Admin-Token | `Admin-Token falsch` |

Cheat-Build: Exit 0, Gate nachweislich im Binary (`ANMELDEN`, `Zugang nur mit gueltigem Key`
liegen als UTF-16-Strings drin), 7.954.432 Bytes.

**Nicht getestet:** die PHP-Dateien selbst (kein PHP hier), der echte HTTPS-Endpunkt, und der
Klick-Durchlauf im Fenster (die EXE verlangt Adminrechte, UAC). Nach dem Upload `curl` laufen
lassen, dann die EXE starten.

---

## Grenzen

- **`ADMIN_TOKEN` ist der Generalschlüssel** — nur in `admin.txt`, nie ins Repo (ist in `.gitignore`).
- **HTTPS ist Pflicht**, sonst liest jeder die Keys mit.
- **Kein HWID-Binding**: ein Key läuft auf beliebig vielen PCs. Nächster Schritt, wenn du willst.
- Der Key wird **beim Start** geprüft. Wer die EXE offen lässt, behält Zugriff bis zum Schließen.
- Keys, die im Chat standen, gelten als veröffentlicht → neue erzeugen (Panel, Anzahl 50, 0 Tage).

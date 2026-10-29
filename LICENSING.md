# PRUDA — Loader + Admin-Panel + Lizenz-API

Drei Teile arbeiten zusammen:

| Teil | Datei | Aufgabe |
|---|---|---|
| **Loader** | `spoofer_imgui\bin\Menu.exe` | Discord-ID eingeben, Lizenz prüfen, Cheat starten |
| **Admin-Panel** | [admin/bin/admin.exe](admin/bin/admin.exe) | Discord-ID + Dauer → Key wird automatisch erzeugt |
| **API** | [server/](server) | PHP + MySQL, hält die Lizenzen |

---

## Der Ablauf

```
Admin-Panel                      Server (PHP)                    Loader
-----------                      ------------                    ------
ID 1234567890 + 30 Tage   →   INSERT licenses            ←   ID eingeben
Key wird angezeigt        ←   OK|A1B2-C3D4-...|2026-11-28     Server prüft
                                                             Cheat startet
                                                             F1 öffnet Menü
```

- Eine Discord-ID kann **nur einmal** registriert werden. Beim zweiten Versuch
  antwortet der Server `already_registered` → das Panel zeigt
  **„Falsche ID - schon registriert"**.
- Gibt der User im Loader eine ID ein, die es nicht gibt, kommt
  **„Falsche ID - nicht registriert"**.
- Der Key ist optional: der Loader akzeptiert die ID auch ohne Key. Wer will,
  kann den Key im Feld mitgeben — dann muss er zur ID passen.

---

## 1. Server einrichten (einmal)

1. **Datenbank** in phpMyAdmin anlegen, dann den Inhalt von [server/schema.sql](server/schema.sql)
   im SQL-Tab ausführen. Ergebnis: Tabelle `licenses` mit `UNIQUE` auf `discord_id`.
2. [server/config.php](server/config.php) bearbeiten: `DB_HOST`, `DB_NAME`, `DB_USER`,
   `DB_PASS` und `ADMIN_TOKEN` (langer Zufallswert).
3. Die drei Dateien `config.php`, `login.php`, `admin.php` per FTP nach
   `https://fivem.ovh/goonen/license/` hochladen.

Test aus dem Browser oder per curl:

```
curl -d "discord_id=1234567890" https://fivem.ovh/goonen/license/login.php
```

Erwartet: `ERR|wrong_id` (die ID existiert noch nicht) — das heißt: API läuft.

---

## 2. Admin-Panel benutzen

1. Neben `admin.exe` eine Datei **`admin.txt`** legen, Inhalt = dein `ADMIN_TOKEN`.
   Dann ist das Token-Feld vorausgefüllt und du musst es nicht jedes Mal tippen.
   (`admin.txt` ist in `.gitignore` — nie ins Repo.)
2. `admin.exe` starten → Discord-ID eingeben → Tage wählen:
   **30** / **1 Monat** (=30) / **LFT** (=0, Lifetime).
3. **KEY ERSTELLEN** → der Key erscheint und liegt in der Zwischenablage.
   Ablaufdatum steht in der Statuszeile.

Läuft die API woanders, eine **`api.txt`** neben die exe legen:

```
https://fivem.ovh/goonen/license/
```

Gleiche Datei gilt für den Loader.

---

## 3. Loader benutzen

`Menu.exe` (aus `spoofer_imgui\bin\`) und `Wizard External.exe` müssen **im selben Ordner**
liegen. Loader starten → Discord-ID eintippen → Enter. Bei gültiger Lizenz startet der
Cheat automatisch, im Spiel öffnet **F1** das Menü.

Fehlt die Cheat-EXE, zeigt der Loader „Wizard External.exe fehlt neben dem Loader".

---

## Antwort-Format der API

Reiner Text, eine Zeile, `|`-getrennt:

| Endpoint | Antwort |
|---|---|
| `login.php` | `OK\|2026-11-28\|30`, `OK\|LIFETIME\|0` |
| | `ERR\|wrong_id`, `expired`, `banned`, `bad_key`, `bad_id`, `db` |
| `admin.php` | `OK\|A1B2-C3D4-E5F6-7788\|2026-11-28 20:00:00`, `OK\|...\|LIFETIME` |
| | `ERR\|already_registered`, `auth`, `bad_id`, `db` |

---

## Was getestet ist

Der Client-Code in [admin/license_client.hpp](admin/license_client.hpp) wurde gegen einen
Mock-Server mit denselben Regeln geprüft — 11 Fälle, alle korrekt:

| Fall | Ergebnis |
|---|---|
| gültige ID (30 Tage) | `OK\|2026-11-28\|30` |
| unbekannte ID | `Falsche ID - nicht registriert` |
| abgelaufene Lizenz | `Lizenz abgelaufen` |
| gesperrte Lizenz | `Lizenz gesperrt` |
| falscher Key | `Key passt nicht zu dieser ID` |
| ID mit Buchstaben | `Ungueltige Discord-ID (nur Zahlen)` |
| Key erzeugen (30 Tage) | `OK`, Key im Format `XXXX-XXXX-XXXX-XXXX` |
| dieselbe ID nochmal | `Falsche ID - schon registriert` |
| falsches Admin-Token | `Admin-Token falsch` |
| Key erzeugen (0 = LFT) | `OK`, `LIFETIME` |
| danach einloggen | `OK\|LIFETIME\|0` |

**Nicht getestet:** die PHP-Dateien selbst (auf dem Rechner ist kein PHP installiert) und
der echte HTTPS-Endpunkt. Nach dem Upload einmal den curl-Befehl oben ausführen.

---

## Wichtig / Grenzen

- **`ADMIN_TOKEN` ist der Generalschlüssel.** Er liegt in `admin.txt` beim Admin, nicht
  beim User. Wenn er rausgeht, kann jeder Keys erzeugen → Token in `config.php` ändern.
- **HTTPS ist Pflicht.** Über HTTP kann jeder die Discord-IDs mitlesen.
- **Keine HWID-Bindung.** Die Lizenz hängt nur an der Discord-ID, also kann eine Lizenz
  auf beliebig vielen PCs laufen. Wenn du das willst: `hwid` mitschicken, in der DB
  speichern und beim ersten Login binden (sag Bescheid, dann baue ich es ein).
- **Zwei Kopien von `license_client.hpp`** existieren (Loader + Admin-Panel), weil das
  Loader-Projekt ein eigener Ordner ist. Bei Änderungen beide ersetzen:
  `spoofer_imgui\src\license_client.hpp` und `admin\license_client.hpp`.
- Der Loader prüft die Lizenz **beim Start**. Wer den Prozess offen lässt, behält Zugriff
  bis zum Schließen — für eine echte Laufzeitprüfung müsste der Cheat selbst alle
  X Minuten nachfragen.

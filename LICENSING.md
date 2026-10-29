# PRUDA — Loader + Admin-Panel + Lizenz-API

Kein Discord-Login mehr: der User bekommt einen **Key** und gibt nur den ein.

| Teil | Datei | Aufgabe |
|---|---|---|
| **Loader** | `spoofer_imgui\bin\Menu.exe` | Key eingeben, Lizenz prüfen, Cheat starten |
| **Admin-Panel** | [admin/bin/admin.exe](admin/bin/admin.exe) | Keys erzeugen (einzeln oder bis 100 am Stück) |
| **API** | [server/](server) | PHP + MySQL, hält die Keys |

---

## Ablauf

```
Admin-Panel                   Server (PHP)                  Loader
-----------                   ------------                  ------
Tage + Anzahl wählen   →   INSERT licenses           ←   Key eingeben
Keys werden angezeigt  ←   OK|K1,K2,K3|LIFETIME          Server prüft
Keys in Zwischenablage                                    Cheat startet
                                                          F1 öffnet Menü
```

- Der Key wird **ohne** Discord-ID geprüft — er allein ist die Lizenz.
- Groß-/Kleinschreibung und Bindestriche sind egal: `nflgqsp lzl76 svcz`
  funktioniert genauso wie `NFLG-QSPL-ZL76-SVCZ`.
- Der Loader prüft beim Start. Ist der Key gültig, startet der Cheat automatisch.

---

## Server einrichten (einmal)

1. **Datenbank** anlegen, dann [server/schema.sql](server/schema.sql) im SQL-Tab ausführen.
2. [server/config.php](server/config.php): `DB_HOST`, `DB_NAME`, `DB_USER`, `DB_PASS`
   und `ADMIN_TOKEN` (langer Zufallswert) eintragen.
3. `config.php`, `login.php`, `admin.php` nach `https://fivem.ovh/goonen/license/` hochladen.
4. Für die 50 fertigen Lifetime-Keys: [server/keys_lifetime_50.sql](server/keys_lifetime_50.sql)
   im SQL-Tab ausführen (die lesbare Liste liegt in
   [server/keys_lifetime_50.txt](server/keys_lifetime_50.txt)).

Test:
```
curl -d "key=NFLG-QSPL-ZL76-SVCZ" https://fivem.ovh/goonen/license/login.php
```
Erwartet: `OK|LIFETIME|0` — wenn die Keys importiert sind.

---

## Admin-Panel

1. Datei **`admin.txt`** neben `admin.exe` mit dem `ADMIN_TOKEN` → Feld ist vorausgefüllt.
2. **Tage** wählen: `0` = Lifetime, oder die Buttons **30 Tage / 1 Monat / Lifetime**.
3. **Anzahl**: wie viele Keys auf einmal (1–100).
4. **KEYS ERSTELLEN** → Keys erscheinen in der Liste und liegen in der Zwischenablage.

Anderer Server? Datei **`api.txt`** daneben, Inhalt z. B. `https://fivem.ovh/goonen/license/`.
Gleiche Datei gilt für den Loader.

---

## Antwort-Format der API

Reiner Text, eine Zeile, `|`-getrennt:

| Endpoint | Antwort |
|---|---|
| `login.php` (`key`) | `OK\|2026-11-28\|30`, `OK\|LIFETIME\|0` |
| | `ERR\|wrong_key`, `expired`, `banned`, `bad_key`, `db` |
| `admin.php` (`token`, `days`, `count`) | `OK\|A1B2-C3D4-E5F6-7788\|LIFETIME` |
| | `OK\|K1,K2,K3\|2026-11-28 20:00:00` (mehrere) |
| | `ERR\|auth`, `db` |

---

## Was getestet ist

Der Client ([admin/license_client.hpp](admin/license_client.hpp)) lief gegen einen Mock-Server
mit denselben Regeln:

| Fall | Ergebnis |
|---|---|
| Lifetime-Key | `OK\|LIFETIME\|0` |
| gleicher Key in Kleinbuchstaben mit Leerzeichen | `OK` (Normalisierung greift) |
| 30-Tage-Key | `OK\|2026-11-28\|30` |
| abgelaufener Key | `Key abgelaufen` |
| gesperrter Key | `Key gesperrt` |
| unbekannter Key | `Falscher Key` |
| leerer Key | `Key-Format ungueltig` |
| 50 Keys auf einmal erzeugen | `OK`, 50 Keys, `LIFETIME` |
| falsches Admin-Token | `Admin-Token falsch` |

Die 50 ausgelieferten Keys sind zusätzlich geprüft: 50 Stück, 0 Duplikate,
alle im Format `XXXX-XXXX-XXXX-XXXX` ohne `0/O/1/I`.

**Nicht getestet:** die PHP-Dateien selbst (kein PHP auf dem Rechner) und der echte
HTTPS-Endpunkt. Nach dem Upload einmal den curl-Befehl oben laufen lassen.

---

## Wichtig / Grenzen

- **`ADMIN_TOKEN` ist der Generalschlüssel** — nur beim Admin in `admin.txt`, nie ins Repo
  (steht in `.gitignore`). Wenn er rausgeht: in `config.php` ändern.
- **HTTPS ist Pflicht**, sonst liest jeder die Keys mit.
- **Keys sind an keinen PC gebunden** — ein Key läuft auf beliebig vielen Rechnern.
  HWID-Bindung wäre der nächste Schritt (sag Bescheid).
- **Keys, die im Chat stehen, gelten als veröffentlicht.** Wenn du sie irgendwo gepostet
  hast, behandle sie als verbrannt und erzeuge neue (Panel → Anzahl 50 → 0 Tage).
- **Zwei Kopien von `license_client.hpp`** existieren (Loader + Admin-Panel), weil das
  Loader-Projekt ein eigener Ordner ist. Bei Änderungen beide ersetzen.

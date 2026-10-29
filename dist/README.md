# Wizard External — Start per cmd (ohne EXE-Download)

Der User muss nichts herunterladen und nichts anklicken. Er pastet **eine Zeile in cmd**, der Rest läuft automatisch:

```
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/DEINNAME/DEINREPO/main/dist/install.ps1')"
```

Was dabei passiert:

1. `dist/install.ps1` wird aus deinem GitHub-Repo geladen und ausgeführt (liegt nur im RAM, keine Datei beim User).
2. Das Script fragt das **letzte GitHub-Release** ab.
3. Die EXE wird nach `%TEMP%\wz\wizard-<version>.exe` geladen — beim zweiten Start sofort aus dem Cache.
4. Größe + `MZ`-Header werden geprüft. Ist der Build kaputt (z. B. 0 Bytes), bricht das Script mit einer klaren Meldung ab, statt die „Diese App kann auf dem PC nicht ausgeführt werden"-Box zu zeigen.
5. Die App startet mit Adminrechten (UAC-Abfrage) — nötig für die HKLM-Werte von NVIDIA Streamproof.

---

## Einrichtung (einmal, ca. 10 Minuten)

### 1. Repo anlegen
GitHub → **New repository** → Name z. B. `wizard` → **Public** (Pflicht!).

> Ein privates Repo funktioniert nicht: Release-Downloads und `raw.githubusercontent.com`
> brauchen ohne Login Authentifizierung. Der One-Liner würde beim User scheitern.

### 2. Code hochladen
Dieses Projekt ist lokal schon als Git-Repo vorbereitet (Branch `main`, erster Commit steht).
Nur noch das Remote setzen und pushen:

```
cd "C:\Users\z3laa\Downloads\Wizard External"
git remote add origin https://github.com/DEINNAME/DEINREPO.git
git push -u origin main
```

Was **nicht** hochgeht (siehe [.gitignore](../:.gitignore)): `x64/`, `src/x64/`,
`src/Wizard External/`, `.vs/`, `packages/`. Das sind ~1,1 GB Build-Müll — der Rest
sind ~172 MB Quellcode, davon 37 MB die eingebetteten Bilder in `slate/resources/images.h`.

### 3. `$Repo` im Launcher eintragen
In [install.ps1](install.ps1) oben:

```powershell
$Repo      = 'DEINNAME/DEINREPO'     # z. B. 'z3laa/wizard'
$AssetName = 'WizardExternal.exe'    # Dateiname im Release
```

Committen und pushen. Fertig — **bauen musst du ab jetzt nichts mehr selbst.**

### 4. Release erzeugen
Der Workflow [.github/workflows/build.yml](../.github/workflows/build.yml) baut die EXE bei jedem
Push automatisch und hängt sie an ein Release, sobald ein Tag gepusht wird:

```
git tag v1.0.0
git push origin v1.0.0
```

~5 Minuten später steht das Release mit `WizardExternal.exe` online. Der Launcher findet es
über die GitHub-API und liefert es aus.

**Update = neuer Tag:**

```
git tag v1.0.1
git push origin v1.0.1
```

Der Launcher erkennt den neuen Tag, lädt neu und räumt die alte Version im Cache weg.
Der User macht nichts.

### 5. Befehl verteilen
Den fertigen One-Liner (oben) posten. Alternativ: [Wizard.cmd](Wizard.cmd) mitschicken —
da ist derselbe Befehl drin, User doppelklickt nur. In der Datei die URL ebenfalls ersetzen.

---

## Was der Build-Job macht

| Schritt | Zweck |
|---|---|
| `actions/checkout` | Quellcode |
| `microsoft/setup-msbuild` | MSBuild aus VS 2026 |
| `msbuild -t:Rebuild` | vollständiger Rebuild, Release \| x64 (`v145`, SDK 10.0.26100) |
| EXE-Prüfung | mind. 500 KB + `MZ`-Header, sonst bricht der Job ab |
| Artifact | `WizardExternal.exe`, 30 Tage abrufbar |
| Release | nur bei Tags `v*`, via `softprops/action-gh-release` |

Der Runner ist `windows-2025` = Windows Server 2025 + **Visual Studio 2026** (`v145`) +
**Windows SDK 10.0.26100.0** — exakt der Toolset, den das Projekt verlangt.

Die Prüfung ist der eigentliche Gewinn: der 0-Byte-Link, der dir ständig passiert ist,
kann so nie mehr beim User landen. Der Job wird rot und du siehst es in der Actions-Übersicht.

---

## Wenn du kein GitHub willst

Der Launcher ist host-agnostisch — es muss nur eine **direkte HTTPS-URL** sein.
Dann in `install.ps1` den Block „Release abfragen" ersetzen:

```powershell
$DirectUrl = 'https://dein-server.de/wizard.exe'
$tag       = 'v1'
$asset     = [pscustomobject]@{ browser_download_url = $DirectUrl; size = 0 }
```

Alles andere (Cache, Prüfung, Start mit Adminrechten) bleibt gleich.

---

## Grenzen / was du wissen musst

- **Ein Download ist technisch unvermeidbar.** „Ohne EXE herunterladen" heißt hier:
  der User sieht keinen Download-Dialog und keine Datei im Downloads-Ordner — die Bytes
  müssen aber aus dem Netz kommen. Echtes Ausführen rein aus dem RAM ohne Datei wäre
  möglich (Reflective PE Loader), wird aber von Defender/AMSI praktisch immer geblockt
  und ist deutlich fehleranfälliger.
- **GitHub kann das Repo sperren**, wenn es als Cheat gemeldet wird. Ein eigener
  Server oder Cloudflare R2 als Host ist robuster.
- **SmartScreen**: die EXE ist nicht signiert, beim ersten Start kann „Weitere Informationen
  → Trotzdem ausführen" nötig sein. Ein Code-Signing-Zertifikat würde das entfernen.
- **Actions-Minuten**: jeder Build braucht ~4–6 Minuten Public-Runner-Zeit (bei Public Repos gratis).
- Der Cache liegt in `%TEMP%\wz\` — passt zum Destruct-Verhalten: nach einem Neustart ist
  die alte Version weg, der nächste Start lädt frisch.

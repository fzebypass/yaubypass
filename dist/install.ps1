# ============================================================
#  Wizard External - One-Liner Launcher
#  Aufruf beim User (cmd):
#    powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/fzebypass/yaubypass/main/dist/install.ps1')"
# ============================================================

$ErrorActionPreference = 'Stop'

# ---------------- HIER ANPASSEN ----------------
$Repo      = 'fzebypass/yaubypass'          # GitHub  user/repo  (MUSS public sein)
$AssetName = 'WizardExternal.exe'         # Dateiname des Assets im Release
$MinSize   = 500KB                        # kleinere Dateien gelten als kaputter Build
# ----------------------------------------------

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$cacheDir = Join-Path $env:TEMP 'wz'
New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null

function Write-Step([string]$text) {
    Write-Host '  [*] ' -NoNewline -ForegroundColor DarkGray
    Write-Host $text -ForegroundColor Gray
}
function Fail([string]$text) {
    Write-Host "`n  [!] $text" -ForegroundColor Red
    Write-Host '      Fenster schliesst in 8 Sekunden ...' -ForegroundColor DarkGray
    Start-Sleep 8
    exit 1
}

Write-Host ''
Write-Host '  Wizard External' -ForegroundColor White
Write-Host '  ---------------------------------------------' -ForegroundColor DarkGray

# ---------- Release abfragen ----------
Write-Step 'Release wird geprueft ...'

$wc = New-Object Net.WebClient
$wc.Headers.Add('User-Agent', 'wizard-launcher')

try {
    $releaseJson = $wc.DownloadString("https://api.github.com/repos/$Repo/releases/latest")
} catch {
    Fail "Release konnte nicht geladen werden ($Repo). Repo public? Internet da?"
}

$release = $releaseJson | ConvertFrom-Json
$tag     = $release.tag_name
if (-not $tag) { $tag = 'latest' }

$asset = $release.assets | Where-Object { $_.name -eq $AssetName } | Select-Object -First 1
if (-not $asset) {
    Fail "Im Release $tag fehlt das Asset '$AssetName'."
}

# ---------- Cache ----------
$target = Join-Path $cacheDir ("wizard-$tag.exe")
$needDownload = $true

if (Test-Path $target) {
    $existing = Get-Item $target
    if ($existing.Length -ge $MinSize) {
        $needDownload = $false
        Write-Step "Version $tag bereits vorhanden (Cache)"
    }
}

# ---------- Download ----------
if ($needDownload) {
    Write-Step "Version $tag wird geladen ($([math]::Round($asset.size / 1MB, 2)) MB) ..."
    $tmp = "$target.part"

    try {
        $wc.DownloadFile($asset.browser_download_url, $tmp)
    } catch {
        Fail 'Download fehlgeschlagen.'
    }

    if (-not (Test-Path $tmp)) { Fail 'Download lieferte keine Datei.' }

    $size = (Get-Item $tmp).Length
    if ($size -lt $MinSize) {
        Remove-Item $tmp -Force -ErrorAction SilentlyContinue
        Fail "Datei ist nur $size Bytes gross - kaputter Build. Upload neu machen."
    }

    Move-Item -Force $tmp $target
}

# ---------- Integritaet ----------
$info = Get-Item $target
if ($info.Length -lt $MinSize) {
    Remove-Item $target -Force -ErrorAction SilentlyContinue
    Fail 'Datei ist zu klein - kein gueltiges Programm.'
}

$fs = [IO.File]::OpenRead($target)
$sig = New-Object byte[] 2
$null = $fs.Read($sig, 0, 2)
$fs.Close()

if ($sig[0] -ne 0x4D -or $sig[1] -ne 0x5A) {   # 'MZ'
    Remove-Item $target -Force -ErrorAction SilentlyContinue
    Fail 'Datei ist kein gueltiges Windows-Programm (kein MZ-Header).'
}

Write-Step ("{0:N0} Bytes - OK" -f $info.Length)

# ---------- Alte Versionen aufraeumen ----------
Get-ChildItem $cacheDir -Filter 'wizard-*.exe' |
    Where-Object { $_.FullName -ne $target } |
    Remove-Item -Force -ErrorAction SilentlyContinue

# ---------- Starten ----------
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Step 'Startet ...'
try {
    if ($isAdmin) {
        Start-Process -FilePath $target -WorkingDirectory $cacheDir
    } else {
        # Die App schreibt HKLM-Werte (NVIDIA Streamproof) - braucht Adminrechte.
        Start-Process -FilePath $target -WorkingDirectory $cacheDir -Verb RunAs
    }
} catch {
    Fail 'Start fehlgeschlagen (UAC abgelehnt?).'
}

Write-Host '  ---------------------------------------------' -ForegroundColor DarkGray
Write-Host '  Fertig. Taste F1 oeffnet das Menue.' -ForegroundColor DarkGray
Write-Host ''

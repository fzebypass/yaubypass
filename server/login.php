<?php
// ---------------------------------------------------------------------------
//  POST login.php
//      key = A1B2-C3D4-E5F6-7788   (Bindestriche und Kleinschreibung egal)
//
//  Antwort (eine Zeile):
//      OK|2026-11-28|30    Key gueltig, 30 Tage uebrig
//      OK|LIFETIME|0       Lifetime
//      ERR|wrong_key       Key existiert nicht
//      ERR|expired         abgelaufen
//      ERR|banned          gesperrt
//      ERR|bad_key         leer / zu lang
//      ERR|db              Datenbankfehler
// ---------------------------------------------------------------------------

require __DIR__ . '/config.php';

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    fail('bad_request');
}

$raw = trim($_POST['key'] ?? '');

// Nur A-Z und 0-9 behalten, alles andere (Bindestriche, Leerzeichen) weg.
$key = strtoupper(preg_replace('/[^A-Za-z0-9]/', '', $raw));

if ($key === '' || strlen($key) > 32) {
    fail('bad_key');
}

try {
    $stmt = db()->prepare('SELECT id, expires_at, active FROM licenses WHERE key_plain = ? LIMIT 1');
    $stmt->execute([$key]);
    $row = $stmt->fetch();
} catch (Throwable $e) {
    fail('db');
}

if (!$row) {
    fail('wrong_key');
}

if ((int)$row['active'] !== 1) {
    fail('banned');
}

$touch = function () use ($row) {
    try {
        db()->prepare('UPDATE licenses SET last_login = NOW() WHERE id = ?')->execute([$row['id']]);
    } catch (Throwable $e) { /* nicht kritisch */ }
};

if ($row['expires_at'] === null) {
    $touch();
    respond('OK|LIFETIME|0');
}

$ts = strtotime((string)$row['expires_at']);
if ($ts === false || $ts <= time()) {
    fail('expired');
}

$touch();
respond('OK|' . date('Y-m-d', $ts) . '|' . (int)floor(($ts - time()) / 86400));

<?php
// ---------------------------------------------------------------------------
//  POST login.php
//      discord_id = 123456789012345678
//      key        = optional, muss passen wenn gesetzt
//
//  Antwort (eine Zeile):
//      OK|2026-11-28|30        Lizenz gueltig, 30 Tage uebrig
//      OK|LIFETIME|0           Lifetime
//      ERR|wrong_id            ID nicht registriert
//      ERR|expired             abgelaufen
//      ERR|banned              gesperrt
//      ERR|bad_key             Key passt nicht
//      ERR|bad_id              keine gueltige Discord-ID
// ---------------------------------------------------------------------------

require __DIR__ . '/config.php';

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    fail('bad_request');
}

$discord_id = trim($_POST['discord_id'] ?? '');
$key        = trim($_POST['key'] ?? '');

if ($discord_id === '' || !ctype_digit($discord_id) || strlen($discord_id) > 32) {
    fail('bad_id');
}

try {
    $stmt = db()->prepare('SELECT discord_id, license_key, expires_at, active FROM licenses WHERE discord_id = ? LIMIT 1');
    $stmt->execute([$discord_id]);
    $row = $stmt->fetch();
} catch (Throwable $e) {
    fail('db');
}

if (!$row) {
    fail('wrong_id');
}

if ((int)$row['active'] !== 1) {
    fail('banned');
}

if ($key !== '' && !hash_equals((string)$row['license_key'], $key)) {
    fail('bad_key');
}

if ($row['expires_at'] === null) {
    // Lifetime
    try {
        db()->prepare('UPDATE licenses SET last_login = NOW() WHERE discord_id = ?')->execute([$discord_id]);
    } catch (Throwable $e) { /* nicht kritisch */ }

    respond('OK|LIFETIME|0');
}

$ts = strtotime((string)$row['expires_at']);
if ($ts === false || $ts <= time()) {
    fail('expired');
}

$days = (int)floor(($ts - time()) / 86400);

try {
    db()->prepare('UPDATE licenses SET last_login = NOW() WHERE discord_id = ?')->execute([$discord_id]);
} catch (Throwable $e) { /* nicht kritisch */ }

respond('OK|' . date('Y-m-d', $ts) . '|' . $days);

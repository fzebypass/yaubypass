<?php
// ---------------------------------------------------------------------------
//  POST admin.php
//      token      = ADMIN_TOKEN aus config.php
//      discord_id = 123456789012345678
//      days       = 30 | 0 (0 = Lifetime)
//
//  Antwort (eine Zeile):
//      OK|A1B2-C3D4-E5F6-7788|2026-11-28 20:00:00
//      OK|A1B2-C3D4-E5F6-7788|LIFETIME
//      ERR|already_registered  ID hat schon eine Lizenz (falsche ID)
//      ERR|auth                Token falsch
//      ERR|bad_id              keine gueltige Discord-ID
// ---------------------------------------------------------------------------

require __DIR__ . '/config.php';

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    fail('bad_request');
}

$token      = (string)($_POST['token'] ?? '');
$discord_id = trim($_POST['discord_id'] ?? '');
$days       = (int)($_POST['days'] ?? 30);

if (!hash_equals(ADMIN_TOKEN, $token)) {
    fail('auth');
}

if ($discord_id === '' || !ctype_digit($discord_id) || strlen($discord_id) > 32) {
    fail('bad_id');
}

if ($days < 0) {
    $days = 0;
}

// ---- is the ID already taken? ---------------------------------------------
try {
    $check = db()->prepare('SELECT 1 FROM licenses WHERE discord_id = ? LIMIT 1');
    $check->execute([$discord_id]);
    if ($check->fetch()) {
        fail('already_registered');
    }
} catch (Throwable $e) {
    fail('db');
}

// ---- build the key ---------------------------------------------------------
$alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';   // ohne 0/O/1/I
$raw = '';
for ($i = 0; $i < 16; $i++) {
    $raw .= $alphabet[random_int(0, strlen($alphabet) - 1)];
}
$license_key = implode('-', str_split($raw, 4));

$expires_at = $days > 0
    ? date('Y-m-d H:i:s', strtotime('+' . $days . ' days'))
    : null;

try {
    $insert = db()->prepare('INSERT INTO licenses (discord_id, license_key, expires_at) VALUES (?, ?, ?)');
    $insert->execute([$discord_id, $license_key, $expires_at]);
} catch (PDOException $e) {
    // 23000 = duplicate key - the ID got registered in between
    if ($e->getCode() === '23000') {
        fail('already_registered');
    }
    fail('db');
} catch (Throwable $e) {
    fail('db');
}

respond('OK|' . $license_key . '|' . ($expires_at === null ? 'LIFETIME' : $expires_at));

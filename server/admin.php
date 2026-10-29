<?php
// ---------------------------------------------------------------------------
//  POST admin.php
//      token = ADMIN_TOKEN aus config.php
//      days  = 30 | 0 (0 = Lifetime)
//      count = 1..100   (wie viele Keys auf einmal)
//
//  Antwort (eine Zeile):
//      OK|A1B2-C3D4-E5F6-7788|LIFETIME
//      OK|K1,K2,K3|2026-11-28 20:00:00      (count > 1, Keys mit Komma getrennt)
//      ERR|auth        Token falsch
//      ERR|db          Datenbankfehler
// ---------------------------------------------------------------------------

require __DIR__ . '/config.php';

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    fail('bad_request');
}

$token = (string)($_POST['token'] ?? '');
$days  = (int)($_POST['days'] ?? 0);
$count = (int)($_POST['count'] ?? 1);

if (!hash_equals(ADMIN_TOKEN, $token)) {
    fail('auth');
}

if ($days < 0)   $days = 0;
if ($count < 1)  $count = 1;
if ($count > 100) $count = 100;

$expires_at = $days > 0 ? date('Y-m-d H:i:s', strtotime('+' . $days . ' days')) : null;

// ohne 0/O/1/I - laesst sich am Telefon vorlesen
$alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
$insert = db()->prepare('INSERT INTO licenses (license_key, key_plain, expires_at) VALUES (?, ?, ?)');

$created = [];

for ($n = 0; $n < $count; $n++) {
    $done = false;

    for ($attempt = 0; $attempt < 10 && !$done; $attempt++) {
        $plain = '';
        for ($i = 0; $i < 16; $i++) {
            $plain .= $alphabet[random_int(0, strlen($alphabet) - 1)];
        }
        $formatted = implode('-', str_split($plain, 4));

        try {
            $insert->execute([$formatted, $plain, $expires_at]);
            $created[] = $formatted;
            $done = true;
        } catch (PDOException $e) {
            if ($e->getCode() !== '23000') {
                if ($created === []) fail('db');
                break 2;
            }
            // Schluesselkollision - neuer Versuch
        } catch (Throwable $e) {
            if ($created === []) fail('db');
            break 2;
        }
    }
}

if ($created === []) {
    fail('db');
}

respond('OK|' . implode(',', $created) . '|' . ($expires_at === null ? 'LIFETIME' : $expires_at));

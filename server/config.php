<?php
// ---------------------------------------------------------------------------
//  PRUDA licence API - configuration
//  Fill in your database and pick a long random ADMIN_TOKEN.
// ---------------------------------------------------------------------------

define('DB_HOST', 'localhost');
define('DB_NAME', 'pruuda');
define('DB_USER', 'pruuda');
define('DB_PASS', 'CHANGE_ME');

// Long random string. Goes into admin.txt next to admin.exe - never ship it.
define('ADMIN_TOKEN', 'CHANGE_ME_TO_A_LONG_RANDOM_STRING');

function db(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO(
            'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=utf8mb4',
            DB_USER,
            DB_PASS,
            [
                PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES   => false,
            ]
        );
    }
    return $pdo;
}

function respond(string $line): void {
    header('Content-Type: text/plain; charset=utf-8');
    header('Cache-Control: no-store');
    echo $line;
    exit;
}

function fail(string $code): void {
    respond('ERR|' . $code);
}

-- ---------------------------------------------------------------------------
--  PRUDA licences (key based)
--  phpMyAdmin: SQL tab -> paste -> execute
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `licenses` (
  `id`          INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `license_key` VARCHAR(32)  NOT NULL,                 -- angezeigt: A1B2-C3D4-E5F6-7788
  `key_plain`   VARCHAR(32)  NOT NULL,                 -- ohne Bindestriche, fuer den Vergleich
  `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `expires_at`  DATETIME     NULL DEFAULT NULL,        -- NULL = Lifetime
  `active`      TINYINT(1)   NOT NULL DEFAULT 1,       -- 0 = gesperrt
  `last_login`  DATETIME     NULL DEFAULT NULL,
  `note`        VARCHAR(255) NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_key` (`key_plain`),
  KEY `idx_expires` (`expires_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

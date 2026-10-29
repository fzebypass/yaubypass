-- ---------------------------------------------------------------------------
--  PRUDA licences
--  phpMyAdmin: SQL tab -> paste -> execute
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `licenses` (
  `id`          INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `discord_id`  VARCHAR(32)  NOT NULL,
  `license_key` VARCHAR(32)  NOT NULL,
  `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `expires_at`  DATETIME     NULL DEFAULT NULL,       -- NULL = Lifetime
  `active`      TINYINT(1)   NOT NULL DEFAULT 1,      -- 0 = gesperrt
  `last_login`  DATETIME     NULL DEFAULT NULL,
  `note`        VARCHAR(255) NULL DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uniq_discord` (`discord_id`),           -- eine ID = eine Lizenz
  UNIQUE KEY `uniq_key` (`license_key`),
  KEY `idx_expires` (`expires_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

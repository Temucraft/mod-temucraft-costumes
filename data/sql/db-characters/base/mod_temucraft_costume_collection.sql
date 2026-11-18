-- mod-temucraft-costumes: costumes each account has collected.
CREATE TABLE IF NOT EXISTS `mod_temucraft_costume_collection` (
  `account` INT UNSIGNED NOT NULL,
  `display` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`account`, `display`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

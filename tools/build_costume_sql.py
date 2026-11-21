#!/usr/bin/env python3
# build_costume_sql.py anims.json out.sql: the displays whose model has every player animation (animcheck.py output),
# without invisible helper models and world doodads.
# Small output: each model path is written once (a numbered list in a temporary table), the displays point at its number.
import json, sys
a = json.load(open(sys.argv[1]))
rows = []
for d, v in sorted(a.items(), key=lambda x: int(x[0])):
    m = v['model'].upper()
    if v['missing'] or 'INVISIBLE' in m or m.startswith('WORLD\\') or 'STALKER' in m:
        continue
    rows.append((int(d), v['model'].replace('\\', '\\\\').replace("'", "''")))
models = {}
for d, m in rows:
    models.setdefault(m, len(models) + 1)
with open(sys.argv[2], 'w') as f:
    f.write("-- mod-temucraft-costumes: creature displays that can be worn (every walk/run/strafe/jump/swim/combat/cast/\n"
            "-- sit/mount/loot/sleep/kneel animation present). Made by tools/build_costume_sql.py from tools/animcheck.py.\n"
            "DROP TABLE IF EXISTS `mod_temucraft_costume_display`;\n"
            "CREATE TABLE `mod_temucraft_costume_display` (\n  `display` INT UNSIGNED NOT NULL,\n  `model` VARCHAR(255) NOT NULL DEFAULT '',\n"
            "  PRIMARY KEY (`display`)\n) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;\n"
            "-- the model paths, numbered; then (display, model number)\n"
            "DROP TEMPORARY TABLE IF EXISTS `tmp_costume_model`;\n"
            "CREATE TEMPORARY TABLE `tmp_costume_model` (`id` SMALLINT UNSIGNED NOT NULL PRIMARY KEY, `model` VARCHAR(255) NOT NULL);\n"
            "INSERT INTO `tmp_costume_model` (`id`, `model`) VALUES\n")
    f.write(",\n".join(f"({i},'{m}')" for m, i in models.items()) + ";\n")
    f.write("DROP TEMPORARY TABLE IF EXISTS `tmp_costume`;\n"
            "CREATE TEMPORARY TABLE `tmp_costume` (`display` INT UNSIGNED NOT NULL PRIMARY KEY, `m` SMALLINT UNSIGNED NOT NULL);\n")
    for i in range(0, len(rows), 5000):
        f.write("INSERT INTO `tmp_costume` (`display`, `m`) VALUES\n")
        f.write(",\n".join(",".join(f"({d},{models[m]})" for d, m in rows[j:j + 20])
                           for j in range(i, min(i + 5000, len(rows)), 20)) + ";\n")
    f.write("INSERT INTO `mod_temucraft_costume_display` (`display`, `model`)\n"
            "SELECT c.`display`, m.`model` FROM `tmp_costume` c JOIN `tmp_costume_model` m ON m.`id` = c.`m`;\n"
            "DROP TEMPORARY TABLE `tmp_costume`;\nDROP TEMPORARY TABLE `tmp_costume_model`;\n")
print(len(rows), "displays,", len(models), "models")

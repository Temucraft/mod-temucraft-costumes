#!/usr/bin/env python3
# build_costume_sql.py anims.json out.sql: the displays whose model has every player animation (animcheck.py output),
# without invisible helper models and world doodads.
import json, sys
a = json.load(open(sys.argv[1]))
rows = []
for d, v in sorted(a.items(), key=lambda x: int(x[0])):
    m = v['model'].upper()
    if v['missing'] or 'INVISIBLE' in m or m.startswith('WORLD\\') or 'STALKER' in m:
        continue
    rows.append((int(d), v['model'].replace('\\', '\\\\').replace("'", "''")))
with open(sys.argv[2], 'w') as f:
    f.write("-- mod-temucraft-costumes: creature displays that can be worn (every walk/run/strafe/jump/swim/combat/cast/\n"
            "-- sit/mount/loot/sleep/kneel animation present). Made by tools/build_costume_sql.py from tools/animcheck.py.\n"
            "DROP TABLE IF EXISTS `mod_temucraft_costume_display`;\n"
            "CREATE TABLE `mod_temucraft_costume_display` (\n  `display` INT UNSIGNED NOT NULL,\n  `model` VARCHAR(255) NOT NULL DEFAULT '',\n"
            "  PRIMARY KEY (`display`)\n) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;\n")
    for i in range(0, len(rows), 500):
        f.write("INSERT INTO `mod_temucraft_costume_display` (`display`, `model`) VALUES\n")
        f.write(",\n".join(f"({d}, '{m}')" for d, m in rows[i:i+500]) + ";\n")
print(len(rows), "displays")

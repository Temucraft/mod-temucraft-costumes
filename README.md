# mod-temucraft-costumes

Wear the look of a creature of the world. Only creatures whose model has **every animation a player body uses**
can be worn: walk, run, backwards, strafe, sprint, jump (start, air, land, land running), fall, swim (idle,
forward, left, right, back), unarmed/1H/2H attacks, ready, wound, stun, death, spell ready/cast (directed and
omni), channel, sit, mount, loot, sleep and kneel. So a costume never slides, freezes or T-poses.

- `tools/animcheck.py` reads the client's MPQs (CreatureDisplayInfo, CreatureModelData and each model's M2
  animation list) and writes which displays have every animation. `tools/build_costume_sql.py` turns that into
  `mod_temucraft_costume_display` (invisible helper models and world doodads left out).
- Costumes are named after the creature that uses the display and grouped by creature type.
- Collected per account: defeating a creature or talking to it unlocks its look, or everything is open
  (`Temucraft.Costumes.Unlock`).
- `.costume` (or `/wardrobe`) opens the **wardrobe window** of the Temucraft interface (patch-enUS-T,
  [`TemucraftCostumes.lua`](https://github.com/Temucraft/mod-temucraft-client-version/blob/master/client/Interface/FrameXML/Temucraft/TemucraftCostumes.lua), addon prefix TCO): categories by creature type, a search box, a turnable 3D preview of the
  creature, Wear (or double-click) and Take off. It lists what you've collected and updates when you unlock a new one.
  Without the interface patch, `.costume` shows the same wardrobe as a gossip menu; `.costume menu` always does.
  `.costume off` takes it off.
- The window's catalog (names and types) is built into the interface: after changing the world data, run
  `client/costumes/costume_catalog.sql` ([mod-temucraft-client-version](https://github.com/Temucraft/mod-temucraft-client-version)) into `costumes.tsv`, then
  `build_costume_window.py`, and rebuild the patch.
- The look stays through shapeshifts and mounting; it ends on logout or (by default) in battlegrounds and arenas.

GM commands (RBAC): `.costume unlock <display>` (101601) on the selected player, `.costume reload` (101602).

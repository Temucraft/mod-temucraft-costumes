-- mod-temucraft-costumes: its commands (all under .temucraft) as RBAC permissions 101650-101699. Every command is admin only by
-- default: linked to the administrator roles 192 and 1014. To give one to players, link it to 195 and 1010 here (or
-- grant it per account with .rbac account grant <account> <id>). "Temucraft: use ..." permissions are not
-- commands: they stay linked to the player roles so the items keep working.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 101650 AND 101699;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 101650 AND 101699;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(101650, 'Command: temucraft costume'),
(101651, 'Command: temucraft costume off'),
(101652, 'Command: temucraft costume menu');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 101650),
(1014, 101650),
(192, 101651),
(1014, 101651),
(192, 101652),
(1014, 101652);

-- mod-temucraft-costumes: its player commands (all under .temucraft) as RBAC permissions 101650-101699. Linked to the stock player
-- role 195 and the Temucraft player role 1010, so every account has them by default. To take one away, remove its
-- link here (or deny it per account with .rbac account deny <account> <id>); to give it to staff only, link it to
-- 1011-1014 instead.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 101650 AND 101699;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 101650 AND 101699;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(101650, 'Command: temucraft costume'),
(101651, 'Command: temucraft costume off'),
(101652, 'Command: temucraft costume menu');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 101650),
(1010, 101650),
(195, 101651),
(1010, 101651),
(195, 101652),
(1010, 101652);

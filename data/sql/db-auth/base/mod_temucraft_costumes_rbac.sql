-- mod-temucraft-costumes: its game master commands as RBAC permissions (101601-101602). Linked to the stock
-- game master / administrator roles and the Temucraft Entertainer / Administrator tiers. Grant or deny them per
-- account with .rbac account grant / deny, or link them to other roles.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 101601 AND 101649;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 101601 AND 101649;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(101601, 'Command: temucraft costume unlock'),
(101602, 'Command: temucraft costume reload');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192, 101601),
(1014, 101601),
(192, 101602),
(1014, 101602);

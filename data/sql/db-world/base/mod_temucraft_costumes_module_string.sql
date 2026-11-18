-- mod-temucraft-costumes: player-facing texts. Translations go in module_string_locale (same module and id, plus the locale).
DELETE FROM `module_string` WHERE `module` = 'mod-temucraft-costumes';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-temucraft-costumes', 1, 'Costumes are turned off.'),
('mod-temucraft-costumes', 2, 'Wardrobe'),
('mod-temucraft-costumes', 3, '{} ({})'),
('mod-temucraft-costumes', 4, 'Take the costume off.'),
('mod-temucraft-costumes', 5, 'Search by name...'),
('mod-temucraft-costumes', 6, 'Type part of a creature''s name.'),
('mod-temucraft-costumes', 7, 'Next page'),
('mod-temucraft-costumes', 8, 'Previous page'),
('mod-temucraft-costumes', 9, 'Back'),
('mod-temucraft-costumes', 10, 'You now look like {}.'),
('mod-temucraft-costumes', 11, 'You take the costume off.'),
('mod-temucraft-costumes', 12, '|cff66ccffNew costume:|r {} (.costume).'),
('mod-temucraft-costumes', 13, 'No costumes yet: defeat creatures or talk to people to collect their looks.'),
('mod-temucraft-costumes', 14, 'Costumes can''t be worn in battlegrounds and arenas.'),
('mod-temucraft-costumes', 15, 'No costume of yours matches that.'),
('mod-temucraft-costumes', 16, 'You aren''t wearing a costume.'),
('mod-temucraft-costumes', 17, 'Unlocked {} for {}.'),
('mod-temucraft-costumes', 18, 'That display isn''t a costume.');

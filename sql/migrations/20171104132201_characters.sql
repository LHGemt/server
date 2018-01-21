DROP PROCEDURE IF EXISTS add_migration;
delimiter ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20171104132201');
IF v=0 THEN
INSERT INTO `migrations` VALUES ('20171104132201');
-- Add your query below.


--
-- Table structure for table loot logging system
--

DROP TABLE IF EXISTS `loot_creature_death`;
CREATE TABLE `loot_creature_death` (
  `key` bigint(20) unsigned NOT NULL,
  `creatureGuid` bigint unsigned NOT NULL,
  `creatureEntry` int(10) unsigned NOT NULL,
  `timestamp` bigint(20) NOT NULL,
  `instanceId` int(10) unsigned NOT NULL,
  PRIMARY KEY (`key`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;


DROP TABLE IF EXISTS `loot_items`;
CREATE TABLE `loot_items` (
  `key` bigint(20) unsigned NOT NULL,
  `itemEntry` int(10) unsigned NOT NULL,
  `looterGuid` int(11) unsigned default NULL,
  PRIMARY KEY (`key`, `itemEntry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

DROP TABLE IF EXISTS `loot_candidates`;
CREATE TABLE `loot_candidates` (
  `key` bigint(20) unsigned NOT NULL,
  `playerGuid` int(11) unsigned NOT NULL,
  PRIMARY KEY (`key`, `playerGuid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

-- End of migration.
END IF;
END??
delimiter ; 
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;



DROP PROCEDURE IF EXISTS add_migration;
delimiter ??
CREATE PROCEDURE `add_migration`()
BEGIN
DECLARE v INT DEFAULT 1;
SET v = (SELECT COUNT(*) FROM `migrations` WHERE `id`='20171026211901');
IF v=0 THEN
INSERT INTO `migrations` VALUES ('20171026211901');
-- Add your query below.

-- -------------------------------
-- 
-- Issue 151
-- 
-- AQ gate visibility distance increase
--
-- -------------------------------

update gameobject set visibilitymod=350, spawnFlags=1 where id in(176146, 176147); -- gate and roots
update gameobject set visibilitymod=315, spawnFlags=1 where id = 176148; -- the runes

-- End of migration.
END IF;
END??
delimiter ; 
CALL add_migration();
DROP PROCEDURE IF EXISTS add_migration;

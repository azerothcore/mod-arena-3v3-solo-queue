SET @entry             := 1000003;
SET @CreatureDisplayID := 3280;
SET @name              := 'Arena 3v3 Solo';

DELETE FROM `creature_template` WHERE `entry` = @entry;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@entry, 0, 0, 0, 0, 0, @name, '', '', 0, 80, 80, 2, 35, 1, 1, 1.14286, 1, 1, 20, 1, 1, 0, 1, 1, 1, 1, 1, 1, 131078, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 138936390, 0, 0, 'npc_solo3v3', '12340');

DELETE FROM `creature_template_model` WHERE `CreatureID` = @entry;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(@entry, 0, @CreatureDisplayID, 1, 1, 0);

-- npc text
SET @npc_text_3v3_soloq := "This NPC enables you to join 3v3soloQ unrated arenas, create a 3v3soloQ arena team, and join 3v3soloQ rated arenas.$B$BIf you prefer not to interact with the NPC, you can use the following commands to join or create the arena:$B$B.qsolo rated$B$B.qsolo unrated$B$BNote that if you don’t already have a 3v3soloQ arena team, using .qsolo rated will automatically create one for you.";
DELETE FROM `npc_text` WHERE `ID` = 1000004;
INSERT INTO `npc_text` (`id`, `text0_0`, `text0_1`, `Probability0`) VALUES
(1000004, @npc_text_3v3_soloq, @npc_text_3v3_soloq, 1);

-- Command
DELETE FROM `command` WHERE `name` IN ('qsolo', 'qsolo rated', 'qsolo unrated', 'testqsolo');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('qsolo', 0, '.qsolo rated/unrated\nJoin arena 3v3soloQ rated or unrated'),
('qsolo rated', 0, 'Syntax .qsolo rated\nJoin arena 3v3soloQ rated'),
('qsolo unrated', 0, 'Syntax .qsolo unrated\nJoin arena 3v3soloQ unrated'),
('testqsolo', 4, '.testqsolo -> join arena 3v3soloQ for testing');

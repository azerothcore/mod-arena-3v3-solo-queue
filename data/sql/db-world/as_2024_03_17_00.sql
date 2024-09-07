SET @entry             := 1000003;
SET @CreatureDisplayID := 3280;
SET @name              := 'Arena 3v3 Solo';

DELETE FROM `creature_template` WHERE `entry` = @entry;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(@entry, 0, 0, 0, 0, 0, @name, '', '', 0, 80, 80, 2, 35, 1, 1, 1.14286, 1, 1, 20, 1, 1, 0, 1, 1, 1, 1, 1, 1, 131078, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 138936390, 0, 0, 'npc_solo3v3', '12340');


DELETE FROM `creature_template_model` WHERE `CreatureID` = @entry;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(@entry, 0, @CreatureDisplayID, 1, 1, 0);

-- npc text (MCH on):
SET @entryNpcText1 := 1000003;
SET @text1 := '                                |TInterface/ICONS/achievement_bg_killXEnemies_generalsroom:25|t$B                        Solo 3v3 Arena$B$B              Melee Caster Healer [|cff00ff00on|r]';
DELETE FROM `npc_text` WHERE `ID` = @entryNpcText;
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES (@entryNpcText1, @text1);

-- npc text (MCH: off):
SET @entryNpcText2 := 1000004;
SET @text2 := '                                |TInterface/ICONS/achievement_bg_killXEnemies_generalsroom:25|t$B                        Solo 3v3 Arena$B$B              Melee Caster Healer [|cffff0000off|r]';
DELETE FROM `npc_text` WHERE `ID` = @entryNpcText;
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES (@entryNpcText2, @text2);
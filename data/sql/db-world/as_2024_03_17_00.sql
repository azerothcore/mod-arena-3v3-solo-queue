DELETE FROM `creature_template` WHERE `entry`=1000003;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `scale`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `mechanic_immune_mask`, `spell_school_immune_mask`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
('1000003', '0', '0', '0', '0', '0', 'Arena 3v3 SoloQ', '3v3 Solo Queue Arena Master', '', '0', '80', '80', '2', '35', '1', '1', '1.14286', 1, 1, 20, '1', '1', '0', 1, 1, 1, 1, 1, '1', '131078', '0', '0', '0', '0', '0', '0', '0', '7', '0', '0', '0', '0', '0', '0', '0', '0', '', '0', '1', 1, 1, 1, 1, '0', '0', '1', '138936390', 0, '0', 'npc_solo3v3', '12340');


DELETE FROM `creature_template_model` WHERE `CreatureID` = 1000003;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(1000003, 0, 3280, 1, 1, 0);

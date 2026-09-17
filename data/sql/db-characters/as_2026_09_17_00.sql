-- Solo MMR used to accumulate the visible rating delta on top of the 1500 start value,
-- so stored rows hold ~1500 + rating. Reseed from the visible rating (floored at the
-- start MMR) so Elo matchmaking begins from a value that reflects results so far.
UPDATE `character_arena_stats` cas
JOIN `arena_team_member` atm ON atm.`guid` = cas.`guid`
JOIN `arena_team` at ON at.`arenaTeamId` = atm.`arenaTeamId` AND at.`type` = 4
SET cas.`matchMakerRating` = GREATEST(1500, atm.`personalRating`),
    cas.`maxMMR`           = GREATEST(1500, atm.`personalRating`)
WHERE cas.`slot` = 4;

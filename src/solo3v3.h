/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SOLO_3V3_H_
#define _SOLO_3V3_H_

#include "Common.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "Player.h"

// Custom 1v1 Arena Rated
constexpr uint32 BATTLEGROUND_QUEUE_1v1 = 11;

// custom 3v3 Arena solo
constexpr uint32 ARENA_TYPE_3v3_SOLO = 4;
constexpr uint32 ARENA_TEAM_SOLO_3v3 = 4;
constexpr uint32 ARENA_SLOT_SOLO_3v3 = 4;
constexpr uint32 BATTLEGROUND_QUEUE_3v3_SOLO = 12;
constexpr BattlegroundQueueTypeId bgQueueTypeId = (BattlegroundQueueTypeId)((int) BATTLEGROUND_QUEUE_3v3_SOLO);

const uint32 FORBIDDEN_TALENTS_IN_1V1_ARENA[] =
{
    // Healer
    201, // PriestDiscipline
    202, // PriestHoly
    382, // PaladinHoly
    262, // ShamanRestoration
    282, // DruidRestoration
    0
};

// SOLO_3V3_TALENTS found in: TalentTab.dbc -> TalentTabID
// Warrior, Rogue, Deathknight etc.
const uint32 SOLO_3V3_TALENTS_MELEE[] =
{
    383, // PaladinProtection
    163, // WarriorProtection
    161,
    182,
    398,
    164,
    181,
    263,
    281,
    399,
    183,
    381,
    400,
    0 // End
};

// Mage, Hunter, Warlock etc.
const uint32 SOLO_3V3_TALENTS_RANGE[] =
{
    81,
    261,
    283,
    302,
    361,
    41,
    303,
    363,
    61,
    203,
    301,
    362,
    0 // End
};

const uint32 SOLO_3V3_TALENTS_HEAL[] =
{
    201, // PriestDiscipline
    202, // PriestHoly
    382, // PaladinHoly
    262, // ShamanRestoration
    282, // DruidRestoration
    0 // End
};

enum Solo3v3TalentCat
{
    MELEE = 0,
    RANGE,
    HEALER,
    MAGE,
    WARLOCK,
    PRIEST,
    ROGUE,
    DRUID,
    HUNTER,
    SHAMAN,
    DK,
    PALADIN,
    WARRIOR,
    MAX_TALENT_CAT
};

#define BG_TEAMS_COUNT 2

class Solo3v3
{
public:
    static Solo3v3* instance();

    uint32 GetAverageMMR(ArenaTeam* team);
    void CheckStartSolo3v3Arena(Battleground* bg);
    void CleanUp3v3SoloQ(Battleground* bg);
    bool CheckSolo3v3Arena(BattlegroundQueue* queue, BattlegroundBracketId bracket_id, bool isRated);
    void CreateTempArenaTeamForQueue(BattlegroundQueue* queue, ArenaTeam* arenaTeams[]);
    void CountAsLoss(Player* player, bool isInProgress);

    // Return false, if player have invested more than 35 talentpoints in a forbidden talenttree.
    bool Arena3v3CheckTalents(Player* player);

    // Returns MELEE, RANGE or HEALER (depends on talent builds)
    Solo3v3TalentCat GetTalentCatForSolo3v3(Player* player);
    Solo3v3TalentCat GetFirstAvailableSlot(bool soloTeam[][MAX_TALENT_CAT]);

    // Returns true if candidate has an ignore relationship with any player already in the given team's selection pool
    bool HasIgnoreConflict(Player* candidate, BattlegroundQueue* queue, uint32 teamId);

    // Logs all arena participants to log_arena_fights / log_arena_memberstats for matches
    // that end before completion (leaver during preparation or missing player at arena start).
    // Must be called while bg->isRated() is still true, i.e. before SetRated(false).
    void SaveIncompleteMatchLogs(Battleground* bg);

private:
    struct Candidate
    {
        GroupQueueInfo*  group;
        Player*          player;
        Solo3v3TalentCat role;
        uint32           mmr;
        uint8            classId; //< player->GetClass() cached at queue time
    };

    uint32 GetMMR(Player* player, GroupQueueInfo* ginfo);

    int CountIgnorePairs(std::vector<uint32> const& indices, std::vector<Candidate> const& selected, bool avoidIgnore);

    // Returns true when the team at @p indices contains two players of the
    // same class that violate the configured stacking level and class mask.
    bool HasClassStackingConflict(
        std::vector<uint32> const& indices,
        std::vector<Candidate> const& selected,
        uint8 preventLevel,
        uint32 classMask) const;

    // Converts a WoW class ID (1–11) to its bit position for the
    // Solo.3v3.PreventClassStacking.Classes bitmask.
    // Mirrors 1<<(classId-1) for classes 1-9; Druid (11) maps to bit 10.
    static uint32 ClassIdToMaskBit(uint8 classId);

    void EnumerateCombinations(
        uint32 start,
        uint32 depth,
        std::vector<uint32>& combo,
        std::vector<Candidate> const& selected,
        uint32 teamSize,
        uint32 n,
        bool filterTalents,
        bool allDpsMatch,
        bool avoidIgnore,
        uint8 preventClassStacking,
        uint32 classStackMask,
        std::vector<uint32>& bestTeam1,
        bool& haveBest,
        uint64& bestDiff,
        int& bestIgnores);

    void AssignToPool(
        std::vector<uint32> const& indices,
        std::vector<Candidate> const& selected,
        uint32 poolTeam,
        BattlegroundQueue* queue,
        BattlegroundBracketId bracket_id,
        uint8 allianceGroupType,
        uint8 hordeGroupType,
        uint32 MinPlayers);

    std::unordered_set<uint32> arenasWithDeserter;
};

#define sSolo Solo3v3::instance()

#endif // _SOLO_3V3_H_

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

enum AZTHArenaType
{
    ARENA_TYPE_1v1 = 1, // Custom 1v1 Arena Rated
    ARENA_TYPE_3v3_SOLO = 4 // 3v3 soloqueue
};

enum AZTHArenaTeamType
{
    ARENA_TEAM_1v1 = 1, // Custom 1v1 Arena Rated
    ARENA_TEAM_SOLO_3v3 = 4 // 3v3 soloqueue
};

enum AZTHArenaSlot
{
    ARENA_SLOT_1v1 = 3, // Custom 1v1 Arena Rated
    ARENA_SLOT_SOLO_3v3 = 4 // 3v3 soloqueue
};

enum AZTHBattlegroundQueueTypeId
{
    BATTLEGROUND_QUEUE_1v1 = 11,
    BATTLEGROUND_QUEUE_3v3_SOLO = 12
};

// constexpr uint32 ARENA_TEAM_SOLO_3v3 = 6;
// constexpr uint32 ARENA_TYPE_3v3_SOLO = 6;
// constexpr uint32 ARENA_SLOT_SOLO_3v3 = 6;
// constexpr uint32 BATTLEGROUND_QUEUE_3v3_SOLO = 11;
constexpr BattlegroundQueueTypeId bgQueueTypeId = (BattlegroundQueueTypeId)((int)BATTLEGROUND_QUEUE_3v3);


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
    MAX_TALENT_CAT
};

#define BG_TEAMS_COUNT 2

class Solo3v3
{
public:
    static Solo3v3* instance();

    void SaveSoloDB(ArenaTeam* team);
    uint32 GetAverageMMR(ArenaTeam* team);
    void CheckStartSolo3v3Arena(Battleground* bg);
    void CleanUp3v3SoloQ(Battleground* bg);
    bool CheckSolo3v3Arena(BattlegroundQueue* queue, BattlegroundBracketId bracket_id);
    void CreateTempArenaTeamForQueue(BattlegroundQueue* queue, ArenaTeam* arenaTeams[]);

    // Return false, if player have invested more than 35 talentpoints in a forbidden talenttree.
    bool Arena3v3CheckTalents(Player* player);

    // Returns MELEE, RANGE or HEALER (depends on talent builds)
    Solo3v3TalentCat GetTalentCatForSolo3v3(Player* player);
};

#define sSolo Solo3v3::instance()

#endif // _SOLO_3V3_H_

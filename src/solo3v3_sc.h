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

#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "DisableMgr.h"
#include "Chat.h"
#include "Language.h"
#include "ScriptedGossip.h"
#include "Config.h"
#include "Battleground.h"
#include "solo3v3.h"

class NpcSolo3v3 : public CreatureScript
{
public:
    NpcSolo3v3() : CreatureScript("npc_solo3v3")
    {
        Initialize();
    }

    void Initialize();
    bool OnGossipHello(Player* player, Creature* creature) override;
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override;

private:
    bool ArenaCheckFullEquipAndTalents(Player* player);
    bool JoinQueueArena(Player* player, Creature* creature, bool isRated);
    bool CreateArenateam(Player* player, Creature* creature);
    void fetchQueueList();

    int cache3v3Queue[MAX_TALENT_CAT];
    uint32 lastFetchQueueList;
};

class Solo3v3BG : public AllBattlegroundScript
{
public:
    Solo3v3BG() : AllBattlegroundScript("Solo3v3_BG") {}

    void OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundUpdate(Battleground* bg, uint32 /*diff*/) override;
    bool OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundDestroy(Battleground* bg) override;
    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /* winnerTeamId */) override;
};

class ConfigLoader3v3Arena : public WorldScript
{
public:
    ConfigLoader3v3Arena() : WorldScript("config_loader_3v3_arena") {}

    virtual void OnAfterConfigLoad(bool /*Reload*/) override;
};

class Team3v3arena : public ArenaTeamScript
{
public:
    Team3v3arena() : ArenaTeamScript("team_3v3_arena") {}

    void OnGetSlotByType(const uint32 type, uint8& slot) override;
    void OnGetArenaPoints(ArenaTeam* at, float& points) override;
    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override;
    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override;
};

class PlayerScript3v3Arena : public PlayerScript
{
public:
    PlayerScript3v3Arena() : PlayerScript("player_script_3v3_arena") {}

    void OnLogin(Player* pPlayer) override;
    void GetCustomGetArenaTeamId(const Player* player, uint8 slot, uint32& id) const override;
    void GetCustomArenaPersonalRating(const Player* player, uint8 slot, uint32& rating) const override;
    void OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override;
    void OnGetArenaTeamId(Player* player, uint8 slot, uint32& result) override;
    bool NotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType type, uint32 value) override;
    bool CanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 action) override;
};

void AddSC_Solo_3v3_Arena()
{
    // ArenaSlotByType
    // if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_1v1))
    //     ArenaTeam::ArenaSlotByType[ARENA_TEAM_1v1] = ARENA_SLOT_1v1;

    if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_SOLO_3v3))
        ArenaTeam::ArenaSlotByType[ARENA_TEAM_SOLO_3v3] = ARENA_SLOT_SOLO_3v3;

    // ArenaReqPlayersForType
    // if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_1v1))
    //     ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_1v1] = 2;

    if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_3v3_SOLO))
        ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_3v3_SOLO] = 6;

    // queueToBg
    // if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_1v1))
    //     BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_1v1] = BATTLEGROUND_AA;

    if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_3v3_SOLO] = BATTLEGROUND_AA;

    // ArenaTypeToQueue
    // if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_1v1))
    //     BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_1v1] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1;

    if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_3v3_SOLO))
        BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_3v3_SOLO] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO;

    // QueueToArenaType
    // if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_1v1))
    //     BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_1v1] = (ArenaType)ARENA_TYPE_1v1;

    if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_3v3_SOLO] = (ArenaType)ARENA_TYPE_3v3_SOLO;

    new NpcSolo3v3();
    new Solo3v3BG();
    new Team3v3arena();
    new ConfigLoader3v3Arena();
    new PlayerScript3v3Arena();
}

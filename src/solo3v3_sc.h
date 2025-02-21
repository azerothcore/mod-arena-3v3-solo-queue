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
#include "Spell.h"

#define NPC_TEXT_3v3 1000004

enum Npc3v3Actions {
    NPC_3v3_ACTION_MAIN_MENU = 0,
    NPC_3v3_ACTION_CREATE_ARENA_TEAM = 1,
    NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED = 2,
    NPC_3v3_ACTION_LEAVE_QUEUE = 3,
    NPC_3v3_ACTION_GET_STATISTICS = 4,
    NPC_3v3_ACTION_DISBAND_ARENATEAM = 5,
    NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED = 6,
    NPC_3v3_ACTION_SCRIPT_INFO = 8
};

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
    bool ArenaCheckFullEquipAndTalents(Player* player);
    bool JoinQueueArena(Player* player, Creature* creature, bool isRated);
    bool CreateArenateam(Player* player, Creature* creature);

private:
    void fetchQueueList();
    int cache3v3Queue[MAX_TALENT_CAT];
    uint32 lastFetchQueueList;
};

class Solo3v3BG : public AllBattlegroundScript
{
public:
    Solo3v3BG() : AllBattlegroundScript("Solo3v3_BG", {
        ALLBATTLEGROUNDHOOK_ON_QUEUE_UPDATE,
        ALLBATTLEGROUNDHOOK_ON_QUEUE_UPDATE_VALIDITY,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END_REWARD
    }) {}

    void OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override;
    bool OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundDestroy(Battleground* bg) override;
    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /* winnerTeamId */) override;
};

class ConfigLoader3v3Arena : public WorldScript
{
public:
    ConfigLoader3v3Arena() : WorldScript("config_loader_3v3_arena", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) {}

    virtual void OnAfterConfigLoad(bool /*Reload*/) override;
};

class Team3v3arena : public ArenaTeamScript
{
public:
    Team3v3arena() : ArenaTeamScript("team_3v3_arena", {
        ARENATEAMHOOK_ON_GET_SLOT_BY_TYPE,
        ARENATEAMHOOK_ON_GET_ARENA_POINTS,
        ARENATEAMHOOK_ON_TYPEID_TO_QUEUEID,
        ARENATEAMHOOK_ON_QUEUEID_TO_ARENA_TYPE
    }) {}

    void OnGetSlotByType(const uint32 type, uint8& slot) override;
    void OnGetArenaPoints(ArenaTeam* at, float& points) override;
    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override;
    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override;
};

class PlayerScript3v3Arena : public PlayerScript
{
public:
    PlayerScript3v3Arena() : PlayerScript("player_script_3v3_arena", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_GET_ARENA_PERSONAL_RATING,
        PLAYERHOOK_ON_GET_MAX_PERSONAL_ARENA_RATING_REQUIREMENT,
        PLAYERHOOK_ON_GET_ARENA_TEAM_ID,
        PLAYERHOOK_NOT_SET_ARENA_TEAM_INFO_FIELD,
        PLAYERHOOK_CAN_BATTLEFIELD_PORT,
        PLAYERHOOK_ON_BATTLEGROUND_DESERTION
    }) {}

    void OnPlayerLogin(Player* pPlayer) override;
    void OnPlayerGetArenaPersonalRating(Player* player, uint8 slot, uint32& rating) override;
    void OnPlayerGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override;
    void OnPlayerGetArenaTeamId(Player* player, uint8 slot, uint32& result) override;
    bool OnPlayerNotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType type, uint32 value) override;
    bool OnPlayerCanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 action) override;
    void OnPlayerBattlegroundDesertion(Player* player, const BattlegroundDesertionType type) override;
};

class Arena_SC : public ArenaScript
{
public:
    Arena_SC() : ArenaScript("Arena_SC", {
        ARENAHOOK_CAN_ADD_MEMBER,
        ARENAHOOK_ON_GET_POINTS,
        ARENAHOOK_CAN_SAVE_TO_DB,
        ARENAHOOK_ON_ARENA_START
    }) { }

    bool CanAddMember(ArenaTeam* team, ObjectGuid /* playerGuid */) override
    {
        if (!team)
            return false;

        if (!team->GetMembersSize())
            return true;

        if (team->GetType() == ARENA_TEAM_SOLO_3v3)
            return false;

        return true;
    }

    void OnGetPoints(ArenaTeam* team, uint32 /* memberRating */, float& points) override
    {
        if (!team)
            return;

        if (team->GetType() == ARENA_TEAM_SOLO_3v3)
            points *= sConfigMgr->GetOption<float>("Solo.3v3.ArenaPointsMulti", 0.88f);
    }

    bool CanSaveToDB(ArenaTeam* team) override
    {
        if (team->GetId() >= MAX_ARENA_TEAM_ID)
            return false;

        return true;
    }

    void OnArenaStart(Battleground* bg) override;
};

class Solo3v3Spell : public SpellSC
{
public:
    Solo3v3Spell() : SpellSC("Solo3v3Spell", {
        ALLSPELLHOOK_CAN_SELECT_SPEC_TALENT
    }) { }


    bool CanSelectSpecTalent(Spell* spell) override
    {
        if (!spell)
            return false;

        if (spell->GetCaster()->IsPlayer())
        {
            Player* plr = spell->GetCaster()->ToPlayer();

            if (plr->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
            {
                plr->GetSession()->SendAreaTriggerMessage("You can't change your talents while in queue for solo arena.");
                return false;
            }
        }

        return true;
    }

};

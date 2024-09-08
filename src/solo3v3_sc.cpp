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

#include "solo3v3_sc.h"

void NpcSolo3v3::Initialize()
{
    for (int i = 0; i < MAX_TALENT_CAT; i++)
        cache3v3Queue[i] = 0;

    lastFetchQueueList = 0;
}

bool NpcSolo3v3::OnGossipHello(Player* player, Creature* creature)
{
    if (!player || !creature)
        return true;

    if (sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true) == false)
    {
        ChatHandler(player->GetSession()).SendSysMessage("Arena disabled!");
        return true;
    }

    fetchQueueList();
    std::stringstream infoQueue;

    infoQueue << " ---------------------------------------------";
    infoQueue << "\n               " << (cache3v3Queue[MELEE] + cache3v3Queue[RANGE] + cache3v3Queue[HEALER]) << " Queued Player(s)";
    infoQueue << "\n                 |TInterface/ICONS/ability_rogue_shadowstrikes:21:21:0:11|t       |TInterface/ICONS/spell_shadow_shadowembrace:21:21:0:11|t        |TInterface/ICONS/spell_holy_holynova:21:21:0:11|t";
    infoQueue << "\n\n              Melee  Caster  Healer";
    infoQueue << "\n                 [" << cache3v3Queue[MELEE] << "]        [" << cache3v3Queue[RANGE] << "]        [" << cache3v3Queue[HEALER] << "]";
    infoQueue << "\n\n   |TInterface\\icons\\inv_jewelry_talisman_04:17:17:0:30|t [" << cache3v3Queue[SHAMAN] << "]  " << " |TInterface\\icons\\inv_hammer_01:17:17:0:30|t [" << cache3v3Queue[PALADIN] << "]  "
        << " |TInterface\\icons\\inv_sword_27:17:17:0:30|t [" << cache3v3Queue[WARRIOR] << "]  " << " |TInterface\\icons\\inv_misc_monsterclaw_04:17:17:0:30|t [" << cache3v3Queue[DRUID] << "]  "
        << " |TInterface\\icons\\spell_deathknight_classicon:17:17:0:30|t [" << cache3v3Queue[DK] << "]" << "\n   |TInterface\\icons\\spell_nature_drowsy:17:17:0:30|t [" << cache3v3Queue[WARLOCK] << "]  "
        << " |TInterface\\icons\\inv_staff_30:17:17:0:30|t [" << cache3v3Queue[PRIEST] << "]  " << " |TInterface\\icons\\inv_weapon_bow_07:17:17:0:30|t [" << cache3v3Queue[HUNTER] << "]  "
        << " |TInterface\\icons\\inv_staff_13:17:17:0:30|t [" << cache3v3Queue[MAGE] << "]  " << " |TInterface\\icons\\inv_throwingknife_04:17:17:0:30|t [" << cache3v3Queue[ROGUE] << "]";
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, infoQueue.str().c_str(), GOSSIP_SENDER_MAIN, 0);

    if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_2v2_7:30|t Leave Solo queue", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_LEAVE_QUEUE, "Are you sure you want to remove the solo queue?", 0, false);

    if (!player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO)))
    {
        uint32 cost = sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1);
        if (player->IsPvP())
            cost = 0;

        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_2v2_7:30|t  Create new Solo arena team", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_CREATE_ARENA_TEAM, "Create new solo arena team?", cost, false);
    }
    else
    {
        if (!player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
        {
            //AddGossipItemFor(player,GOSSIP_ICON_INTERACT_1, "Queue up for 1vs1 Wargame\n", GOSSIP_SENDER_MAIN, 20);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30|t Queue up for 3vs3 Arena Solo\n", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/Achievement_Arena_2v2_7:30|t Disband Arena team", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_DISBAND_ARENATEAM, "Are you sure?", 0, false);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Coin_01:30|t Show statistics", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_GET_STATISTICS);
    }

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Coin_03:30|t How to Use NPC?", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_SCRIPT_INFO);

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, infoQueue.str().c_str(), GOSSIP_SENDER_MAIN, 0);
    SendGossipMenuFor(player, 60015, creature->GetGUID());
    return true;
}

bool NpcSolo3v3::OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
    if (!player || !creature)
        return true;

    player->PlayerTalkClass->ClearMenus();

    switch (action)
    {
        case NPC_3v3_ACTION_CREATE_ARENA_TEAM:
        {
            if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80) <= player->GetLevel())
            {
                int cost = sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1);

                if (player->IsPvP())
                    cost = 0;

                if (cost >= 0 && player->GetMoney() >= uint32(cost) && CreateArenateam(player, creature))
                    player->ModifyMoney(sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1) * -1);
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("You need level %u+ to create an arena team.", sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80));
            }

            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED:
        {
            // check Deserter debuff
            if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
            {
                WorldPacket data;
                sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
                player->GetSession()->SendPacket(&data);
            }
            else
                if (ArenaCheckFullEquipAndTalents(player) && JoinQueueArena(player, creature, true) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("Something went wrong while joining queue. Already in another queue?");

            CloseGossipMenuFor(player);
            return true;
        }

        case NPC_3v3_ACTION_LEAVE_QUEUE:
        {
            if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
            {
                uint8 arenaType = ARENA_TYPE_3v3_SOLO;

                WorldPacket Data;
                Data << arenaType << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
                player->GetSession()->HandleBattleFieldPortOpcode(Data);
                CloseGossipMenuFor(player);

            }
            return true;
        }

        case NPC_3v3_ACTION_GET_STATISTICS:
        {
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO)));
            if (at)
            {
                std::stringstream s;
                s << "Rating: " << at->GetStats().Rating;
                s << "\nPersonal Rating: " << player->GetArenaPersonalRating(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO));
                s << "\nRank: " << at->GetStats().Rank;
                s << "\nSeason Games: " << at->GetStats().SeasonGames;
                s << "\nSeason Wins: " << at->GetStats().SeasonWins;
                s << "\nWeek Games: " << at->GetStats().WeekGames;
                s << "\nWeek Wins: " << at->GetStats().WeekWins;

                ChatHandler(player->GetSession()).PSendSysMessage("{}", s.str().c_str());
                CloseGossipMenuFor(player);
            }

            return true;
        }
        case NPC_3v3_ACTION_DISBAND_ARENATEAM:
        {
            WorldPacket Data;
            Data << player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO));
            player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
            ChatHandler(player->GetSession()).PSendSysMessage("Arena team deleted!");
            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case NPC_3v3_ACTION_SCRIPT_INFO:
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Click on Create new 3v3 SoloQ Arena team", GOSSIP_SENDER_MAIN, action);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Join 3v3 SoloQ Arena and ready!", GOSSIP_SENDER_MAIN, action);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Enjoy!", GOSSIP_SENDER_MAIN, action);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back", GOSSIP_SENDER_MAIN, 7);
            SendGossipMenuFor(player, 68, creature->GetGUID());
            return true;
        }
        break;
    }

    OnGossipHello(player, creature);
    return true;
}

bool NpcSolo3v3::ArenaCheckFullEquipAndTalents(Player* player)
{
    if (!player)
        return false;

    if (!sConfigMgr->GetOption<bool>("Arena.CheckEquipAndTalents", true))
        return true;

    std::stringstream err;

    if (player->GetFreeTalentPoints() > 0)
        err << "You have currently " << player->GetFreeTalentPoints() << " free talent points. Please spend all your talent points before queueing arena.\n";

    Item* newItem = NULL;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_OFFHAND || slot == EQUIPMENT_SLOT_RANGED || slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
            continue;

        newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (newItem == NULL)
        {
            err << "Your character is not fully equipped.\n";
            break;
        }
    }

    if (err.str().length() > 0)
    {
        ChatHandler(player->GetSession()).SendSysMessage(err.str().c_str());
        return false;
    }

    return true;
}

bool NpcSolo3v3::JoinQueueArena(Player* player, Creature* creature, bool isRated)
{
    if (!player || !creature)
        return false;

    if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80) > player->GetLevel())
        return false;

    uint8 arenatype = ARENA_TYPE_3v3_SOLO;
    // uint8 arenaslot = ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO);
    uint32 arenaRating = 0;
    uint32 matchmakerRating = 0;

    // ignore if we already in BG or BG queue
    if (player->InBattleground())
        return false;

    //check existance
    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);

    if (!bg)
    {
        LOG_ERROR("module", "Battleground: template bg (all arenas) not found");
        return false;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
    {
        ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);
        return false;
    }

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->GetLevel());

    if (!bracketEntry)
        return false;

    // check if already in queue
    if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return false; // player is already in this queue

    // check if has free queue slots
    if (!player->HasFreeBattlegroundQueueId())
        return false;

    uint32 ateamId = 0;

    if (isRated)
    {
        ateamId = player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO));
        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(ateamId);
        if (!at)
        {
            player->GetSession()->SendNotInArenaTeamPacket(arenatype);
            return false;
        }

        // get the team rating for queueing
        arenaRating = std::max(0u, at->GetRating());
        matchmakerRating = arenaRating;
        // the arenateam id must match for everyone in the group
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    BattlegroundTypeId bgTypeId = BATTLEGROUND_AA;

    bg->SetRated(isRated);
    bg->SetMinPlayersPerTeam(3);

    GroupQueueInfo* ginfo = bgQueue.AddGroup(player, nullptr, bgTypeId, bracketEntry, arenatype, isRated, false, arenaRating, matchmakerRating, ateamId, 0);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
    uint32 queueSlot = player->AddBattlegroundQueueId(bgQueueTypeId);

    // send status packet (in queue)
    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenatype, TEAM_NEUTRAL, isRated);
    player->GetSession()->SendPacket(&data);

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, ARENA_TYPE_3v3_SOLO, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());

    sScriptMgr->OnPlayerJoinArena(player);

    return true;
}

bool NpcSolo3v3::CreateArenateam(Player* player, Creature* creature)
{
    if (!player || !creature)
        return false;

    // Check if player is already in an arena team
    if (player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO)))
    {
        player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
        return false;
    }

    // Teamname = playername
    // if team name exist, we have to choose another name (playername + number)
    int i = 1;
    std::stringstream teamName;
    teamName << player->GetName();

    do
    {
        if (sArenaTeamMgr->GetArenaTeamByName(teamName.str()) != NULL) // teamname exist, so choose another name
        {
            teamName.str(std::string());
            teamName << player->GetName() << i++;
        }
        else
            break;
    }
    while (i < 100); // should never happen

    // Create arena team
    ArenaTeam* arenaTeam = new ArenaTeam();

    if (!arenaTeam->Create(player->GetGUID(), uint8(ARENA_TYPE_3v3_SOLO), teamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
    {
        delete arenaTeam;
        return false;
    }

    // Register arena team
    sArenaTeamMgr->AddArenaTeam(arenaTeam);

    ChatHandler(player->GetSession()).SendSysMessage("Arena team successful created!");

    return true;
}

void NpcSolo3v3::fetchQueueList()
{
    if (GetMSTimeDiffToNow(lastFetchQueueList) < 1000)
        return;

    lastFetchQueueList = getMSTime();

    BattlegroundQueue* queue = &sBattlegroundMgr->GetBattlegroundQueue((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO);

    for (int i = 0; i < MAX_TALENT_CAT; i++)
        cache3v3Queue[i] = 0;

    for (int i = BG_BRACKET_ID_FIRST; i <= BG_BRACKET_ID_LAST; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (auto queueInfo : queue->m_QueuedGroups[i][j])
            {
                if (queueInfo->IsInvitedToBGInstanceGUID) // Skip when invited
                    continue;

                for (auto const& playerGuid : queueInfo->Players)
                {
                    Player* _player = ObjectAccessor::FindPlayer(playerGuid);
                    if (!_player)
                        continue;

                    switch (_player->getClass())
                    {
                        case CLASS_WARRIOR:
                            cache3v3Queue[WARRIOR]++;
                            break;
                        case CLASS_PALADIN:
                            cache3v3Queue[PALADIN]++;
                            break;
                        case CLASS_DEATH_KNIGHT:
                            cache3v3Queue[DK]++;
                            break;
                        case CLASS_HUNTER:
                            cache3v3Queue[HUNTER]++;
                            break;
                        case CLASS_SHAMAN:
                            cache3v3Queue[SHAMAN]++;
                            break;
                        case CLASS_ROGUE:
                            cache3v3Queue[ROGUE]++;
                            break;
                        case CLASS_DRUID:
                            cache3v3Queue[DRUID]++;
                            break;
                        case CLASS_MAGE:
                            cache3v3Queue[MAGE]++;
                            break;
                        case CLASS_WARLOCK:
                            cache3v3Queue[WARLOCK]++;
                            break;
                        case CLASS_PRIEST:
                            cache3v3Queue[PRIEST]++;
                            break;
                        default:
                            break;
                    }

                    Solo3v3TalentCat plrCat = sSolo->GetTalentCatForSolo3v3(_player); // get talent cat
                    cache3v3Queue[plrCat]++;
                }
            }
        }
    }
}

void Solo3v3BG::OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/)
{
    if (arenaType != (ArenaType)ARENA_TYPE_3v3_SOLO)
        return;

    Battleground* bg_template = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);

    if (!bg_template)
        return;

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketById(bg_template->GetMapId(), bracket_id);
    if (!bracketEntry)
        return;

    // Solo 3v3
    if (sSolo->CheckSolo3v3Arena(queue, bracket_id))
    {
        Battleground* arena = sBattlegroundMgr->CreateNewBattleground(bgTypeId, bracketEntry, arenaType, isRated);
        if (!arena)
            return;

        // Create temp arena team and store arenaTeamId
        ArenaTeam* arenaTeams[BG_TEAMS_COUNT];
        sSolo->CreateTempArenaTeamForQueue(queue, arenaTeams);

        // invite those selection pools
        for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
            for (auto const& citr : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
            {
                citr->ArenaTeamId = arenaTeams[i]->GetId();
                queue->InviteGroupToBG(citr, arena, citr->teamId);
            }

        // Override ArenaTeamId to temp arena team (was first set in InviteGroupToBG)
        arena->SetArenaTeamIdForTeam(TEAM_ALLIANCE, arenaTeams[TEAM_ALLIANCE]->GetId());
        arena->SetArenaTeamIdForTeam(TEAM_HORDE, arenaTeams[TEAM_HORDE]->GetId());

        oldTeamRatingAlliance = arenaTeams[TEAM_ALLIANCE]->GetStats().Rating;
        oldTeamRatingHorde = arenaTeams[TEAM_HORDE]->GetStats().Rating;

        // Set matchmaker rating for calculating rating-modifier on EndBattleground (when a team has won/lost)
        arena->SetArenaMatchmakerRating(TEAM_ALLIANCE, sSolo->GetAverageMMR(arenaTeams[TEAM_ALLIANCE]));
        arena->SetArenaMatchmakerRating(TEAM_HORDE, sSolo->GetAverageMMR(arenaTeams[TEAM_HORDE]));

        // start bg
        arena->StartBattleground();
    }
}

bool Solo3v3BG::OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/)
{
    // if it's an arena 3v3soloQueue, return false to exit from BattlegroundQueueUpdate
    if (arenaType == (ArenaType)ARENA_TYPE_3v3_SOLO)
        return false;

    return true;
}

void Solo3v3BG::OnBattlegroundUpdate(Battleground* bg, uint32 /*diff*/)
{
    if (bg->GetStatus() != STATUS_IN_PROGRESS || !bg->isArena())
        return;

    sSolo->CheckStartSolo3v3Arena(bg);
}

void Solo3v3BG::OnBattlegroundDestroy(Battleground* bg)
{
    sSolo->CleanUp3v3SoloQ(bg);
}

void Solo3v3BG::OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId)
{
    if (bg->isRated() && bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
    {
        ArenaTeam* plrArenaTeam = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_3v3_SOLO);

        if (!plrArenaTeam)
            return;

        ArenaTeamStats atStats = plrArenaTeam->GetStats();
        int32 ratingModifier;
        int32 oldTeamRating;

        TeamId bgTeamId = player->GetBgTeamId();
        const bool isPlayerWinning = bgTeamId == winnerTeamId;
        if (isPlayerWinning) {
            ArenaTeam* winnerArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(winnerTeamId == TEAM_NEUTRAL ? TEAM_HORDE : winnerTeamId));
            oldTeamRating = winnerTeamId == TEAM_HORDE ? oldTeamRatingHorde : oldTeamRatingAlliance;
            ratingModifier = int32(winnerArenaTeam->GetRating()) - oldTeamRating;

            atStats.SeasonWins += 1;
            atStats.WeekWins += 1;
        } else {
            ArenaTeam* loserArenaTeam  = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(winnerTeamId == TEAM_NEUTRAL ? TEAM_ALLIANCE : bg->GetOtherTeamId(winnerTeamId)));
            oldTeamRating = winnerTeamId != TEAM_HORDE ? oldTeamRatingHorde : oldTeamRatingAlliance;
            ratingModifier = int32(loserArenaTeam->GetRating()) - oldTeamRating;
        }

        ArenaTeam* loserArenaTeam  = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(winnerTeamId == TEAM_NEUTRAL ? TEAM_ALLIANCE : bg->GetOtherTeamId(winnerTeamId)));
        ArenaTeam* winnerArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(winnerTeamId == TEAM_NEUTRAL ? TEAM_HORDE : winnerTeamId));

        if (int32(atStats.Rating) + ratingModifier < 0)
            atStats.Rating = 0;
        else
            atStats.Rating += ratingModifier;

        atStats.SeasonGames += 1;
        atStats.WeekGames += 1;

        // Update team's rank, start with rank 1 and increase until no team with more rating was found
        atStats.Rank = 1;
        ArenaTeamMgr::ArenaTeamContainer::const_iterator i = sArenaTeamMgr->GetArenaTeamMapBegin();
        for (; i != sArenaTeamMgr->GetArenaTeamMapEnd(); ++i) {
            if (i->second->GetType() == ARENA_TYPE_3v3_SOLO && i->second->GetStats().Rating > atStats.Rating)
                ++atStats.Rank;
        }

        plrArenaTeam->SetArenaTeamStats(atStats);
        plrArenaTeam->NotifyStatsChanged();

        for (ArenaTeam::MemberList::iterator itr = plrArenaTeam->GetMembers().begin(); itr != plrArenaTeam->GetMembers().end(); ++itr)
        {
            if (itr->Guid == player->GetGUID())
            {
                itr->PersonalRating = atStats.Rating;
                itr->WeekWins = atStats.WeekWins;
                itr->SeasonWins = atStats.SeasonWins;
                itr->WeekGames = atStats.WeekGames;
                itr->SeasonGames = atStats.SeasonGames;

                if (isPlayerWinning) {
                    // itr->MatchMakerRating = bg->GetArenaMatchmakerRating(winnerTeamId);
                    itr->MatchMakerRating += ratingModifier;
                    itr->MaxMMR = std::max(itr->MaxMMR, itr->MatchMakerRating);
                } else {
                    // itr->MatchMakerRating = bg->GetArenaMatchmakerRating(bg->GetOtherTeamId(winnerTeamId));

                    if (int32(itr->MatchMakerRating) + ratingModifier < 0)
                        itr->MatchMakerRating = 0;
                    else
                        itr->MatchMakerRating += ratingModifier;
                }

                break;
            }

        }

        plrArenaTeam->SaveToDB(true);
    }
}

void ConfigLoader3v3Arena::OnAfterConfigLoad(bool /*Reload*/)
{
    ArenaTeam::ArenaSlotByType.emplace(ARENA_TEAM_SOLO_3v3, ARENA_SLOT_SOLO_3v3);
    ArenaTeam::ArenaReqPlayersForType.emplace(ARENA_TYPE_3v3_SOLO, 6);

    BattlegroundMgr::queueToBg.insert({ BATTLEGROUND_QUEUE_3v3_SOLO, BATTLEGROUND_AA });
    BattlegroundMgr::QueueToArenaType.emplace(BATTLEGROUND_QUEUE_3v3_SOLO, (ArenaType)ARENA_TYPE_3v3_SOLO);
    BattlegroundMgr::ArenaTypeToQueue.emplace(ARENA_TYPE_3v3_SOLO, (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO);
}

void Team3v3arena::OnGetSlotByType(const uint32 type, uint8& slot)
{
    if (type == ARENA_TYPE_3v3_SOLO)
    {
        slot = ARENA_SLOT_SOLO_3v3;
    }
}

void Team3v3arena::OnGetArenaPoints(ArenaTeam* at, float& points)
{
    if (at->GetType() == ARENA_TYPE_3v3_SOLO)
    {
        points *= sConfigMgr->GetOption<float>("Solo.3v3.ArenaPointsMulti", 0.8f);
    }
}

void Team3v3arena::OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId)
{
    if (arenaType == ARENA_TYPE_3v3_SOLO)
    {
        _bgQueueTypeId = bgQueueTypeId;
    }
}

void Team3v3arena::OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType)
{
    if (_bgQueueTypeId == bgQueueTypeId)
    {
        arenaType = ARENA_TYPE_3v3_SOLO;
    }
}

void PlayerScript3v3Arena::OnLogin(Player* pPlayer)
{
    if (sConfigMgr->GetOption<bool>("Solo.3v3.Enable", false)) {
        ChatHandler(pPlayer->GetSession()).SendSysMessage("This server is running the |cff4CFF00Arena solo Q 3v3 |rmodule.");
    }
}

// not sure if this function is ever called, TODO: verify if the hook is called, if not, remove it
void PlayerScript3v3Arena::GetCustomGetArenaTeamId(const Player* player, uint8 slot, uint32& id) const
{
    if (slot == ARENA_SLOT_SOLO_3v3)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_3v3_SOLO))
        {
            id = at->GetId();
        }
    }
}

void PlayerScript3v3Arena::OnGetArenaPersonalRating(Player* player, uint8 slot, uint32& rating)
{
    if (slot == ARENA_SLOT_SOLO_3v3)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_3v3_SOLO))
        {
            rating = at->GetRating();
        }
    }
}

void PlayerScript3v3Arena::OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const
{
    if (minslot < 6)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TYPE_3v3_SOLO))
        {
            maxArenaRating = std::max(at->GetRating(), maxArenaRating);
        }
    }
}

void PlayerScript3v3Arena::OnGetArenaTeamId(Player* player, uint8 slot, uint32& result)
{
    if (!player)
        return;

    // [AZTH] use static method of ArenaTeam to retrieve the slot
    // if (slot == ArenaTeam::GetSlotByType(ARENA_TEAM_1v1))
    //     result = player->GetArenaTeamIdFromDB(player->GetGUID(), ARENA_TEAM_1v1);

    if (slot == ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO))
        result = player->GetArenaTeamIdFromDB(player->GetGUID(), ARENA_TYPE_3v3_SOLO);
}

bool PlayerScript3v3Arena::NotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType /* type */, uint32 /* value */)
{
    if (!player)
        return false;

    // [AZTH] avoid higher slots to be set in datafield
    // if (slot == ArenaTeam::GetSlotByType(ARENA_TEAM_1v1))
    // {
    //     sAZTH->GetAZTHPlayer(player)->setArena1v1Info(type, value);
    //     return false;
    // }

    if (slot == ArenaTeam::GetSlotByType(ARENA_TYPE_3v3_SOLO))
    {
        // sAZTH->GetAZTHPlayer(player)->setArena3v3Info(type, value);
        return false;
    }

    return true;
}


bool PlayerScript3v3Arena::CanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 /*action*/)
{
    if (!player)
        return false;

    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(BGTypeID, arenaType);
    if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
        return false;

    // if ((bgQueueTypeId == (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1 || bgQueueTypeId == (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO
    //     && (action == 1 /*accept join*/  && !sSolo->Arena1v1CheckTalents(player)))
    //     return false;

    return true;
}


// class Spell_SC : public SpellSC
// {
// public:
//     Spell_SC() : SpellSC("Spell_SC") { }

//     bool CanSelectSpecTalent(Spell* spell) override
//     {
//         if (!spell)
//             return false;

//         if (spell->GetCaster()->isPlayer())
//         {
//             Player* plr = spell->GetCaster()->ToPlayer();

//             if (plr->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO) /*||
//                 plr->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1)*/)
//             {
//                 plr->GetSession()->SendAreaTriggerMessage("You can't change your talents while in queue for 3v3."); // or 1v1
//                 return false;
//             }
//         }

//         return true;
//     }
// }

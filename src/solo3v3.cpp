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

#include "solo3v3.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "Config.h"
#include "GameTime.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "DisableMgr.h"
#include "SocialMgr.h"
#include <algorithm>
#include <functional>

Solo3v3* Solo3v3::instance()
{
    static Solo3v3 instance;
    return &instance;
}

uint32 Solo3v3::GetAverageMMR(ArenaTeam* team)
{
    if (!team)
        return 0;

    // this could be improved with a better balanced calculation
    uint32 matchMakerRating = team->GetStats().Rating;

    return matchMakerRating;
}

void Solo3v3::CountAsLoss(Player* player, bool isInProgress)
{
    if (player->IsSpectator())
        return;

    ArenaTeam* plrArenaTeam = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3));

    if (!plrArenaTeam)
        return;

    int32 ratingLoss = 0;
    uint32 instanceId = 0;

    bool playerLeftAlive = player->IsAlive();

    if (Battleground* bg = player->GetBattleground())
        instanceId = bg->GetInstanceID();

    // leave while arena is in progress but player is already dead - no penalty
    if (isInProgress && !playerLeftAlive)
        return;

    // leave while arena is in progress
    if (isInProgress && playerLeftAlive)
    {
        bool isFirstLeaver = instanceId && arenasWithDeserter.count(instanceId) == 0;
        ratingLoss = sConfigMgr->GetOption<int32>(isFirstLeaver ? "Solo.3v3.RatingPenalty.FirstLeaveDuringMatch" : "Solo.3v3.RatingPenalty.LeaveDuringMatch", isFirstLeaver ? 50 : 24);

        if (isFirstLeaver)
        {
            arenasWithDeserter.insert(instanceId);

            if (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true))
                player->CastSpell(player, 26013, true);
        }
    }

    // leave while arena is in preparation || don't accept queue || logout while invited
    else
    {
        ratingLoss = sConfigMgr->GetOption<int32>("Solo.3v3.RatingPenalty.LeaveBeforeMatchStart", 50);
        player->CastSpell(player, 26013, true);
    }

    ArenaTeamStats atStats = plrArenaTeam->GetStats();

    if (int32(atStats.Rating) - ratingLoss < 0)
        atStats.Rating = 0;
    else
        atStats.Rating -= ratingLoss;

    atStats.SeasonGames += 1;
    atStats.WeekGames += 1;
    atStats.Rank = 1;

    // Update team's rank, start with rank 1 and increase until no team with more rating was found
    ArenaTeamMgr::ArenaTeamContainer::const_iterator i = sArenaTeamMgr->GetArenaTeamMapBegin();
    for (; i != sArenaTeamMgr->GetArenaTeamMapEnd(); ++i) {
        if (i->second->GetType() == ARENA_TEAM_SOLO_3v3 && i->second->GetStats().Rating > atStats.Rating)
            ++atStats.Rank;
    }

    for (ArenaTeam::MemberList::iterator itr = plrArenaTeam->GetMembers().begin(); itr != plrArenaTeam->GetMembers().end(); ++itr) {
        if (itr->Guid == player->GetGUID()) {
            itr->WeekGames += 1;
            itr->SeasonGames += 1;
            itr->PersonalRating = atStats.Rating;

            if (int32(itr->MatchMakerRating) - ratingLoss < 0)
                itr->MatchMakerRating = 0;
            else
                itr->MatchMakerRating -= ratingLoss;

            break;
        }
    }

    plrArenaTeam->SetArenaTeamStats(atStats);
    plrArenaTeam->NotifyStatsChanged();
    plrArenaTeam->SaveToDB(true);
}

void Solo3v3::CleanUp3v3SoloQ(Battleground* bg)
{
    // Cleanup temp arena teams for solo 3v3
    if (bg->isArena() && bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
    {
        uint32 instanceId = bg->GetInstanceID();
        if (instanceId)
            arenasWithDeserter.erase(instanceId);

        ArenaTeam* tempAlliArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(TEAM_ALLIANCE));
        ArenaTeam* tempHordeArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(TEAM_HORDE));

        if (tempAlliArenaTeam && tempAlliArenaTeam->GetId() >= MAX_ARENA_TEAM_ID)
        {
            sArenaTeamMgr->RemoveArenaTeam(tempAlliArenaTeam->GetId());
            delete tempAlliArenaTeam;
        }

        if (tempHordeArenaTeam && tempHordeArenaTeam->GetId() >= MAX_ARENA_TEAM_ID)
        {
            sArenaTeamMgr->RemoveArenaTeam(tempHordeArenaTeam->GetId());
            delete tempHordeArenaTeam;
        }
    }
}

void Solo3v3::CheckStartSolo3v3Arena(Battleground* bg)
{
    bool someoneNotInArena = false;
    uint32 PlayersInArena = 0;

    for (const auto& playerPair : bg->GetPlayers())
    {
        Player* player = playerPair.second;

        if (!player)
            continue;

        // prevent crash with Arena Replay module
        if (player->IsSpectator())
            return;

        PlayersInArena++;
    }

    uint32 AmountPlayersSolo3v3 = 6;
    if (PlayersInArena < AmountPlayersSolo3v3)
    {
        someoneNotInArena = true;
    }

    // if one player didn't enter arena and StopGameIncomplete is true, then end arena
    if (someoneNotInArena && sConfigMgr->GetOption<bool>("Solo.3v3.StopGameIncomplete", true))
    {
        bg->SetRated(false);
        bg->EndBattleground(TEAM_NEUTRAL);
    }
}

uint32 Solo3v3::GetMMR(Player* player, GroupQueueInfo* ginfo)
{
    if (ginfo->ArenaMatchmakerRating > 0)
        return ginfo->ArenaMatchmakerRating;

    ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3));
    if (!at)
        return sConfigMgr->GetOption<uint32>("Arena.ArenaStartPersonalRating", 0);

    for (auto const& m : at->GetMembers())
        if (m.Guid == player->GetGUID())
            return m.MatchMakerRating > 0 ? m.MatchMakerRating : at->GetRating();

    return at->GetRating();
}

int Solo3v3::CountIgnorePairs(std::vector<uint32> const& indices, std::vector<Candidate> const& selected, bool avoidIgnore)
{
    if (!avoidIgnore)
        return 0;

    int pairs = 0;
    for (uint32 i = 0; i < indices.size(); ++i)
        for (uint32 j = i + 1; j < indices.size(); ++j)
        {
            Player* a = selected[indices[i]].player;
            Player* b = selected[indices[j]].player;
            if (a->GetSocial()->HasIgnore(b->GetGUID()) ||
                b->GetSocial()->HasIgnore(a->GetGUID()))
                ++pairs;
        }
    return pairs;
}

void Solo3v3::EnumerateCombinations(
    uint32 start,
    uint32 depth,
    std::vector<uint32>& combo,
    std::vector<Candidate> const& selected,
    uint32 teamSize,
    uint32 n,
    bool filterTalents,
    bool allDpsMatch,
    bool avoidIgnore,
    std::vector<uint32>& bestTeam1,
    bool& haveBest,
    uint64& bestDiff,
    int& bestIgnores)
{
    if (depth == teamSize)
    {
        // Build team2 as the complement of combo within [0, n)
        std::vector<uint32> team2;
        uint32 ci = 0;
        for (uint32 i = 0; i < n; ++i)
        {
            if (ci < teamSize && combo[ci] == i) { ++ci; continue; }
            team2.push_back(i);
        }

        // Composition validation (filterTalents only)
        if (filterTalents)
        {
            uint32 h1 = 0, h2 = 0;
            for (uint32 i : combo) if (selected[i].role == HEALER) ++h1;
            for (uint32 i : team2) if (selected[i].role == HEALER) ++h2;

            if (allDpsMatch  && (h1 != 0 || h2 != 0)) return;
            if (!allDpsMatch && (h1 != 1 || h2 != 1)) return;
        }

        // MMR balance score
        int64 sum1 = 0, sum2 = 0;
        for (uint32 i : combo) sum1 += selected[i].mmr;
        for (uint32 i : team2) sum2 += selected[i].mmr;
        uint64 diff = static_cast<uint64>(sum1 > sum2 ? sum1 - sum2 : sum2 - sum1);

        // Ignore-pair count as tie-breaker
        int ign = CountIgnorePairs(combo, selected, avoidIgnore) + CountIgnorePairs(team2, selected, avoidIgnore);

        if (!haveBest || diff < bestDiff || (diff == bestDiff && ign < bestIgnores))
        {
            haveBest   = true;
            bestDiff   = diff;
            bestIgnores = ign;
            bestTeam1.assign(combo.begin(), combo.end());
        }
        return;
    }

    for (uint32 i = start; i <= n - (teamSize - depth); ++i)
    {
        combo[depth] = i;
        EnumerateCombinations(i + 1, depth + 1, combo, selected, teamSize, n, filterTalents, allDpsMatch, avoidIgnore, bestTeam1, haveBest, bestDiff, bestIgnores);
    }
}

void Solo3v3::AssignToPool(
    std::vector<uint32> const& indices,
    std::vector<Candidate> const& selected,
    uint32 poolTeam,
    BattlegroundQueue* queue,
    BattlegroundBracketId bracket_id,
    uint8 allianceGroupType,
    uint8 hordeGroupType,
    uint32 MinPlayers)
{
    uint8 const targetGroupType = (poolTeam == TEAM_ALLIANCE) ? allianceGroupType : hordeGroupType;
    for (uint32 idx : indices)
    {
        Candidate const& c = selected[idx];

        if (c.group->teamId != static_cast<TeamId>(poolTeam))
        {
            uint8 const srcGroupType = (c.group->teamId == TEAM_ALLIANCE) ? allianceGroupType : hordeGroupType;

            c.group->teamId    = static_cast<TeamId>(poolTeam);
            c.group->GroupType = targetGroupType;

            // Re-insert into destination bucket in JoinTime order to preserve FIFO fairness
            auto& dstList = queue->m_QueuedGroups[bracket_id][targetGroupType];
            auto& srcList = queue->m_QueuedGroups[bracket_id][srcGroupType];

            auto insertPos = dstList.begin();
            while (insertPos != dstList.end() && (*insertPos)->JoinTime <= c.group->JoinTime)
                ++insertPos;

            dstList.insert(insertPos, c.group);
            srcList.erase(std::find(srcList.begin(), srcList.end(), c.group));
        }

        queue->m_SelectionPools[poolTeam].AddGroup(c.group, MinPlayers);
    }
}

bool Solo3v3::CheckSolo3v3Arena(BattlegroundQueue* queue, BattlegroundBracketId bracket_id, bool isRated)
{
    queue->m_SelectionPools[TEAM_ALLIANCE].Init();
    queue->m_SelectionPools[TEAM_HORDE].Init();

    uint32 const MinPlayers      = sBattlegroundMgr->isArenaTesting() ? 1 : 3;
    bool   const filterTalents   = sConfigMgr->GetOption<bool>("Solo.3v3.FilterTalents", false);
    bool   const avoidIgnore     = sConfigMgr->GetOption<bool>("Solo.3v3.AvoidSameTeamIgnore", true);
    uint32 const allDpsTimerMs   = sConfigMgr->GetOption<uint32>("Solo.3v3.FilterTalents.AllDPSTimer", 60) * 1000;

    uint8 const allianceGroupType = isRated ? BG_QUEUE_PREMADE_ALLIANCE : BG_QUEUE_NORMAL_ALLIANCE;
    uint8 const hordeGroupType    = isRated ? BG_QUEUE_PREMADE_HORDE    : BG_QUEUE_NORMAL_HORDE;

    uint32 const now = GameTime::GetGameTimeMS().count();

    // === Phase 1: collect all eligible candidates in queue order (FIFO) ===
    std::vector<Candidate> allCandidates;
    for (int t = 0; t < 2; ++t)
    {
        int idx = t + (isRated ? 0 : PVP_TEAMS_COUNT);
        for (auto const& g : queue->m_QueuedGroups[bracket_id][idx])
        {
            if (g->IsInvitedToBGInstanceGUID)
                continue;

            for (auto const& guid : g->Players)
            {
                Player* plr = ObjectAccessor::FindPlayer(guid);
                if (!plr)
                    continue;

                Solo3v3TalentCat role = filterTalents ? GetTalentCatForSolo3v3(plr) : MELEE;
                allCandidates.push_back({g, plr, role, GetMMR(plr, g)});
                break; // solo queue: exactly one player per group
            }
        }
    }

    if (allCandidates.size() < MinPlayers * 2)
        return false;

    // === Phase 2: select candidates that form a valid match (composition-aware, FIFO) ===
    std::vector<Candidate> selected;
    bool allDpsMatch = false;

    if (!filterTalents)
    {
        // No role filtering: take the first MinPlayers*2 players
        selected.assign(allCandidates.begin(), allCandidates.begin() + MinPlayers * 2);
    }
    else
    {
        std::vector<Candidate> healers, dps;
        for (auto& c : allCandidates)
        {
            if (c.role == HEALER) healers.push_back(c);
            else                  dps.push_back(c);
        }

        // For MinPlayers==1 (arena testing) no healer requirement; otherwise 1 healer per team.
        uint32 const healersNeeded = (MinPlayers > 1) ? 2 : 0;
        uint32 const dpsNeeded     = MinPlayers * 2 - healersNeeded;

        if (healers.size() >= healersNeeded && dps.size() >= dpsNeeded)
        {
            // Standard: take oldest healers and oldest DPS (FIFO within each role bucket)
            for (uint32 i = 0; i < healersNeeded; ++i) selected.push_back(healers[i]);
            for (uint32 i = 0; i < dpsNeeded;     ++i) selected.push_back(dps[i]);
        }
        else if (healers.empty())
        {
            // All-DPS fallback: only include DPS players whose wait timer has elapsed
            std::vector<Candidate> timedDps;
            for (auto& c : dps)
                if (c.group->JoinTime + allDpsTimerMs <= now)
                    timedDps.push_back(c);

            if (timedDps.size() >= MinPlayers * 2)
            {
                for (uint32 i = 0; i < MinPlayers * 2; ++i)
                    selected.push_back(timedDps[i]);
                allDpsMatch = true;
            }
        }
        // 1 healer present: unbalanced composition, cannot form a valid match — fall through.
    }

    if (selected.size() < MinPlayers * 2)
        return false;

    // === Phase 3: exhaustive search for the MMR-balanced team split ===
    // For 6 players / teamSize=3: C(6,3)=20 combinations — negligible overhead.
    // Primary:   minimise |sum_mmr_team1 - sum_mmr_team2|
    // Secondary: minimise mutual-ignore pairs within teams (avoidIgnore tie-breaker)
    uint32 const n        = static_cast<uint32>(selected.size());
    uint32 const teamSize = MinPlayers;

    std::vector<uint32> bestTeam1;
    bool     haveBest   = false;
    uint64   bestDiff   = 0;
    int      bestIgnores = 0;

    std::vector<uint32> combo(teamSize);
    EnumerateCombinations(0, 0, combo, selected, teamSize, n, filterTalents, allDpsMatch, avoidIgnore, bestTeam1, haveBest, bestDiff, bestIgnores);

    if (!haveBest)
        return false;

    // Build team2 as complement of bestTeam1
    std::vector<uint32> team2Indices;
    {
        uint32 ci = 0;
        for (uint32 i = 0; i < n; ++i)
        {
            if (ci < static_cast<uint32>(bestTeam1.size()) && bestTeam1[ci] == i) { ++ci; continue; }
            team2Indices.push_back(i);
        }
    }

    // === Phase 4: assign to selection pools, reclassifying faction bucket if needed ===
    AssignToPool(bestTeam1,    selected, TEAM_ALLIANCE, queue, bracket_id, allianceGroupType, hordeGroupType, MinPlayers);
    AssignToPool(team2Indices, selected, TEAM_HORDE,    queue, bracket_id, allianceGroupType, hordeGroupType, MinPlayers);

    return true;
}

void Solo3v3::CreateTempArenaTeamForQueue(BattlegroundQueue* queue, ArenaTeam* arenaTeams[])
{
    // Create temp arena team
    for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
    {
        ArenaTeam* tempArenaTeam = new ArenaTeam();  // delete it when all players have left the arena match. Stored in sArenaTeamMgr
        std::vector<Player*> playersList;
        uint32 atPlrItr = 0;

        for (auto const& itr : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
        {
            if (atPlrItr >= 3)
                break; // Should never happen

            for (auto const& itr2 : itr->Players)
            {
                auto _PlayerGuid = itr2;
                if (Player * _player = ObjectAccessor::FindPlayer(_PlayerGuid))
                {
                    playersList.push_back(_player);
                    atPlrItr++;
                }

                break;
            }
        }

        std::stringstream ssTeamName;
        ssTeamName << "Solo Team - " << (i + 1);

        tempArenaTeam->CreateTempArenaTeam(playersList, ARENA_TYPE_3v3_SOLO, ssTeamName.str());
        sArenaTeamMgr->AddArenaTeam(tempArenaTeam);
        arenaTeams[i] = tempArenaTeam;
    }
}

bool Solo3v3::Arena3v3CheckTalents(Player* player)
{
    if (!player)
        return false;

    if (!sConfigMgr->GetOption<bool>("Arena.3v3.BlockForbiddenTalents", false))
        return true;

    uint32 count = 0;
    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] == 0)
                continue;

            if (player->HasTalent(talentInfo->RankID[rank], player->GetActiveSpec()))
            {
                for (int8 i = 0; FORBIDDEN_TALENTS_IN_1V1_ARENA[i] != 0; i++)
                    if (FORBIDDEN_TALENTS_IN_1V1_ARENA[i] == talentInfo->TalentTab)
                        count += rank + 1;
            }
        }
    }

    if (count >= 36)
    {
        ChatHandler(player->GetSession()).SendSysMessage("You can't join, because you have invested to much points in a forbidden talent. Please edit your talents.");
        return false;
    }

    return true;
}

Solo3v3TalentCat Solo3v3::GetTalentCatForSolo3v3(Player* player)
{
    uint32 count[MAX_TALENT_CAT];

    for (int i = 0; i < MAX_TALENT_CAT; i++)
        count[i] = 0;

    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] == 0)
                continue;

            if (player->HasTalent(talentInfo->RankID[rank], player->GetActiveSpec()))
            {
                for (int8 i = 0; SOLO_3V3_TALENTS_MELEE[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_MELEE[i] == talentInfo->TalentTab)
                        count[MELEE] += rank + 1;

                for (int8 i = 0; SOLO_3V3_TALENTS_RANGE[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_RANGE[i] == talentInfo->TalentTab)
                        count[RANGE] += rank + 1;

                for (int8 i = 0; SOLO_3V3_TALENTS_HEAL[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_HEAL[i] == talentInfo->TalentTab)
                        count[HEALER] += rank + 1;
            }
        }
    }

    uint32 prevCount = 0;

    Solo3v3TalentCat talCat = MELEE; // Default MELEE (if no talent points set)

    for (int i = 0; i < MAX_TALENT_CAT; i++)
    {
        if (count[i] > prevCount)
        {
            talCat = (Solo3v3TalentCat)i;
            prevCount = count[i];
        }
    }

    return talCat;
}

Solo3v3TalentCat Solo3v3::GetFirstAvailableSlot(bool soloTeam[][MAX_TALENT_CAT]) {
    if (!soloTeam[0][MELEE] || !soloTeam[1][MELEE])
        return MELEE;

    if (!soloTeam[0][RANGE] || !soloTeam[1][RANGE])
        return RANGE;

    if (!soloTeam[0][HEALER] || !soloTeam[1][HEALER])
        return HEALER;

    return MELEE;
}

bool Solo3v3::HasIgnoreConflict(Player* candidate, BattlegroundQueue* queue, uint32 teamId)
{
    for (auto const& group : queue->m_SelectionPools[teamId].SelectedGroups)
    {
        for (auto const& existingGuid : group->Players)
        {
            Player* existingPlayer = ObjectAccessor::FindPlayer(existingGuid);
            if (!existingPlayer)
                continue;

            if (candidate->GetSocial()->HasIgnore(existingGuid) ||
                existingPlayer->GetSocial()->HasIgnore(candidate->GetGUID()))
                return true;
        }
    }
    return false;
}

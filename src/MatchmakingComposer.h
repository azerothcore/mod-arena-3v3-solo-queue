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

#ifndef _MATCHMAKING_COMPOSER_H_
#define _MATCHMAKING_COMPOSER_H_

#include <algorithm>
#include <cstdint>
#include <vector>

/// Simplified role enum for matchmaking composition logic.
/// DPS covers both MELEE and RANGE from Solo3v3TalentCat.
enum class PlayerRole : uint8_t
{
    DPS    = 0,
    HEALER = 1,
};

/// Lightweight, WoW-server-independent representation of a queued candidate.
/// Mirrors the private Candidate struct in Solo3v3 but with no game-engine types.
struct QueuedCandidate
{
    uint32_t   id;       ///< Unique identifier (player GUID equivalent)
    PlayerRole role;     ///< Detected talent category (DPS or HEALER)
    uint32_t   mmr;      ///< Current matchmaker rating
    uint32_t   joinTime; ///< Queue join timestamp in ms (for FIFO ordering)
};

/// Result of FindBestTeamSplit(): indices into the selected candidate vector.
struct TeamSplitResult
{
    bool                 valid = false;
    std::vector<uint32_t> team1Indices; ///< Indices for team 1 (Alliance)
    std::vector<uint32_t> team2Indices; ///< Indices for team 2 (Horde)
    uint64_t             mmrDiff = 0;  ///< |sum_mmr_team1 - sum_mmr_team2|
};

/// Standalone implementation of the 3v3 solo queue Phase-2 candidate selection
/// and Phase-3 exhaustive MMR-balancing team split.
///
/// This class mirrors the logic in Solo3v3::CheckSolo3v3Arena and
/// Solo3v3::EnumerateCombinations without any dependency on WoW server types,
/// making it fully unit-testable.
class MatchmakingComposer
{
public:
    /// Phase 2 — Select a valid set of candidates for a single match.
    ///
    /// When @p filterTalents is true the selection enforces role-based
    /// composition: normal path requires exactly 2 healers + 4 DPS (for
    /// teamSize 3). If no healers are present and every DPS player's wait time
    /// has exceeded @p allDpsTimer an all-DPS match is allowed instead.
    /// A queue with exactly 1 healer cannot form a valid match.
    ///
    /// @param candidates    All eligible queued candidates in FIFO order.
    /// @param teamSize      Players per team (normally 3).
    /// @param filterTalents Enforce role-based composition rules.
    /// @param allDpsTimer   Wait time (ms) a DPS player must have queued before
    ///                      an all-DPS fallback match is allowed.
    /// @param now           Current timestamp in ms.
    /// @param[out] selected Chosen candidates (size == teamSize*2 on success).
    /// @param[out] allDpsMatch Set to true when the all-DPS fallback is used.
    /// @returns true if a full set of candidates was selected.
    bool SelectCandidates(
        std::vector<QueuedCandidate> const& candidates,
        uint32_t                            teamSize,
        bool                                filterTalents,
        uint32_t                            allDpsTimer,
        uint32_t                            now,
        std::vector<QueuedCandidate>&       selected,
        bool&                               allDpsMatch) const
    {
        selected.clear();
        allDpsMatch = false;

        if (candidates.size() < teamSize * 2)
            return false;

        if (!filterTalents)
        {
            // No role filtering: take the first teamSize*2 players (FIFO)
            selected.assign(candidates.begin(), candidates.begin() + teamSize * 2);
            return true;
        }

        // Separate into role buckets (preserving FIFO order within each bucket)
        std::vector<QueuedCandidate> healers, dps;
        for (auto const& c : candidates)
        {
            if (c.role == PlayerRole::HEALER)
                healers.push_back(c);
            else
                dps.push_back(c);
        }

        // For teamSize > 1: need 1 healer per team  =>  2 healers total
        uint32_t const healersNeeded = (teamSize > 1) ? 2 : 0;
        uint32_t const dpsNeeded     = teamSize * 2 - healersNeeded;

        if (healers.size() >= healersNeeded && dps.size() >= dpsNeeded)
        {
            // Standard path: oldest healers + oldest DPS (FIFO within each bucket)
            for (uint32_t i = 0; i < healersNeeded; ++i)
                selected.push_back(healers[i]);
            for (uint32_t i = 0; i < dpsNeeded; ++i)
                selected.push_back(dps[i]);
            return true;
        }
        else if (healers.empty())
        {
            // All-DPS fallback: only include DPS players whose timer has elapsed
            std::vector<QueuedCandidate> timedDps;
            for (auto const& c : dps)
                if (c.joinTime + allDpsTimer <= now)
                    timedDps.push_back(c);

            if (timedDps.size() >= teamSize * 2)
            {
                for (uint32_t i = 0; i < teamSize * 2; ++i)
                    selected.push_back(timedDps[i]);
                allDpsMatch = true;
                return true;
            }
        }
        // Exactly 1 healer: unbalanced composition — cannot form a valid match

        return false;
    }

    /// Phase 3 — Exhaustive search for the best MMR-balanced team split.
    ///
    /// Enumerates all C(n, teamSize) combinations. For each combination the
    /// complement forms the opposing team. The split that minimises
    /// |sum_mmr_team1 - sum_mmr_team2| while satisfying the role constraint
    /// (exactly 1 healer per team when filterTalents && !allDpsMatch, no
    /// healers on either team when allDpsMatch) is returned.
    ///
    /// @param selected      Candidates to split (size must equal teamSize*2).
    /// @param teamSize      Players per team.
    /// @param filterTalents Enforce composition constraints.
    /// @param allDpsMatch   When true, no healers are allowed on either team.
    /// @returns TeamSplitResult with the optimal split, or !valid if none found.
    TeamSplitResult FindBestTeamSplit(
        std::vector<QueuedCandidate> const& selected,
        uint32_t                            teamSize,
        bool                                filterTalents,
        bool                                allDpsMatch) const
    {
        TeamSplitResult result;
        uint32_t const  n = static_cast<uint32_t>(selected.size());

        if (n < teamSize * 2)
            return result;

        bool                 haveBest = false;
        uint64_t             bestDiff = 0;
        std::vector<uint32_t> bestTeam1;
        std::vector<uint32_t> combo(teamSize);

        Enumerate(0, 0, combo, selected, teamSize, n,
                  filterTalents, allDpsMatch,
                  bestTeam1, haveBest, bestDiff);

        if (!haveBest)
            return result;

        result.valid       = true;
        result.mmrDiff     = bestDiff;
        result.team1Indices = bestTeam1;

        // Build team2 as the complement of team1
        uint32_t ci = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            if (ci < static_cast<uint32_t>(bestTeam1.size()) && bestTeam1[ci] == i)
            {
                ++ci;
                continue;
            }
            result.team2Indices.push_back(i);
        }

        return result;
    }

private:
    void Enumerate(
        uint32_t                            start,
        uint32_t                            depth,
        std::vector<uint32_t>&              combo,
        std::vector<QueuedCandidate> const& selected,
        uint32_t                            teamSize,
        uint32_t                            n,
        bool                                filterTalents,
        bool                                allDpsMatch,
        std::vector<uint32_t>&              bestTeam1,
        bool&                               haveBest,
        uint64_t&                           bestDiff) const
    {
        if (depth == teamSize)
        {
            // Build team2 as complement of combo within [0, n)
            std::vector<uint32_t> team2;
            uint32_t ci = 0;
            for (uint32_t i = 0; i < n; ++i)
            {
                if (ci < teamSize && combo[ci] == i)
                {
                    ++ci;
                    continue;
                }
                team2.push_back(i);
            }

            // Composition constraint (mirrors EnumerateCombinations in solo3v3.cpp)
            if (filterTalents)
            {
                uint32_t h1 = 0, h2 = 0;
                for (uint32_t i : combo)
                    if (selected[i].role == PlayerRole::HEALER) ++h1;
                for (uint32_t i : team2)
                    if (selected[i].role == PlayerRole::HEALER) ++h2;

                if (allDpsMatch  && (h1 != 0 || h2 != 0)) return;
                if (!allDpsMatch && (h1 != 1 || h2 != 1)) return;
            }

            // MMR balance score
            int64_t sum1 = 0, sum2 = 0;
            for (uint32_t i : combo) sum1 += selected[i].mmr;
            for (uint32_t i : team2) sum2 += selected[i].mmr;
            uint64_t diff = static_cast<uint64_t>(sum1 > sum2 ? sum1 - sum2 : sum2 - sum1);

            if (!haveBest || diff < bestDiff)
            {
                haveBest  = true;
                bestDiff  = diff;
                bestTeam1.assign(combo.begin(), combo.end());
            }
            return;
        }

        for (uint32_t i = start; i <= n - (teamSize - depth); ++i)
        {
            combo[depth] = i;
            Enumerate(i + 1, depth + 1, combo, selected, teamSize, n,
                      filterTalents, allDpsMatch,
                      bestTeam1, haveBest, bestDiff);
        }
    }
};

#endif // _MATCHMAKING_COMPOSER_H_

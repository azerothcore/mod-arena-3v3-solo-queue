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

#include "gtest/gtest.h"
#include "MatchmakingComposer.h"

#include <algorithm>
#include <numeric>

/// Test fixture for 3v3 solo queue matchmaking composition tests.
class MatchmakingTest : public ::testing::Test
{
protected:
    MatchmakingComposer composer;

    /// Default configuration constants (mirrors production config defaults).
    static constexpr uint32_t TEAM_SIZE      = 3;      // MinPlayers
    static constexpr uint32_t ALL_DPS_TIMER  = 60000;  // 60 s in ms
    static constexpr uint32_t DEFAULT_MMR    = 1500;

    // ── Helpers ──────────────────────────────────────────────────────────────

    /// Build a candidate list from (role, mmr) pairs.
    /// @p baseJoinTime is applied to all entries. classId defaults to 0.
    std::vector<QueuedCandidate> MakeCandidates(
        std::vector<std::pair<PlayerRole, uint32_t>> const& specs,
        uint32_t baseJoinTime = 0) const
    {
        std::vector<QueuedCandidate> out;
        uint32_t id = 1;
        for (auto const& [role, mmr] : specs)
            out.push_back({id++, role, mmr, baseJoinTime, 0});
        return out;
    }

    /// Build a candidate list from (role, mmr, classId) tuples.
    std::vector<QueuedCandidate> MakeCandidatesWithClass(
        std::vector<std::tuple<PlayerRole, uint32_t, uint8_t>> const& specs,
        uint32_t baseJoinTime = 0) const
    {
        std::vector<QueuedCandidate> out;
        uint32_t id = 1;
        for (auto const& [role, mmr, cls] : specs)
            out.push_back({id++, role, mmr, baseJoinTime, cls});
        return out;
    }

    /// Count how many unique class IDs appear in the given team.
    uint32_t CountUniqueClasses(
        std::vector<uint32_t> const&         indices,
        std::vector<QueuedCandidate> const&  pool) const
    {
        std::vector<uint8_t> classes;
        for (uint32_t i : indices)
            classes.push_back(pool[i].classId);
        std::sort(classes.begin(), classes.end());
        classes.erase(std::unique(classes.begin(), classes.end()), classes.end());
        return static_cast<uint32_t>(classes.size());
    }

    /// Returns true if a team contains two candidates with the same class ID.
    bool TeamHasDuplicateClass(
        std::vector<uint32_t> const&         indices,
        std::vector<QueuedCandidate> const&  pool) const
    {
        for (uint32_t i = 0; i < indices.size(); ++i)
            for (uint32_t j = i + 1; j < indices.size(); ++j)
                if (pool[indices[i]].classId == pool[indices[j]].classId &&
                    pool[indices[i]].classId != 0)
                    return true;
        return false;
    }

    /// Count how many candidates in @p indices are healers.
    uint32_t CountHealers(
        std::vector<uint32_t> const&         indices,
        std::vector<QueuedCandidate> const&  pool) const
    {
        uint32_t n = 0;
        for (uint32_t i : indices)
            if (pool[i].role == PlayerRole::HEALER) ++n;
        return n;
    }

    /// Sum the MMR of candidates at the given @p indices.
    uint32_t SumMMR(
        std::vector<uint32_t> const&         indices,
        std::vector<QueuedCandidate> const&  pool) const
    {
        uint32_t sum = 0;
        for (uint32_t i : indices)
            sum += pool[i].mmr;
        return sum;
    }

    /// Run the full two-phase pipeline and return the split result.
    TeamSplitResult RunFullPipeline(
        std::vector<QueuedCandidate> const& candidates,
        bool                                filterTalents,
        uint32_t                            allDpsTimer = ALL_DPS_TIMER,
        uint32_t                            now         = 100000) const
    {
        std::vector<QueuedCandidate> selected;
        bool allDpsMatch = false;

        if (!composer.SelectCandidates(candidates, TEAM_SIZE, filterTalents,
                                       allDpsTimer, now, selected, allDpsMatch))
            return {};

        return composer.FindBestTeamSplit(selected, TEAM_SIZE, filterTalents, allDpsMatch);
    }
};

// ── Phase 2: SelectCandidates ─────────────────────────────────────────────────

/// Test 1: With 2 healers and 4 DPS in queue, selection picks all 6 players
/// (2 healers + 4 DPS) and does not trigger the all-DPS fallback.
TEST_F(MatchmakingTest, SelectCandidates_TwoHealersFourDPS_SelectsSixPlayers)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, DEFAULT_MMR}, {PlayerRole::HEALER, DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
    });

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_TRUE(ok);
    EXPECT_EQ(selected.size(), 6u);
    EXPECT_FALSE(allDpsMatch);

    uint32_t healerCount = static_cast<uint32_t>(
        std::count_if(selected.begin(), selected.end(),
                      [](auto const& c){ return c.role == PlayerRole::HEALER; }));
    EXPECT_EQ(healerCount, 2u) << "Selection must include exactly 2 healers";
}

/// Test 2: When fewer than 6 players are queued, no match can be formed.
TEST_F(MatchmakingTest, SelectCandidates_InsufficientPlayers_ReturnsFalse)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, // only 4 players — need 6
    });

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_FALSE(ok) << "Must fail when fewer than 6 players are queued";
}

/// Test 3: With exactly 1 healer and 5 DPS the composition is unbalanced;
/// no valid match can be formed (cannot assign 1 healer to each team).
TEST_F(MatchmakingTest, SelectCandidates_ExactlyOneHealer_UnbalancedComposition_ReturnsFalse)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR},
    });

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_FALSE(ok) << "1 healer cannot be distributed fairly between two teams";
}

/// Test 4: When there are no healers and all DPS players have waited long
/// enough, the all-DPS fallback activates and a match is formed.
TEST_F(MatchmakingTest, SelectCandidates_AllDPS_FallbackAfterTimer_Succeeds)
{
    uint32_t const joinTime = 0;
    uint32_t const now      = 65000; // 65 s elapsed — past the 60 s timer
    uint32_t const timer    = ALL_DPS_TIMER;

    auto candidates = MakeCandidates({
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
    }, joinTime);

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        timer, now, selected, allDpsMatch);

    EXPECT_TRUE(ok)         << "All-DPS match must be allowed after timer expires";
    EXPECT_TRUE(allDpsMatch) << "allDpsMatch flag must be set for an all-DPS match";
    EXPECT_EQ(selected.size(), 6u);
}

/// Test 5: When no healers are present but the DPS players have not waited
/// long enough, the all-DPS fallback must be blocked.
TEST_F(MatchmakingTest, SelectCandidates_AllDPS_BlockedBeforeTimer)
{
    uint32_t const joinTime = 0;
    uint32_t const now      = 30000; // only 30 s — timer needs 60 s
    uint32_t const timer    = ALL_DPS_TIMER;

    auto candidates = MakeCandidates({
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
    }, joinTime);

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        timer, now, selected, allDpsMatch);

    EXPECT_FALSE(ok) << "All-DPS match must be blocked before timer expires";
}

/// Test 6: When filterTalents is disabled any 6 players form a match
/// regardless of their roles (even all DPS without a timer wait).
TEST_F(MatchmakingTest, SelectCandidates_FilterTalentsDisabled_IgnoresRoles)
{
    auto candidates = MakeCandidates({
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
        {PlayerRole::DPS, DEFAULT_MMR}, {PlayerRole::DPS, DEFAULT_MMR},
    });

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, false,
                                        ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_TRUE(ok);
    EXPECT_EQ(selected.size(), 6u);
    EXPECT_FALSE(allDpsMatch);
}

/// Test 7: FIFO ordering — the oldest queued players within each role bucket
/// are selected first; later joiners are excluded when there are extras.
TEST_F(MatchmakingTest, SelectCandidates_FIFOOrder_OldestPlayersPickedFirst)
{
    // Players 1-2 are the earliest healers; players 3-6 are the earliest DPS.
    // Players 7-8 join later and must NOT be selected.
    std::vector<QueuedCandidate> candidates = {
        {1, PlayerRole::HEALER, DEFAULT_MMR, 0},
        {2, PlayerRole::HEALER, DEFAULT_MMR, 100},
        {3, PlayerRole::DPS,    DEFAULT_MMR, 200},
        {4, PlayerRole::DPS,    DEFAULT_MMR, 300},
        {5, PlayerRole::DPS,    DEFAULT_MMR, 400},
        {6, PlayerRole::DPS,    DEFAULT_MMR, 500},
        // Extras — should never be selected
        {7, PlayerRole::DPS,    DEFAULT_MMR, 600},
        {8, PlayerRole::DPS,    DEFAULT_MMR, 700},
    };

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_TRUE(ok);
    ASSERT_EQ(selected.size(), 6u);

    for (auto const& c : selected)
    {
        EXPECT_NE(c.id, 7u) << "Player 7 (late joiner) must not be selected";
        EXPECT_NE(c.id, 8u) << "Player 8 (late joiner) must not be selected";
    }

    bool found1 = std::any_of(selected.begin(), selected.end(),
                               [](auto const& c){ return c.id == 1; });
    EXPECT_TRUE(found1) << "Player 1 (earliest healer) must be selected first";
}

// ── Phase 3: FindBestTeamSplit ────────────────────────────────────────────────

/// Test 8: The fundamental composition invariant — each team must have exactly
/// 1 healer and 2 DPS when filterTalents is enabled.
TEST_F(MatchmakingTest, FindBestTeamSplit_EachTeamHasExactlyOneHealerAndTwoDPS)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, DEFAULT_MMR}, {PlayerRole::HEALER, DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
    });

    auto result = RunFullPipeline(candidates, true);

    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.team1Indices.size(), 3u);
    ASSERT_EQ(result.team2Indices.size(), 3u);

    auto selected = candidates; // SelectCandidates takes all 6 in this case
    EXPECT_EQ(CountHealers(result.team1Indices, selected), 1u)
        << "Team 1 must have exactly 1 healer";
    EXPECT_EQ(CountHealers(result.team2Indices, selected), 1u)
        << "Team 2 must have exactly 1 healer";
}

/// Test 9: The two healers must always end up on different teams.
/// Runs with asymmetric MMR to ensure the optimizer does not accidentally
/// put both healers together even when their MMR differs significantly.
TEST_F(MatchmakingTest, FindBestTeamSplit_HealersAreAlwaysOnDifferentTeams)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, 1600}, {PlayerRole::HEALER, 1400},
        {PlayerRole::DPS,    1550}, {PlayerRole::DPS,    1450},
        {PlayerRole::DPS,    1500}, {PlayerRole::DPS,    1500},
    });

    auto result = RunFullPipeline(candidates, true);

    ASSERT_TRUE(result.valid);

    // Re-run SelectCandidates to obtain the same 'selected' vector
    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    composer.SelectCandidates(candidates, TEAM_SIZE, true,
                               ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    uint32_t h1 = CountHealers(result.team1Indices, selected);
    uint32_t h2 = CountHealers(result.team2Indices, selected);

    EXPECT_EQ(h1, 1u) << "Team 1 must have exactly 1 healer";
    EXPECT_EQ(h2, 1u) << "Team 2 must have exactly 1 healer";
}

/// Test 10: When all six players share the same MMR the resulting teams must
/// have an MMR difference of zero (perfect balance).
TEST_F(MatchmakingTest, FindBestTeamSplit_EqualMMR_PerfectBalance)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, DEFAULT_MMR}, {PlayerRole::HEALER, DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
        {PlayerRole::DPS,    DEFAULT_MMR}, {PlayerRole::DPS,    DEFAULT_MMR},
    });

    auto result = RunFullPipeline(candidates, true);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.mmrDiff, 0u)
        << "Teams built from equal-MMR players must have zero MMR difference";
}

/// Test 11: With healers that differ by 1000 MMR and uniform DPS the minimum
/// achievable difference — given the 1-healer-per-team constraint — is 1000.
/// Verifies that the optimizer correctly reports this lower bound.
TEST_F(MatchmakingTest, FindBestTeamSplit_LargeHealerMMRSpread_MinDiffIs1000)
{
    // H1=2000, H2=1000, four DPS at 1500
    // Any valid split: one team gets H2000 + two 1500-DPS = 5000
    //                  other team gets H1000 + two 1500-DPS = 4000  → diff = 1000
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, 2000}, {PlayerRole::HEALER, 1000},
        {PlayerRole::DPS,    1500}, {PlayerRole::DPS,    1500},
        {PlayerRole::DPS,    1500}, {PlayerRole::DPS,    1500},
    });

    auto result = RunFullPipeline(candidates, true);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.mmrDiff, 1000u)
        << "Minimum possible diff with 1000-point healer spread must be 1000";

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    composer.SelectCandidates(candidates, TEAM_SIZE, true,
                               ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    EXPECT_EQ(CountHealers(result.team1Indices, selected), 1u);
    EXPECT_EQ(CountHealers(result.team2Indices, selected), 1u);
}

/// Test 12: The algorithm finds the optimal DPS distribution that minimises
/// the total MMR gap across all composition-valid splits.
TEST_F(MatchmakingTest, FindBestTeamSplit_OptimalDPSDistribution_MinimisesMMRDiff)
{
    // H1=1500, H2=1500, DPS: 1700, 1300, 1500, 1500
    // Optimal: [H1500 + D1700 + D1300 = 4500] vs [H1500 + D1500 + D1500 = 4500] → diff = 0
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, 1500}, {PlayerRole::HEALER, 1500},
        {PlayerRole::DPS,    1700}, {PlayerRole::DPS,    1300},
        {PlayerRole::DPS,    1500}, {PlayerRole::DPS,    1500},
    });

    auto result = RunFullPipeline(candidates, true);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.mmrDiff, 0u)
        << "Symmetric DPS distribution should allow a perfectly balanced split";
}

/// Test 13: Verifies the MMR difference across both teams never exceeds what
/// would arise from a naive (first-three vs last-three) split.
/// The optimizer must do better than or equal to any arbitrary split.
TEST_F(MatchmakingTest, FindBestTeamSplit_NeverWorseThanNaiveSplit)
{
    auto candidates = MakeCandidates({
        {PlayerRole::HEALER, 1800}, {PlayerRole::HEALER, 1200},
        {PlayerRole::DPS,    1700}, {PlayerRole::DPS,    1600},
        {PlayerRole::DPS,    1400}, {PlayerRole::DPS,    1300},
    });

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    composer.SelectCandidates(candidates, TEAM_SIZE, true,
                               ALL_DPS_TIMER, 100000, selected, allDpsMatch);

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, allDpsMatch);
    ASSERT_TRUE(result.valid);

    // Naive split [indices 0,1,2] vs [3,4,5] regardless of composition:
    // Just compute diff for comparison — the optimizer must do at least as well.
    uint32_t naiveSum1 = SumMMR({0, 1, 2}, selected);
    uint32_t naiveSum2 = SumMMR({3, 4, 5}, selected);
    uint64_t naiveDiff = naiveSum1 > naiveSum2 ? naiveSum1 - naiveSum2
                                               : naiveSum2 - naiveSum1;

    EXPECT_LE(result.mmrDiff, naiveDiff)
        << "Optimizer must not produce a worse split than a naive first-three vs last-three";
}

// ── All-DPS match composition ─────────────────────────────────────────────────

/// Test 14: In an all-DPS match neither team may contain a healer.
TEST_F(MatchmakingTest, FindBestTeamSplit_AllDPSMatch_NoHealersOnEitherTeam)
{
    // Directly populate 'selected' as all DPS and set allDpsMatch=true
    std::vector<QueuedCandidate> selected = {
        {1, PlayerRole::DPS, 1600, 0}, {2, PlayerRole::DPS, 1550, 0},
        {3, PlayerRole::DPS, 1500, 0}, {4, PlayerRole::DPS, 1450, 0},
        {5, PlayerRole::DPS, 1400, 0}, {6, PlayerRole::DPS, 1350, 0},
    };

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, /*allDpsMatch=*/true);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(CountHealers(result.team1Indices, selected), 0u)
        << "All-DPS match: team 1 must not contain a healer";
    EXPECT_EQ(CountHealers(result.team2Indices, selected), 0u)
        << "All-DPS match: team 2 must not contain a healer";
}

/// Test 15: The full pipeline with the all-DPS fallback produces balanced teams.
TEST_F(MatchmakingTest, FullPipeline_AllDPS_ProducesBalancedTeams)
{
    uint32_t const timer = ALL_DPS_TIMER;
    uint32_t const now   = 70000; // timer has elapsed for all players

    std::vector<QueuedCandidate> candidates = {
        {1, PlayerRole::DPS, 1600, 0}, {2, PlayerRole::DPS, 1400, 0},
        {3, PlayerRole::DPS, 1550, 0}, {4, PlayerRole::DPS, 1450, 0},
        {5, PlayerRole::DPS, 1500, 0}, {6, PlayerRole::DPS, 1500, 0},
    };

    std::vector<QueuedCandidate> selected;
    bool allDpsMatch = false;
    bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                        timer, now, selected, allDpsMatch);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(allDpsMatch);

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, allDpsMatch);
    ASSERT_TRUE(result.valid);

    uint32_t s1 = SumMMR(result.team1Indices, selected);
    uint32_t s2 = SumMMR(result.team2Indices, selected);
    uint64_t diff = s1 > s2 ? s1 - s2 : s2 - s1;

    // Best achievable: [1600+1500+1400=4500] vs [1550+1500+1450=4500] → 0
    EXPECT_EQ(diff, 0u) << "Symmetric all-DPS MMR must produce perfectly balanced teams";
}

// ── filterTalents disabled ────────────────────────────────────────────────────

/// Test 16: With filterTalents off, FindBestTeamSplit accepts any split
/// regardless of how many healers each team contains.
TEST_F(MatchmakingTest, FindBestTeamSplit_FilterTalentsDisabled_AnyCompositionValid)
{
    std::vector<QueuedCandidate> selected = {
        {1, PlayerRole::HEALER, 1500, 0}, {2, PlayerRole::HEALER, 1500, 0},
        {3, PlayerRole::HEALER, 1500, 0}, {4, PlayerRole::DPS,    1500, 0},
        {5, PlayerRole::DPS,    1500, 0}, {6, PlayerRole::DPS,    1500, 0},
    };

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, false, false);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.team1Indices.size(), 3u);
    EXPECT_EQ(result.team2Indices.size(), 3u);
}

// ── End-to-end: 1H+2DPS vs 1H+2DPS assertion ─────────────────────────────────

/// Test 17: Full pipeline with varied MMR values — end-to-end verification
/// that the pairing is always 1 healer + 2 DPS vs 1 healer + 2 DPS.
TEST_F(MatchmakingTest, FullPipeline_VariedMMR_AlwaysOneHealerTwoDPSPerTeam)
{
    // Run several different MMR configurations
    std::vector<std::vector<std::pair<PlayerRole, uint32_t>>> configs = {
        // Config A: uniform MMR
        {{PlayerRole::HEALER, 1500}, {PlayerRole::HEALER, 1500},
         {PlayerRole::DPS, 1500},   {PlayerRole::DPS, 1500},
         {PlayerRole::DPS, 1500},   {PlayerRole::DPS, 1500}},
        // Config B: healer spread, DPS spread
        {{PlayerRole::HEALER, 1800}, {PlayerRole::HEALER, 1300},
         {PlayerRole::DPS, 1700},   {PlayerRole::DPS, 1600},
         {PlayerRole::DPS, 1400},   {PlayerRole::DPS, 1200}},
        // Config C: high-rated healer, average DPS
        {{PlayerRole::HEALER, 2200}, {PlayerRole::HEALER, 1500},
         {PlayerRole::DPS, 1600},   {PlayerRole::DPS, 1500},
         {PlayerRole::DPS, 1450},   {PlayerRole::DPS, 1400}},
    };

    for (size_t ci = 0; ci < configs.size(); ++ci)
    {
        auto candidates = MakeCandidates(configs[ci]);

        std::vector<QueuedCandidate> selected;
        bool allDpsMatch = false;
        bool ok = composer.SelectCandidates(candidates, TEAM_SIZE, true,
                                            ALL_DPS_TIMER, 100000, selected, allDpsMatch);

        ASSERT_TRUE(ok) << "Config " << ci << ": selection must succeed";

        auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, allDpsMatch);
        ASSERT_TRUE(result.valid) << "Config " << ci << ": team split must be valid";

        EXPECT_EQ(CountHealers(result.team1Indices, selected), 1u)
            << "Config " << ci << ": team 1 must have exactly 1 healer";
        EXPECT_EQ(CountHealers(result.team2Indices, selected), 1u)
            << "Config " << ci << ": team 2 must have exactly 1 healer";

        EXPECT_EQ(result.team1Indices.size(), 3u)
            << "Config " << ci << ": team 1 must have 3 players";
        EXPECT_EQ(result.team2Indices.size(), 3u)
            << "Config " << ci << ": team 2 must have 3 players";
    }
}

// ── PreventClassStacking ──────────────────────────────────────────────────────
// WoW class IDs referenced below (from SharedDefines.h):
//   1=Warrior  2=Paladin  3=Hunter  4=Rogue  5=Priest
//   6=DeathKnight  7=Shaman  8=Mage  9=Warlock  11=Druid

/// Test 18: With PreventClassStacking disabled (level 0), two players of the
/// same class on the same team must be accepted.
TEST_F(MatchmakingTest, ClassStacking_Disabled_SameClassAllowed)
{
    // Two Warriors (class 1) as DPS — should land on the same team freely
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest healer
        {PlayerRole::HEALER, 1500, 7},  // Shaman healer
        {PlayerRole::DPS,    1500, 1},  // Warrior
        {PlayerRole::DPS,    1500, 1},  // Warrior (duplicate class)
        {PlayerRole::DPS,    1500, 4},  // Rogue
        {PlayerRole::DPS,    1500, 4},  // Rogue (duplicate class)
    });

    // level 0 = disabled
    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 0, 0);

    ASSERT_TRUE(result.valid) << "With stacking disabled, a match must always form";
}

/// Test 19: With PreventClassStacking = 1 (all roles), two players of the same
/// class must never appear on the same team.
TEST_F(MatchmakingTest, ClassStacking_Level1_AllRoles_NoDuplicateClassPerTeam)
{
    // H:Priest, H:Shaman, DPS:Warrior, DPS:Rogue, DPS:Warrior, DPS:Mage
    // The two Warriors (class 1) must be split across teams.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest
        {PlayerRole::HEALER, 1500, 7},  // Shaman
        {PlayerRole::DPS,    1500, 1},  // Warrior A
        {PlayerRole::DPS,    1500, 4},  // Rogue
        {PlayerRole::DPS,    1500, 1},  // Warrior B
        {PlayerRole::DPS,    1500, 8},  // Mage
    });

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 1, 0);

    ASSERT_TRUE(result.valid);
    EXPECT_FALSE(TeamHasDuplicateClass(result.team1Indices, selected))
        << "Team 1 must not contain two players of the same class";
    EXPECT_FALSE(TeamHasDuplicateClass(result.team2Indices, selected))
        << "Team 2 must not contain two players of the same class";
}

/// Test 20: When every possible split would place two same-class players
/// together (no valid split exists), FindBestTeamSplit must return !valid.
TEST_F(MatchmakingTest, ClassStacking_Level1_NoValidSplit_ReturnsInvalid)
{
    // 2 healers of the same class + 4 DPS of the same class:
    // any valid 1H+2DPS split forces both teams to have the same class.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest
        {PlayerRole::HEALER, 1500, 5},  // Priest (duplicate)
        {PlayerRole::DPS,    1500, 1},  // Warrior
        {PlayerRole::DPS,    1500, 1},  // Warrior (duplicate)
        {PlayerRole::DPS,    1500, 1},  // Warrior (duplicate)
        {PlayerRole::DPS,    1500, 1},  // Warrior (duplicate)
    });

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 1, 0);

    EXPECT_FALSE(result.valid)
        << "No valid split exists when all DPS share the same class and both healers share a class";
}

/// Test 21: ClassIdToMaskBit helper returns the correct bitmask for each class.
TEST_F(MatchmakingTest, ClassStacking_ClassIdToMaskBit_CorrectValues)
{
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(1),  1u);     // Warrior
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(2),  2u);     // Paladin
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(3),  4u);     // Hunter
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(4),  8u);     // Rogue
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(5),  16u);    // Priest
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(6),  32u);    // Death Knight
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(7),  64u);    // Shaman
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(8),  128u);   // Mage
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(9),  256u);   // Warlock
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(11), 1024u);  // Druid
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(0),  0u);     // none
    EXPECT_EQ(MatchmakingComposer::ClassIdToMaskBit(10), 0u);     // unused slot
}

/// Test 22: Class mask filtering — when the duplicate class is excluded from
/// the mask, the stacking rule must not apply and the match must form.
TEST_F(MatchmakingTest, ClassStacking_ClassMask_ExcludedClassIsIgnored)
{
    // Two Warriors as DPS. Warrior bitmask = 1.
    // Setting classMask = 2 (Paladin only) must NOT block the Warriors.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest
        {PlayerRole::HEALER, 1500, 7},  // Shaman
        {PlayerRole::DPS,    1500, 1},  // Warrior A
        {PlayerRole::DPS,    1500, 1},  // Warrior B — same class as A
        {PlayerRole::DPS,    1500, 8},  // Mage
        {PlayerRole::DPS,    1500, 9},  // Warlock
    });

    // level 1, mask = Paladin only (2) — Warriors are NOT in the mask
    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 1, 2);

    ASSERT_TRUE(result.valid)
        << "Warriors are excluded from the mask so duplicate Warriors must be allowed";
}

/// Test 23: Class mask filtering — when the duplicate class IS in the mask,
/// the stacking rule must apply and prevent them from sharing a team.
TEST_F(MatchmakingTest, ClassStacking_ClassMask_IncludedClassIsBlocked)
{
    // Two Warriors (class 1, mask bit 1). Set classMask = 1 to block Warriors.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest
        {PlayerRole::HEALER, 1500, 7},  // Shaman
        {PlayerRole::DPS,    1500, 1},  // Warrior A
        {PlayerRole::DPS,    1500, 1},  // Warrior B — duplicate
        {PlayerRole::DPS,    1500, 8},  // Mage
        {PlayerRole::DPS,    1500, 9},  // Warlock
    });

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 1, 1);

    ASSERT_TRUE(result.valid);
    EXPECT_FALSE(TeamHasDuplicateClass(result.team1Indices, selected))
        << "Warriors (in mask) must not both land on team 1";
    EXPECT_FALSE(TeamHasDuplicateClass(result.team2Indices, selected))
        << "Warriors (in mask) must not both land on team 2";
}

/// Test 24: Level 4 (any DPS) — two same-class DPS must be split.
/// Healer duplicates are ignored by this level.
TEST_F(MatchmakingTest, ClassStacking_Level4_DPSOnly_HealerDuplicateAllowed)
{
    // Healers: two Priests (class 5) — level 4 must NOT block them.
    // DPS: two Rogues (class 4) — level 4 MUST split them.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 5},  // Priest A
        {PlayerRole::HEALER, 1500, 5},  // Priest B (duplicate healer class)
        {PlayerRole::DPS,    1500, 4},  // Rogue A
        {PlayerRole::DPS,    1500, 4},  // Rogue B (duplicate DPS class)
        {PlayerRole::DPS,    1500, 8},  // Mage
        {PlayerRole::DPS,    1500, 9},  // Warlock
    });

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 4, 0);

    ASSERT_TRUE(result.valid) << "Level 4 must still allow duplicate healers";

    // Rogue A and Rogue B must be on opposite teams
    bool rogueAOnTeam1 = std::find(result.team1Indices.begin(), result.team1Indices.end(), 2u) != result.team1Indices.end();
    bool rogueBOnTeam1 = std::find(result.team1Indices.begin(), result.team1Indices.end(), 3u) != result.team1Indices.end();
    EXPECT_NE(rogueAOnTeam1, rogueBOnTeam1) << "The two Rogues must be on different teams (level 4)";
}

/// Test 25: Levels 5 & 6 — healer and DPS of the same class must not share a team.
/// Example: Resto Druid (healer) + Balance Druid (DPS) on the same team is blocked.
TEST_F(MatchmakingTest, ClassStacking_Level6_HealerDPSSameClass_BlockedTogether)
{
    // Healer: Druid (class 11). DPS: Druid (class 11) + 3 others.
    // Level 6 must prevent Druid healer + Druid DPS on the same team.
    auto selected = MakeCandidatesWithClass({
        {PlayerRole::HEALER, 1500, 11}, // Resto Druid
        {PlayerRole::HEALER, 1500, 5},  // Disc Priest
        {PlayerRole::DPS,    1500, 11}, // Balance Druid (same class as healer)
        {PlayerRole::DPS,    1500, 8},  // Mage
        {PlayerRole::DPS,    1500, 1},  // Warrior
        {PlayerRole::DPS,    1500, 9},  // Warlock
    });

    auto result = composer.FindBestTeamSplit(selected, TEAM_SIZE, true, false, 6, 0);

    ASSERT_TRUE(result.valid);

    // The Druid healer (index 0) and Druid DPS (index 2) must be on different teams
    bool druidHealerOnTeam1 = std::find(result.team1Indices.begin(), result.team1Indices.end(), 0u) != result.team1Indices.end();
    bool druidDPSOnTeam1    = std::find(result.team1Indices.begin(), result.team1Indices.end(), 2u) != result.team1Indices.end();
    EXPECT_NE(druidHealerOnTeam1, druidDPSOnTeam1)
        << "Resto Druid and Balance Druid must not share a team (level 6)";
}

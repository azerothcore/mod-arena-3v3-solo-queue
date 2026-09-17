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
#include "ArenaTeam.h"
#include "ObjectGuid.h"
#include "WorldMock.h"
#include "gtest/gtest.h"

/// Per-player rating/MMR maths of Solo3v3::ApplyRatedResult against worldserver defaults:
/// Arena.ArenaWinRatingModifier1/2 = 48/24, Arena.ArenaLoseRatingModifier = 24,
/// Arena.ArenaMatchmakerRatingModifier = 24, Arena.MaxAllowedMMRDrop = 500.
class RatedResultTest : public ::testing::Test
{
protected:
    static constexpr ObjectGuid::LowType MEMBER_GUID = 42;

    void SetUp() override
    {
        previousWorld_ = std::move(sWorld);
        auto* mock = new ::testing::NiceMock<WorldMock>();
        ON_CALL(*mock, getIntConfig(::testing::_)).WillByDefault(::testing::Return(0));
        ON_CALL(*mock, getIntConfig(CONFIG_MAX_ALLOWED_MMR_DROP)).WillByDefault(::testing::Return(500));
        ON_CALL(*mock, getFloatConfig(CONFIG_ARENA_WIN_RATING_MODIFIER_1)).WillByDefault(::testing::Return(48.f));
        ON_CALL(*mock, getFloatConfig(CONFIG_ARENA_WIN_RATING_MODIFIER_2)).WillByDefault(::testing::Return(24.f));
        ON_CALL(*mock, getFloatConfig(CONFIG_ARENA_LOSE_RATING_MODIFIER)).WillByDefault(::testing::Return(24.f));
        ON_CALL(*mock, getFloatConfig(CONFIG_ARENA_MATCHMAKER_RATING_MODIFIER)).WillByDefault(::testing::Return(24.f));
        sWorld.reset(mock);
    }

    void TearDown() override
    {
        sWorld = std::move(previousWorld_);
    }

    /// Turns a default team into a solo team holding one member with the given rating and MMR.
    static void SetupTeam(ArenaTeam& team, uint16 rating, uint16 mmr, uint16 maxMMR)
    {
        ArenaTeamStats stats = team.GetStats();
        stats.Rating = rating;
        team.SetArenaTeamStats(stats);

        ArenaTeamMember member{};
        member.Guid             = ObjectGuid::Create<HighGuid::Player>(MEMBER_GUID);
        member.PersonalRating   = rating;
        member.MatchMakerRating = mmr;
        member.MaxMMR           = maxMMR;
        team.GetMembers().push_back(member);
    }

    static ArenaTeamMember const& Member(ArenaTeam& team)
    {
        return team.GetMembers().front();
    }

    static void Apply(ArenaTeam& team, bool won, uint32 ownSideMMR, uint32 opponentSideMMR)
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(MEMBER_GUID);
        sSolo->ApplyRatedResult(&team, guid, won, ownSideMMR, opponentSideMMR);
    }

    std::unique_ptr<IWorld> previousWorld_;
};

TEST_F(RatedResultTest, FreshPlayerWinAgainstStartMMR)
{
    ArenaTeam team;
    SetupTeam(team, 0, 1500, 1500);
    Apply(team, true, 1500, 1500);

    EXPECT_EQ(team.GetStats().Rating, 48u);
    EXPECT_EQ(team.GetStats().SeasonGames, 1u);
    EXPECT_EQ(team.GetStats().WeekGames, 1u);
    EXPECT_EQ(team.GetStats().SeasonWins, 1u);
    EXPECT_EQ(team.GetStats().WeekWins, 1u);
    EXPECT_EQ(Member(team).PersonalRating, 48u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1512u);
    EXPECT_EQ(Member(team).MaxMMR, 1512u);
    EXPECT_EQ(Member(team).SeasonGames, 1u);
    EXPECT_EQ(Member(team).SeasonWins, 1u);
}

TEST_F(RatedResultTest, FreshPlayerLossAgainstStartMMR)
{
    ArenaTeam team;
    SetupTeam(team, 0, 1500, 1500);
    Apply(team, false, 1500, 1500);

    EXPECT_EQ(team.GetStats().Rating, 0u);
    EXPECT_EQ(team.GetStats().SeasonGames, 1u);
    EXPECT_EQ(team.GetStats().SeasonWins, 0u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1488u);
    EXPECT_EQ(Member(team).MaxMMR, 1500u);
    EXPECT_EQ(Member(team).SeasonGames, 1u);
    EXPECT_EQ(Member(team).SeasonWins, 0u);
}

TEST_F(RatedResultTest, EvenWin)
{
    ArenaTeam team;
    SetupTeam(team, 1500, 1500, 1500);
    Apply(team, true, 1500, 1500);

    EXPECT_EQ(team.GetStats().Rating, 1512u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1512u);
}

TEST_F(RatedResultTest, UnderdogWin)
{
    ArenaTeam team;
    SetupTeam(team, 1500, 1500, 1500);
    Apply(team, true, 1500, 1800);

    EXPECT_EQ(team.GetStats().Rating, 1518u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1518u);
}

TEST_F(RatedResultTest, UnderdogLoss)
{
    ArenaTeam team;
    SetupTeam(team, 1500, 1500, 1500);
    Apply(team, false, 1500, 1800);

    EXPECT_EQ(team.GetStats().Rating, 1494u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1494u);
}

TEST_F(RatedResultTest, FavouriteLoss)
{
    ArenaTeam team;
    SetupTeam(team, 1800, 1800, 1800);
    Apply(team, false, 1800, 1500);

    EXPECT_EQ(team.GetStats().Rating, 1783u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1783u);
    EXPECT_EQ(Member(team).MaxMMR, 1800u);
}

// Arena.MaxAllowedMMRDrop keeps MMR within 500 of MaxMMR: at 1500 with MaxMMR 2000 a loss changes nothing.
TEST_F(RatedResultTest, MMRLossFlooredByMaxAllowedDrop)
{
    ArenaTeam team;
    SetupTeam(team, 1000, 1500, 2000);
    Apply(team, false, 1500, 1500);

    EXPECT_EQ(Member(team).MatchMakerRating, 1500u);
    EXPECT_EQ(Member(team).MaxMMR, 2000u);
    EXPECT_EQ(team.GetStats().Rating, 997u);
}

TEST_F(RatedResultTest, RatingCannotDropBelowZero)
{
    ArenaTeam team;
    SetupTeam(team, 5, 1500, 1500);
    Apply(team, false, 1500, 0);

    EXPECT_EQ(team.GetStats().Rating, 0u);
    EXPECT_EQ(Member(team).PersonalRating, 0u);
    EXPECT_EQ(Member(team).MatchMakerRating, 1477u);
}

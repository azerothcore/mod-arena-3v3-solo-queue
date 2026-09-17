# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

# Arena solo 3v3 Queue

- Latest build status with azerothcore:

[![Build Status](https://github.com/pangolp/mod-arena-3v3-solo-queue/workflows/core-build/badge.svg)](https://github.com/pangolp/mod-arena-3v3-solo-queue)

# ![mod-arena-3v3-solo-queue](https://github.com/azerothcore/mod-arena-3v3-solo-queue/blob/master/icon.png?raw=true)

This module finally reached a stable version!

Much of the code was extracted from the [mod-azerothshard](https://github.com/azerothcore/mod-azerothshard) module.

# ![mod-arena-3v3-solo-queue-battlemaster](https://github.com/azerothcore/mod-arena-3v3-solo-queue/blob/master/images/3v3soloq-battlemaster.png?raw=true)

### Usage

`.npc add 1000003`

---

### Rating and matchmaker rating (MMR)

`Solo.3v3.UseMatchmakerRating` in `conf/arena_3v3_solo_queue.conf.dist` selects how rated solo
matches change rating and MMR:

- **`1` (default), per-player Elo.** Players are matched by their hidden solo MMR. Each player's
  rating change is computed from their own rating against the opposing side's average MMR, and their
  MMR moves by Elo against the opposing side's MMR. A fresh player climbs fast while an established
  teammate barely moves.
- **`0`, core team delta.** Players are matched by visible rating, and the temporary team's rating
  (the average of its three players) is used as its MMR. The core computes one delta for the
  temporary team and every player receives it on both rating and MMR.

The table uses the worldserver defaults (`Arena.ArenaWinRatingModifier1/2` = 48/24,
`Arena.ArenaLoseRatingModifier` = 24, `Arena.ArenaMatchmakerRatingModifier` = 24, start MMR 1500).
"Fresh" means rating 0 and MMR 1500. Other players have an MMR equal to their rating. Changes are
per player.

| Scenario                                                                   | Result | Elo: rating                       | Elo: MMR | Core: rating                     | Core: MMR      |
| -------------------------------------------------------------------------- | ------ | --------------------------------- | -------- | -------------------------------- | -------------- |
| Six fresh players                                                          | win    | +48                               | +12      | +24                              | +24            |
| Six fresh players                                                          | loss   | 0                                 | -12      | 0                                | -12            |
| Six 1500 players                                                           | win    | +12                               | +12      | +12                              | +12            |
| Six 1500 players                                                           | loss   | -12                               | -12      | -12                              | -12            |
| Three 1500 players vs three 1800 players                                   | win    | +18                               | +18      | +18                              | +18            |
| Three 1500 players vs three 1800 players                                   | loss   | -6                                | -6       | -6                               | -6             |
| Three 1800 players vs three 1500 players                                   | win    | +7                                | +7       | +7                               | +7             |
| Three 1800 players vs three 1500 players                                   | loss   | -17                               | -17      | -17                              | -17            |
| 1800 player and two fresh players vs three 600-rated players with MMR 1500 | win    | 1800 player +7, fresh players +48 | +10      | +24                              | +24            |
| 1800 player and two fresh players vs three 600-rated players with MMR 1500 | loss   | 1800 player -17, fresh players 0  | -14      | 1800 player -12, fresh players 0 | -12            |
| Leaves a running match alive, 1500 sides                                   | n/a    | -50 first leaver, -24 afterwards  | -12      | -50 first leaver, -24 afterwards | same as rating |
| Leaves before the match starts or declines the queue                       | n/a    | -50                               | 0        | -50                              | -50            |

Evenly matched lobbies give the same numbers in both modes. The modes differ when teammates have
different skill levels or when fresh players are involved.

In both modes the end-of-match scoreboard shows the core's temporary team delta, and a draw only
counts the game. With `1`, MMR losses are capped by `Arena.MaxAllowedMMRDrop` relative to the
player's highest MMR, as in the core. On update, `character_arena_stats` rows of the solo slot are
reseeded to `max(1500, personal rating)` so the first matches start from a meaningful MMR.

### Contributors

- mod-azerothshard authors
- Helias
- Laasker
- pangolp/stevej

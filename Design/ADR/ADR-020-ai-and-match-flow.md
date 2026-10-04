# ADR-020 — The AI is a client with its own settings file, a match ends once a player is beaten, and the game opens on a menu

Status: **accepted** · 2026-10-01

## Context

Milestone 6 adds the opponent and the match around it (design §10, §14): the AI player (task 6.1), winning, losing and the menu (task 6.2), and a log for the owner's Q1 and Q3 playtests (task 6.3). The AI must be a client that reads its snapshot and sends commands, and it may include only `GameProtocol` (ADR-002).

Design §10 lists what the AI does and leaves the details open. The owner settled them on 2026-10-01:

- **Gate G9: the attack group's size**, which [ADR-041](ADR-041-ai-plays-a-longer-match.md) sets.
- **Research order, economy first and then the heavies:** Improved Extraction, Hull Plating, Fusion Drive, Large Hull, Mass Driver Calibration, Lance Focusing, Automated Shipyards, Missile Rack.
- **Counters are design §7's triangle, held as data.** The swarm (Small+Ion+Mass Driver) is answered with the brawler (Medium+Ion+Mass Driver), the brawler with the line (Medium+Ion+Lance), and the line with the swarm. The picket (Small+Ion+Lance) is answered with the swarm, and each heavy (Large+Fusion with any weapon) with the picket. Once it has the Large hull and the Fusion drive, it answers the swarm with Large+Fusion+Mass Driver and the brawler with Large+Fusion+Lance instead, as design §7 says they beat them. Anything else, and no enemy fleet yet, gets the brawler. The AI uses only designs it has unlocked.
- **The base:** keep 4 Constructors. Rigs on the 3 home asteroids, then a Shipyard, a Research Lab and a Defence Platform at home. Then rigs on the 3 contested asteroids nearest the AI, each with a platform beside it. A Shipyard for each 10 Ore/s of income: the N-th once income reaches N × 10 Ore/s.
- **The match end:** a Victory, Defeat or Draw banner with the match's length, and the world runs on until the player goes back to the menu.

## Decision

1. **The AI is `Outpost::AiPlayer` in `Opponent`.** The shell hands it every snapshot of its player, and it returns the commands to send through its own connection. It watches every snapshot for shots and decides once a second. It is not deterministic across builds, and it need not be: the server logs the commands it applied, so a match replays from its seed and command log without the AI (ADR-009).
2. **The AI's numbers are in `OutpostCommander/Assets/Opponent.json`, not `Tuning.json`.** They are how one client plays, not the rules of the match, and the server never reads them. That is where gate G9 is recorded, `attackGroupShips`, though the plan named `Tuning.json`. The review interval moved there from `Tuning.json`. `LoadAiSettings` reads the file as strictly as the tuning loader does. It cannot check the identifiers against `Tuning.json`, which only the server reads, so `GameLogicTests` does: every component and topic it names exists, and the research order comes after each topic's prerequisites.
3. **A snapshot shows the hull, drive and weapon of every warship the player sees, whoever owns it** ([ADR-024](ADR-024-fog-of-war.md) decision 6). A design's identifier names a design in its owner's list only, so the components are what the AI counts.
4. **`PlaceGhost` moved from `GameApp` to `GameProtocol`**, unchanged, so that the AI places structures by the rule the player's ghost shows. Both apply the server's placement rule to what a snapshot shows (ADR-016).
5. **The base plan is laid at the AI's first decision.** The home asteroids are the 3 nearest its Command Station. The contested ones are the next nearest, leaving out any nearer the enemy's base, as [ADR-024](ADR-024-fog-of-war.md) decision 8 places it. The Shipyard, the Research Lab and the home platform stand to the sides and front of the station, toward the map's center. A contested platform stands beside its rig, toward the AI's base. Each place keeps 50 m clear of everything else, so that ships still pass, and is found by searching outward in rings when the preferred one is taken. A place is checked again when its structure is ordered. The AI builds the plan in order, at most two Constructors to a site, and the rest go to the next site. A rig whose asteroid another player holds is skipped, and so is its platform. A destroyed structure is built again. Ships are not queued while the next structure waits for Ore. When one of its rigs' asteroids runs dry, it plans a rig, with a platform beside it, on the nearest asteroid it knows still holds ore, outside the enemy's home (Phase 1 design §13). On a map with territory, a rig in a sector the AI does not hold follows a Relay on that sector's node, built once the sector is next to the AI's territory ([ADR-056](ADR-056-territory.md) decision 12).
6. **Shipyards join the plan as the income grows.** Once its income reaches N × `incomePerShipyardOrePerSecond` with N − 1 Shipyards planned, the AI plans the N-th: behind the station, then behind it to either side, each round of three farther out, at the nearest clear place. A planned Shipyard waits while the income is below its share, as after a rig is lost. One Shipyard spends 8 to 14 Ore/s depending on the design, and a quarter more after Automated Shipyards, so 10 Ore/s keeps the Shipyards spending about what the rigs earn.
7. **The AI tracks its Constructors itself.** A snapshot shows no ship's orders, and the server does not report a refused command. So the AI remembers which Constructors it sent where. If the site it ordered has not appeared 3 s later, it takes the order as refused. A site with nobody on it gets the idle Constructors. Constructors it sends to a structure's next level stay until the level is in, and it upgrades a Shipyard below its production design's hull before it queues that design there ([ADR-064](ADR-064-structure-upgrades.md) decision 9).
8. **Counters, and the AI's fleet.**
   - Every 60 s it finds the most common design, by components, among the enemy warships it has seen since the last review ([ADR-024](ADR-024-fog-of-war.md) decision 8), and builds the first of that design's counters it has unlocked, and the default when it has none. `Opponent.json` lists a design's counters in order of preference, the heavy before the triangle's answer. A tie goes to the lowest hull, then drive, then weapon.
   - It saves a design it does not have, and keeps 2 jobs in each Shipyard's queue.
   - Its warships gather in reserve 170 m from its Command Station, toward the map's center. On a map with territory they hold a sector instead (decision 13).
   - When the reserve reaches the attack group's size ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 1), they join the attack group. The group goes for the enemy structures it sees or remembers, production first ([ADR-037](ADR-037-losing-all-production.md) decision 4), and falls back once it has lost too many ships ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 2).
   - A shot on any of its structures, built or a site, sends the reserve there with an attack-move (design §10, owner, 2026-10-01). The reserve goes back 10 s after the last shot. It is sent again only when where it goes, or whether it holds a sector there, changes.
   - The AI never kites (design §7).
9. **A match ends once a player whose base was placed is beaten**, by the rule of [ADR-037](ADR-037-losing-all-production.md) decision 1. Every snapshot carries `matchOver`, the winner, and `matchEndedTick`. The winner is no player when both are beaten in the same tick. The world runs on after the match ends, and the outcome stands. A world with no bases placed never ends, as in the movement and combat tests.
10. **The game opens on a menu with Start skirmish and Quit.** `GameClient` has a menu screen and a match screen. Starting or leaving a match resets everything the last match left in the view, without reloading a mesh. Each match has its own server and seed.
   - The next match's server is made while the menu shows, and the first one on a thread of its own while the window is still hidden ([ADR-049](ADR-049-startup-in-parallel.md) decisions 2 and 3), so that bad tuning, map or AI data is reported before the screen goes full screen.
   - The AI is player 2, which `Models.json` draws with the Tarkan set.
   - `--measure`, `--load` and `--stress` skip the menu, and under load or stress no AI plays.
11. **Every match against the AI is added to `OutpostCommander-matches.log` in the temporary folder**, by `Outpost::MatchLog` in `GameApp`. It records:
    - the seed,
    - each player's research as it finishes,
    - each warship as it first appears, by its components and its module,
    - how the match ended, or that it was left.

    [ADR-038](ADR-038-phase-one-match-log.md) decision 1 adds four more records. `Tools/MatchLog.py` prints, for each match, the research times and the designs each side built in 5-minute windows, with the figures of [ADR-038](ADR-038-phase-one-match-log.md) decision 4, the match's length among them.
12. **`GameLogicTests` lists `Opponent`** in its include path and references, so that the AI plays the real server headlessly (ADR-002's table, AGENTS.md §2).
13. **On a map with territory the AI plays for it** (Phase 2 design §12, plan task 18.1). Its numbers are in `Opponent.json`; on a map without territory none of this happens.
    - **Claims.** Once its first Shipyard stands, it claims the free sector next to its territory nearest its base: a Relay on the node, and a rig on each of the sector's asteroids, which wait for the Relay (decision 5). It claims one sector at a time, once every Relay it has planned stands or waits on the enemy, and at most `claimSectors`, 1, beyond those its rigs take it to: a sector its plan already has a Relay for is not a claim.
    - **Its front.** A sector it holds next to one the enemy holds, not its home, is its front. The Relay there gets `frontPlatforms`, 1, Defence Platform beside it toward the AI's base, once.
    - **A scout.** It keeps `scouts`, 1, of `scoutDesign`: Small+Ion+Mass Driver with the Sensor Array ([ADR-058](ADR-058-modules.md)). A scout is queued ahead of its warships and of a structure waiting for Ore, so that the first sets out in the first two minutes. It attack-moves from one sector next to the enemy's home to the next, so that it meets what is there and suppresses the Relay there while it lives ([ADR-056](ADR-056-territory.md)). The enemy's home is across the map's center from its own. A scout is not a warship of the reserve.
    - **Raids.** While its reserve holds at least twice `raidShips`, 2, that many of it, the smallest hulls, which are the fastest, attack-move on the nearest enemy sector by whose node it sees no enemy warship within 600 m, the enemy's home aside. Those that come within reach of the sector's Relay attack it. The raid ends once the sector is no longer the enemy's, or once it has lost `raidLossShare`, 0.5, of its ships. Its ships then rejoin the reserve, and the next raid waits `raidIntervalSeconds`, 180.
    - **Holding.** The reserve holds a sector with a standing order ([ADR-059](ADR-059-alerts-and-standing-orders.md)): its front sector nearest the rally, or the rally's own sector while it has no front. The server then sends it at the enemy ships it sees in the sector, and back to the node.
    - **The main attack** waits for a lead in nodes, or a larger reserve ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 5).
    - **Ties are mirrored.** Two places as near as each other, such as the two sectors beside its home or beside the enemy's, are told apart by the side of the line from its base to the map's center each lies on. Both seats of the point-symmetric map so choose mirrored places. Told apart by the sectors' identifiers, player 1 claimed the South and player 2 the East, not the North that mirrors the South, and player 1 won 16 of seeds 1 to 20.
14. **The AI plays structure levels** (Phase 3 design §8, plan task 24.1; [ADR-064](ADR-064-structure-upgrades.md)). Its numbers are in `Opponent.json`.
    - **Its Command Station** is upgraded whenever a Relay of its plan waits on the cap, in that Relay's place in the plan's order. Its home and the two flanks its rigs take it to are level 1's cap, so its claim beyond them waits for level 2, and it goes no higher while it wants no further node.
    - **Its Shipyards** are upgraded to the level its production design's hull needs, one at a time: level 2 from the start, since its default design is a Medium hull, and level 3 once it answers with a Large one.
    - **Its Research Lab** is upgraded when the next topic of its research order is of a tier the Lab has not opened, and to the level that gives a second slot once its open tier is `secondSlotTier`, 3, while two topics of its order are left.
    - **An upgrade it cannot yet pay for holds back new production**, as a structure waiting for Ore does, so that the Ore gathers for it.
    - **What it attacks.** Within a rank of [ADR-037](ADR-037-losing-all-production.md) decision 4, each level a structure has above the first counts as `attackLevelMeters`, 500, nearer the group: a Shipyard at level 3 1,000 m farther off goes first.

## Consequences

All figures below are from the GameLogicTests harness on Linux, built with clang at `-O2` from this branch on 2026-10-01. They use the repository's data and the real server.

- **The AI beats a player who does nothing at 5:24** (tick 6,483, seed 3). It places its base plan by 1:45 and its fourth Shipyard by 2:15, has all four built by 3:15, and sends its first attack group by 3:30.
- **Its Shipyards spend most of what it earns.** From about 2:15 its income is 48.75 Ore/s: 3 home rigs, 3 contested rigs, and Improved Extraction, so it has 4 Shipyards. Building brawlers they spend about 33 Ore/s, and it holds about 3,300 Ore when the passive player's station falls, still gaining about 12 Ore/s.
- **The AI builds heavies once it can.** Large Hull lands at 6:35. In 4 of the 5 matches below one side or both built Large+Fusion+Lance or Large+Fusion+Mass Driver from then on, and what each side built changed from one 5-minute window to the next.
- **Two AIs on the mirrored map end a match in 7:51 to 14:54** over seeds 1 to 5, with §12 as retuned on 2026-10-01: 12:09, 11:49, 11:24, 14:54 and 7:51. Player 1 won 2 of the 5. All five are shorter than Q1's 15 to 25 minutes, but a human does not play as the AI does, so it is no answer to Q1.
- **The checks behind the AI** are:
  - Scripted snapshots: its first orders, its answer to each design of the triangle, and the reserve going to its Command Station and to a site under fire. For territory: the reserve holding its home and then its front, the main attack waiting for a lead or a larger reserve, a raid on an unguarded sector of the smallest hulls that ends once the sector is free, no raid on a guarded one, and a scout's tour.
  - The real server: its base in order, its Shipyards by income, its answer to an enemy fleet, the attack once its group has gathered, and the defence of an outpost. For territory: a scout in the first two minutes, one claim next to its home and none when the settings ask for none, and a platform by its Relay on the front. For levels: the station at level 2 before its fourth node and no higher, the Lab's second slot once tier 3 is open, and a Shipyard's level counted in what it attacks.
  - A whole match against a passive player.
- **Territory's play shortens AI-against-AI matches**, and [ADR-041](ADR-041-ai-plays-a-longer-match.md) gives the figures. What it does against a human is not measured: that is the owner's run.

## What this forecloses

- Tuning the AI in `Tuning.json`, or the server reading `Opponent.json`.
- The AI reading server state, or acting more often than once a second, without a new decision.
- A match that ends any other way, such as a time limit or a surrender, without a new decision.
- Snapshots that hide the components of an enemy warship the player sees.

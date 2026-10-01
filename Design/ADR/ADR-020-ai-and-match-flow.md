# ADR-020 — The AI is a client with its own settings file, a match ends when a Command Station falls, and the game opens on a menu

Status: **accepted** · 2026-10-01

## Context

Milestone 6 adds the opponent and the match around it (design §10, §14): the AI player (task 6.1), winning, losing and the menu (task 6.2), and a log for the owner's Q1 and Q3 playtests (task 6.3). The AI must be a client that reads its snapshot and sends commands, and it may include only `GameProtocol` (ADR-002).

Design §10 lists what the AI does and leaves the details open. The owner settled them on 2026-10-01:

- **Gate G9: the attack group is 12 ships.**
- **Research order, economy first and then the heavies:** Improved Extraction, Hull Plating, Fusion Drive, Large Hull, Mass Driver Calibration, Lance Focusing, Automated Shipyards, Missile Rack.
- **Counters are design §7's triangle, held as data.** The swarm (Small+Ion+Mass Driver) is answered with the brawler (Medium+Ion+Mass Driver), the brawler with the line (Medium+Ion+Lance), and the line with the swarm. The picket (Small+Ion+Lance) is answered with the swarm, and each heavy (Large+Fusion with any weapon) with the picket. Once it has the Large hull and the Fusion drive, it answers the swarm with Large+Fusion+Mass Driver and the brawler with Large+Fusion+Lance instead, as design §7 says they beat them. Anything else, and no enemy fleet yet, gets the brawler. The AI uses only designs it has unlocked.
- **The base:** keep 4 Constructors. Rigs on the 3 home asteroids, then a Shipyard, a Research Lab and a Defence Platform at home. Then rigs on the 3 contested asteroids nearest the AI, each with a platform beside it. A Shipyard for each 10 Ore/s of income: the N-th once income reaches N × 10 Ore/s.
- **The match end:** a Victory, Defeat or Draw banner with the match's length, and the world runs on until the player goes back to the menu.

## Decision

1. **The AI is `Outpost::AiPlayer` in `Opponent`.** The shell hands it every snapshot of its player, and it returns the commands to send through its own connection. It watches every snapshot for shots and decides once a second. It is not deterministic across builds, and it need not be: the server logs the commands it applied, so a match replays from its seed and command log without the AI (ADR-009).
2. **The AI's numbers are in `OutpostCommander/Assets/Opponent.json`, not `Tuning.json`.** They are how one client plays, not the rules of the match, and the server never reads them. That is where gate G9 is recorded, `attackGroupShips`, though the plan named `Tuning.json`. The review interval moved there from `Tuning.json`. `LoadAiSettings` reads the file as strictly as the tuning loader does. It cannot check the identifiers against `Tuning.json`, which only the server reads, so `GameLogicTests` does: every component and topic it names exists, and the research order comes after each topic's prerequisites.
3. **Every snapshot shows every warship's hull, drive and weapon, whoever owns it.** There is no fog of war in the MVP, so a player sees each ship, and now its components too. A design's identifier names a design in its owner's list only, so the components are what the AI counts. Fog of war, when it comes, filters these with the rest of the snapshot (ADR-002 decision 4).
4. **`PlaceGhost` moved from `GameApp` to `GameProtocol`**, unchanged, so that the AI places structures by the rule the player's ghost shows. Both apply the server's placement rule to what a snapshot shows (ADR-016).
5. **The base plan is fixed at the AI's first decision.** The home asteroids are the 3 nearest its Command Station. The contested ones are the next nearest, leaving out any nearer another player's station. The Shipyard, the Research Lab and the home platform stand to the sides and front of the station, toward the map's center. A contested platform stands beside its rig, toward the AI's base. Each place keeps 50 m clear of everything else, so that ships still pass, and is found by searching outward in rings when the preferred one is taken. A place is checked again when its structure is ordered. The AI builds the plan in order, at most two Constructors to a site, and the rest go to the next site. A rig whose asteroid another player holds is skipped, and so is its platform. A destroyed structure is built again. Ships are not queued while the next structure waits for Ore.
6. **Shipyards join the plan as the income grows.** Once its income reaches N × `incomePerShipyardOrePerSecond` with N − 1 Shipyards planned, the AI plans the N-th: behind the station, then behind it to either side, each round of three farther out, at the nearest clear place. A planned Shipyard waits while the income is below its share, as after a rig is lost. One Shipyard spends 8 to 14 Ore/s depending on the design, and a quarter more after Automated Shipyards, so 10 Ore/s keeps the Shipyards spending about what the rigs earn.
7. **The AI tracks its Constructors itself.** A snapshot shows no ship's orders, and the server does not report a refused command. So the AI remembers which Constructors it sent where. If the site it ordered has not appeared 3 s later, it takes the order as refused. A site with nobody on it gets the idle Constructors.
8. **Counters, and the AI's fleet.**
   - Every 60 s it finds the enemy's most common warship by components, and builds the first of that design's counters it has unlocked, and the default when it has none. `Opponent.json` lists a design's counters in order of preference, the heavy before the triangle's answer. A tie goes to the lowest hull, then drive, then weapon.
   - It saves a design it does not have, and keeps 2 jobs in each Shipyard's queue.
   - Its warships gather in reserve 170 m from its Command Station, toward the map's center.
   - When the reserve reaches 12 ships, they join the attack group. The group attack-moves on the nearest enemy structure, then on the next, until it dies out.
   - A shot on any of its structures, built or a site, sends the reserve there with an attack-move (design §10, owner, 2026-10-01). The reserve goes back 10 s after the last shot.
   - The AI never kites (design §7).
9. **A match ends once a player whose base was placed has no Command Station.** Every snapshot carries `matchOver`, the winner, and `matchEndedTick`. The winner is no player when both stations fall in the same tick. The world runs on after the match ends, and the outcome stands. A world with no bases placed never ends, as in the movement and combat tests.
10. **The game opens on a menu with Start skirmish and Quit.** `GameClient` has a menu screen and a match screen. Starting or leaving a match resets everything the last match left in the view, without reloading a mesh. Each match has its own server and seed.
   - The next match's server is made while the menu shows, and the first one before the window opens, so that bad tuning, map or AI data is reported before the screen goes full screen.
   - The AI is player 2, which `Models.json` draws with the Tarkan set.
   - `--measure`, `--load` and `--stress` skip the menu, and under load or stress no AI plays.
11. **Every match against the AI is added to `OutpostCommander-matches.log` in the temporary folder**, by `Outpost::MatchLog` in `GameApp`. It records:
    - the seed,
    - each player's research as it finishes,
    - each warship as it first appears, by its components,
    - how the match ended, or that it was left.

    `Tools/MatchLog.py` prints, for each match: its length against Q1's 15 to 25 minutes, the research times, and the designs each side built in 5-minute windows.
12. **`GameLogicTests` lists `Opponent`** in its include path and references, so that the AI plays the real server headlessly (ADR-002's table, AGENTS.md §2).

## Consequences

All figures below are from the GameLogicTests harness on Linux, built with clang at `-O2` from this branch on 2026-10-01. They use the repository's data and the real server.

- **The AI beats a player who does nothing at 5:24** (tick 6,483, seed 3). It places its base plan by 1:45 and its fourth Shipyard by 2:15, has all four built by 3:15, and sends its first attack group by 3:30.
- **Its Shipyards spend most of what it earns.** From about 2:15 its income is 48.75 Ore/s: 3 home rigs, 3 contested rigs, and Improved Extraction, so it has 4 Shipyards. Building brawlers they spend about 33 Ore/s, and it holds about 3,300 Ore when the passive player's station falls, still gaining about 12 Ore/s.
- **The AI builds heavies once it can.** Large Hull lands at 6:35. In 4 of the 5 matches below one side or both built Large+Fusion+Lance or Large+Fusion+Mass Driver from then on, and what each side built changed from one 5-minute window to the next.
- **Two AIs on the mirrored map end a match in 7:51 to 14:54** over seeds 1 to 5, with §12 as retuned on 2026-10-01: 12:09, 11:49, 11:24, 14:54 and 7:51. Player 1 won 2 of the 5. All five are shorter than Q1's 15 to 25 minutes, but a human does not play as the AI does, so it is no answer to Q1.
- **The checks behind the AI** are:
  - Scripted snapshots: its first orders, its answer to each design of the triangle, and the reserve going to its Command Station and to a site under fire.
  - The real server: its base in order, its Shipyards by income, its answer to an enemy fleet, the attack at 12 ships, and the defence of an outpost.
  - A whole match against a passive player.
- **Not run yet.** The menu, the banner and the match flow are presentation and input. They are checked by the HUD's tests and by the owner's run (AGENTS.md §3).

## What this forecloses

- Tuning the AI in `Tuning.json`, or the server reading `Opponent.json`.
- The AI reading server state, or acting more often than once a second, without a new decision.
- A match that ends any other way, such as a time limit or a surrender, without a new decision.
- Snapshots that hide another player's components while fog of war does not exist.

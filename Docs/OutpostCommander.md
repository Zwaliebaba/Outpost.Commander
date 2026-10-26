# Outpost Commander

Status: **living** · what the game is at the head of main

This document says what the game is, rule by rule, as a player meets it in the build at the head of `main`. Each rule cites the ADR decision that implements it, as ADR-NNN decision N, and the ADR says how. It states no tuning number: the numbers are the data's, shown in the generated [Catalogue](Reference/Catalogue.md), and a rule links the Catalogue's row, as [Catalogue: rules](Reference/Catalogue.md#rules) does. A number the code fixes is stated where a player meets it, with the ADR decision that owns it. Each heading ends with its key in brackets, such as [§orders.scheduled]. A key is permanent, never renamed or reused, and it is how code and ADRs cite a section: Game §orders.scheduled.

---

## Overview [§overview]

Outpost Commander is a real-time strategy game in space, built on the model of Warzone 2100. Two players each start with a Command Station in opposite corners of a star system. They mine its asteroids for Ore, research components and upgrades, design their own warships from components, and use them to take the system's territory and destroy each other's production. Both play by one set of rules, with the same components, research and numbers, whoever plays them (ADR-008 decision 3).

**The loop.**

- **Ore** is the one resource. Mining Rigs earn it from asteroids in the sectors a player holds, and each asteroid's reserve runs dry, so income has to move outward over a match (Game §economy).
- **Territory** is income. A player holds a sector by a Relay on its node, territory grows one adjacent sector at a time, and an enemy hurts it by suppressing a Relay or cutting a sector off. In a match, holding fewer nodes drains a player's tickets (Game §territory).
- **Structures** are built by Constructors. The Command Station, the Shipyard and the Research Lab grow by levels, which buy the nodes, the fleet, the hulls and the research tiers a player can reach (Game §structures).
- **Ships** are designs: a hull, a drive, a weapon and a module or none, put together in the designer. The best design depends on what the enemy fields (Game §ships).
- **Combat** is decided by damage after armor, range and numbers: weapons are turrets, every hit lands, and armor takes a share off each (Game §combat).
- **Research** unlocks components and raises rates, in three tiers that the Research Lab's levels open (Game §research).
- **Pirates** guard outposts that keep their sectors from being claimed, and **derelicts** hold Ore and research to salvage (Game §neutrals).
- **Orders** are given now, kept by the server for a group, or scheduled for later, and every ship goes back to be repaired below its retreat threshold (Game §orders).
- **Fog of war** hides what no ship or structure of the player's sees, and the player remembers the enemy structures it has seen (Game §sight).

**Where it is played.**

- **A skirmish** is one match against the AI, at Easy, Normal or Hard, on a server inside the game (Game §session.skirmish; Game §ai).
- **A universe** is a match that does not end, run on a dedicated server for a circle of friends and AI empires. It keeps running while its players are away, a deputy keeps an absent player's empire going, and a player who loses starts again (Game §session.server; Game §universe).
- **Battle matchups and runs without a player** are instruments for the owner, not play (Game §session.matchup; Game §session.instruments).

**How a match ends.** A player whose base was placed is beaten when it has neither a Command Station nor a finished Shipyard, or when its tickets run out. The match ends once, with a winner or a draw (Game §session.end). In a universe no match ends: a fallen empire restarts at its start (Game §universe.restart).

**What decides.** A server decides everything, on a fixed tick, and every game sees only the snapshots it is sent: the player's game, the AI and a deputy give orders as commands, and the server checks and applies them (ADR-002 decisions 2 and 7; Game §orders.giving). A match replays from its seed and its commands on the same build (ADR-009 decision 1).

**How it looks.** A flat battlefield under an RTS camera, in the vector look of the eighties: dark low-poly models with bright lines on their ridges, over a black sky of stars and a faint grid (Game §look), with the HUD and floating windows drawn over it (Game §interface).

---

## Session [§session]

A session is everything between starting the game and closing it: the menu, a skirmish against the AI on the game's own server, a battle matchup, and a universe on a dedicated server that the game joins. The rules of a universe itself, its seats, its saves and what happens when a player loses in it, are Game §universe.

### Starting the game [§session.launch]

- **The game opens on its menu, full screen** (ADR-006 decision 1; Game §interface.screen).
- **With a join file in the player's documents, the game joins its universe at launch,** once, before it offers a skirmish: the menu says it is connecting, and a server that does not answer within 3 s leaves the menu with its notice and the skirmishes to play (ADR-078 decision 9; Game §session.join). The runs without a player and `--matchup` join nothing (Game §session.instruments; Game §session.matchup).
- **Everything a skirmish needs is loaded before the screen goes full screen.** The window stays hidden while the first skirmish's server, the AI's three settings files, the client's meshes and the interface's atlas are made on threads of their own, so that a bad tuning, map, AI or model file is reported before the game takes the screen (ADR-049 decisions 2 and 3; ADR-065 decision 4).
- **A failure is never a silent exit.** A failure at startup, a lost graphics device, or a failure on the skirmish server's thread is shown in a message box, and the game closes (ADR-006 decisions 7 and 8; ADR-025 decision 5).
- **The game needs Windows 11**, since every match goes over QUIC and QUIC needs TLS 1.3 (ADR-060 decision 8).

### The menu [§session.menu]

- **The menu offers, from top to bottom:** Easy skirmish, Normal skirmish and Hard skirmish (ADR-065 decision 4); Join universe, but only when the player's documents hold `Outpost Commander\Join.json` (ADR-078 decision 9); and Quit (ADR-020 decision 10). Its title is "Outpost Commander", over "A skirmish against the AI".
- **A notice says why the last game ended** when it ended for a reason the player did not choose, such as a join that failed or a lost connection, in the warning's color under the subtitle (ADR-078 decision 10).
- **The difficulty is picked per skirmish.** The game keeps the last one asked for while it runs, and starts each run at Normal; it is never remembered between runs and never changed in a match (ADR-065 decision 4 and what it forecloses). The difficulties themselves are Game §ai.difficulty.

### A skirmish [§session.skirmish]

- **A skirmish is one match against the AI** on a server of its own, which runs inside the game on a thread of its own. Both players reach it over QUIC on the loopback address, the player and the AI each taking a seat before the match starts (ADR-060 decision 1; ADR-025 decision 1; ADR-078 decision 4).
- **Each skirmish has its own server and its own seed,** the seed taken from the system clock (ADR-020 decision 10; ADR-009 decision 5). The seed places the map's content (Game §map), and the match replays from its seed and its commands on the same build (ADR-009 decision 1).
- **The player is player 1, and the AI player 2** at the difficulty the menu's button named (ADR-020 decision 10; ADR-065 decision 4). The AI plays by the player's rules and sees only what its player sees (Game §ai).
- **Every skirmish is played under fog of war** (ADR-024 decision 2; Game §sight). Each player starts with the starting Ore, its starting designs saved and its base at its start ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-014 decision 2; ADR-016 decision 3; Game §economy.ore; Game §ships.designs.starting; Game §map.starts).
- **The match runs at the tuning data's tick rate whatever the frame rate** ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-009 decision 3). It runs on while the game is minimized, while windows are open and while the frame loop stalls (ADR-025 decisions 1 and 2). A skirmish has no pause and no save: only a universe is saved (ADR-077 decision 7).
- **A tag in the top-right corner says "Local skirmish",** and warns when no snapshot has come for 2 s (ADR-086 decisions 1 and 2; Game §session.join).
- **The game logs every skirmish** to `OutpostCommander-matches.log` in the temporary folder: its seed, each player's research, each warship as it first appears, and how the match ended or that it was left (ADR-038 decisions 1 and 5).

### The end of a match [§session.end]

- **A match ends once a player whose base was placed is beaten:** by losing its production (Game §universe.loss) or by running out of tickets (Game §territory.domination). It ends once, in the tick it happens, with a winner, or with a draw when both players are beaten in the same tick (ADR-020 decision 9; ADR-037 decision 1; ADR-057 decision 5).
- **A player beaten both ways in one tick is beaten by production** (ADR-057 decision 5). The match records which ending it was (ADR-057 decision 6).
- **The simulation runs on after the match ends, and the outcome stands** (ADR-020 decision 9).
- **A banner says how it ended:** "Victory", "Defeat" or "Draw", and under it the match's length, after "By domination." for a domination, and a button, Back to menu (ADR-020 decision 9; ADR-057 decision 8).
- **Back to menu is the way out of a match.** It ends the match and its server, and the next skirmish's server is made as the menu comes back (ADR-025 decision 1; ADR-020 decision 10). A skirmish or a matchup that has not ended offers no way back to the menu (ADR-020 decision 9); closing the game leaves it, and the match log records it as left (ADR-038 decision 5). A universe is left from its leave window (Game §session.join).

### Battle matchups [§session.matchup]

- **A battle matchup is a fight between two fleets with no bases,** for measuring what a present player buys over the AI. `OutpostCommander --matchup <n>` makes every skirmish started from the menu matchup *n* of `Matchups.json`, counted from 1 ([Catalogue: matchups](Reference/Catalogue.md#matchups); ADR-083 decisions 1 and 6).
- **The map is placed without pirates, derelicts or fog of war,** and the two fleets stand in rows of five, 60 m apart, their front rows the matchup's distance apart and facing each other, across a sector's node along a line the seed picks (ADR-083 decision 2).
- **Each side's ships are of designs saved for it,** its starting designs where they match, and never go back to be repaired (ADR-083 decision 2).
- **The player plays player 1, and the AI's battle behavior plays player 2:** once a second its warships attack-move on the enemy ship nearest the middle of them, or on the nearest enemy structure when it sees no ship (ADR-083 decisions 4 and 6).
- **A matchup ends once a side's warships are all destroyed,** the other side winning, or as a draw after 600 s (ADR-083 decision 3). Its banner says "A fleet destroyed." or "Out of time." before the match's length (ADR-083 decision 3), and the match log names the matchup and how it ended (ADR-038 decision 1).

### The dedicated server [§session.server]

- **A universe runs on `OutpostServer`, a console program that is not packaged** and runs from a folder with the game's data beside it (ADR-078 decision 1).
- **`OutpostServer --new-universe <folder>` makes a universe:** its settings, `Universe.json`, with a seat for each of the map's starts. It refuses a folder that holds a universe already (ADR-078 decisions 1 and 2).
  - `--host` is the name or address players reach the machine at, `localhost` unless given; `--address` the address the server listens on, `0.0.0.0` unless given; `--port` its UDP port, 45000 unless given (ADR-078 decision 2); and `--seed` the universe's seed, the system clock's unless given (ADR-078 decision 1).
  - `--ai <player>[:Easy|Normal|Hard]` gives a seat to an AI empire, Normal unless named (ADR-078 decision 1; ADR-079 decision 7).
- **`OutpostServer <folder>` runs the universe,** coming back from its newest save (Game §universe.persistence). It hosts an AI empire for each AI seat and a deputy for each player's seat (ADR-079 decision 7; Game §universe.handover), and writes a join file, `Join-Player<n>.json`, for each player's seat into the folder each time it runs. The owner hands each player its file (ADR-078 decision 8).
- **Ctrl+C, Ctrl+Break, closing the console, signing out and shutting down all stop it,** and it writes the universe's last save as it stops (ADR-078 decision 1). A failure on the server's thread ends the process with its message (ADR-078 decision 1; ADR-025 decision 5).
- **A universe's certificate is kept in its folder,** so a player pins it once; its hash changes only when its key is lost or after five years (ADR-078 decision 7).
- **Reaching the port from another machine is the owner's to arrange,** through the firewall and the router (ADR-078).

### Joining a universe [§session.join]

- **The game joins a universe from a join file:** `--join <file>`, or the menu's Join universe, which reads `Outpost Commander\Join.json` in the player's documents; and at launch, once, when the documents hold that file (ADR-078 decision 9; Game §session.launch).
- **While a join connects, the menu says so,** "Connecting to 203.0.113.5:4433...", and only Quit can be pressed; the game keeps drawing, and the menu answers, until the server welcomes the player or the join fails (ADR-078 decision 9; ADR-086 decision 1). A join file holds the host, the port, the SHA-256 of the universe's certificate, the seat's token and the player (ADR-078 decision 8).
- **The game accepts only the certificate the join file names** (ADR-060 decision 6).
- **A joined universe is the player's connection alone:** no server, rival or AI runs in the game (ADR-078 decision 9). The player plays the seat as it plays a skirmish, under that universe's rules (Game §universe).
- **A seat is taken with its token, at any time while the universe runs** (ADR-078 decisions 3 and 4). The first snapshot after the seat is taken shows everything the player sees and remembers; the shots and destructions of the ticks it missed are not shown (ADR-078 decision 4).
- **The ground explored before is the game's own record and starts afresh:** everything out of sight is drawn as never seen until it is seen again (Game §sight.memory).
- **A join that fails goes back to the menu, saying why,** after "Could not join the universe:": a join file that cannot be read, a server that cannot be reached, within 3 s at launch and 10 s otherwise, or never seats the player, "The server refused the seat: it is not open there, or the join file's token is not the seat's." for a wrong token, and "The server runs another build of the game." for a game of another build (ADR-078 decisions 3 and 10; ADR-060 decision 5; ADR-086 decision 1).
- **A connection lost in play takes the player back to the menu, saying why,** after "The connection to the server was lost:", in the server's own words when the server closed it (ADR-078 decision 10).
- **The newest connection holds the seat.** A second game that joins with the same join file takes the seat, and the first goes back to the menu with "The seat was taken by another connection with its token." (ADR-078 decisions 4 and 10).
- **A tag in the top-right corner names the server,** "Server" and its host and port as the join file names them, an IPv6 host in brackets. Once no snapshot has come for 2 s it warns, "No word from the server", counting the seconds (ADR-086 decisions 1 and 2).
- **The way back to the menu from a universe is Esc, at any time:** with no window open and nothing armed, Esc opens the leave window, "Leave for the menu?", whose Leave universe goes back to the menu and whose Stay closes it. The connection closes at once, and the deputy takes the seat a minute later (ADR-078 decision 11; Game §universe.handover). Once the player's empire has fallen the banner offers the way back too (ADR-084 decision 7; Game §universe.restart). Closing the game leaves the seat to its deputy (Game §universe.handover).

### Runs without a player [§session.instruments]

These are instruments for the owner, not play.

- **`--measure`, `--load` and `--stress` skip the menu** and start a skirmish at once against the Normal AI; under `--load` or `--stress` no AI plays (ADR-020 decision 10; ADR-065 decision 4).
- **`--ai-matches` plays AI-against-AI matches headless,** stepped as fast as the server ticks, with no window, and logs them; its options name the seeds, each AI's settings and the log, and `--quiet` runs it for a script (ADR-038 decision 3; ADR-063 decisions 1 and 2).
- **`OutpostServer --universe-run <folder>`** plays a universe with an AI empire in every seat, kills it at random ticks and brings it back from its folder, and compares it with the same universe run straight through (ADR-082 decision 4).
- **`OutpostServer --matchups`** fights every battle matchup with the AI's battle behavior on both sides, over 40 seeds unless told otherwise, and says how often each side won (ADR-083 decision 5).

---

## The map [§map]

Every match and every universe is played on the one map the game ships: a square star system on a flat plane, divided into sectors, with the two starts in opposite corners. The map fixes its size, its sectors, the asteroid fields between them, the starts and each home's asteroids; the match's seed places the rest of the ore, the pirates' outposts and the derelicts, so that each seed plays a map of its own and both seats always play the same one (ADR-036 decision 4; ADR-072 decision 3). The map's numbers are data: [Catalogue: sizeMeters](Reference/Catalogue.md#sizeMeters).

### The plane [§map.plane]

- **Everything stands on one plane.** Ships, structures, ore asteroids, asteroid fields and derelicts all sit on it, and ships move and turn on it. Nothing in the rules has a height; a ship's lean in a turn is drawn by the client alone (ADR-010 decision 1; ADR-029 decision 2; Game §look.banking).
- **Every obstacle is a circle.** An ore asteroid, an asteroid field and every structure but a Mining Rig block movement as their whole circle, and ships path round them (ADR-010 decision 1; ADR-016 decision 8). A derelict blocks nothing (Game §neutrals.derelicts).
- **The edge bounds the map.** A structure must stand wholly inside it (ADR-016 decision 4). A ship sent beyond it stops at it, and a ship pushed past it is pulled back (ADR-010 decisions 3 and 7).
- **The ground is always known.** Every player sees every ore asteroid and field from the first tick, fog or not; what fog hides is what stands among them (ADR-024 decision 3; Game §sight.seen).

### Sectors [§map.sectors]

- **The map is a grid of square sectors.** Each is named by a column letter, from west to east, and a row number, from south to north, so that player 1's home is A1 and player 2's is the corner across from it ([Catalogue: sectors](Reference/Catalogue.md#sectors); ADR-036 decisions 1 and 4).
- **Each sector has one node, at its center**, and a home's node is its start. The node is where a sector is held from (ADR-036 decision 4; Game §territory.holding).
- **Two sectors are adjacent exactly when they share a border.** A sector's neighbors are the ones beside it, never the ones diagonal to it, and territory grows along them (ADR-036 decisions 1 and 2; Game §territory.lattice).
- **Every sector can be reached from both starts** (ADR-036 decision 2).
- The sectors' names are what the game calls places by, in the territory's marks, a rig's or a Relay's panel, and the alerts (Game §interface).

### Sector kinds [§map.kinds]

- **Each sector has a kind**, by how far it lies from the homes, and its kind says what ore it gets ([Catalogue: sectorKinds](Reference/Catalogue.md#sectorKinds); ADR-072 decision 1):
  - **home:** a player's start, with its home asteroids;
  - **flank:** the sectors next to a home;
  - **near:** the sectors one step further out;
  - **contested:** the sectors one step beyond those;
  - and on the diagonal between the two corners that hold no start: **center**, the middle sector; **between**, the sectors between the center and each of those corners; and **rich**, the corners themselves, equally far from both starts.
- **The yields rise outward.** Home sectors hold home asteroids, flank and near sectors near ones, the contested, between and center sectors contested ones, and the rich corners rich ones (ADR-072 decision 1; ADR-036 decision 3).
- **The kinds mirror through the center:** the sector across the center from any sector is of the same kind (ADR-072 decision 2).

### Starts and homes [§map.starts]

- **There are two starts, in opposite corners**, and each is a home sector's node ([Catalogue: starts](Reference/Catalogue.md#starts); ADR-036 decision 4).
- **A match puts each player's base on its start:** its Command Station, built and armed, and its starting Constructors in a row in front of it, facing the map's center (ADR-016 decision 3; Game §structures.station).
- **Each home has its own home asteroids**, the same in every match, listed in the map rather than drawn by the seed ([Catalogue: oreAsteroids](Reference/Catalogue.md#oreAsteroids); ADR-036 decision 4; ADR-072 decision 1).

### Ore asteroids [§map.asteroids]

- **An ore asteroid is where Ore comes from.** A Mining Rig is built on it, one to an asteroid (Game §economy.rigs).
- **Each has a yield, a reserve and a size.** The yield is home, near, contested or rich, and sets what a rig on it earns; the reserve is how much Ore it holds before it runs dry ([Catalogue: sectorKinds](Reference/Catalogue.md#sectorKinds), [Catalogue: oreAsteroids](Reference/Catalogue.md#oreAsteroids); ADR-036 decision 3; ADR-035 decision 1; Game §economy.reserves).
- **Its sector decides who may mine it**, since Ore belongs to sectors (Game §territory.gives).

### Asteroid fields and passages [§map.fields]

- **Asteroid fields are the walls between sectors.** They cannot be mined. Fields stand on every border between two sectors, leaving each border a passage at its middle and one at each end ([Catalogue: asteroidFields](Reference/Catalogue.md#asteroidFields); ADR-036 decision 4).
- **A field blocks its whole circle.** The rocks drawn inside it only show where it is; no ship passes between them (ADR-087 decision 3; Game §look.models.rocks).
- **No passage is too narrow for a ship.** Every gap between two asteroids or fields, between one and the edge, and between one and a start, is at least the map's minimum gap, and no hull is wider than that gap, so no ship can be walled off ([Catalogue: minimumGapMeters](Reference/Catalogue.md#minimumGapMeters); ADR-072 decision 5; ADR-010 decision 8).

### Placement from the seed [§map.placement]

- **The seed places the map before the first tick:** every ore asteroid but the home ones, then the pirates' outposts, then the derelicts. The seed places nothing once the match has begun; only a cleared outpost's wreck appears later (ADR-072 decisions 3, 7 and 8; ADR-074 decision 6; Game §neutrals.clearing).
- **Both seats get the same map.** One of each pair of sectors across the center is drawn and the other is its mirror through the center; the center draws half of its content and mirrors it within itself (ADR-072 decision 4).
- **A drawn asteroid stays out of the way.** It lies inside its sector, clear of the sector's border so that the passages stay open, clear of the node so that a Relay and its defenders have room, the minimum gap from every obstacle, the edge and each start, and far enough from every other ore asteroid that a rig has room for a Defence Platform beside it ([Catalogue: placement](Reference/Catalogue.md#placement); ADR-072 decision 5).
- **The same seed places the same map.** Placement depends on the map and the seed alone, so a match replays from its seed, and a new seed plays a new map: where in each sector its asteroids lie, which sectors hold a camp, and which hold a derelict and where (ADR-072 decision 3; Game §session.skirmish).
- **A battle matchup is played on the map without its pirates and derelicts** (ADR-083 decision 2; Game §session.matchup).

### Pirate outposts [§map.outposts]

- **Pirates hold some nodes from the start.** Camps stand in some of each player's contested sectors, which the seed draws, and strongholds in both rich corners and in the center; how many of each is data ([Catalogue: outposts](Reference/Catalogue.md#outposts); ADR-072 decision 7).
- **A sector holds at most one outpost**, and an outpost stands on its sector's node (ADR-072 decision 7; ADR-073 decision 4).
- What an outpost is made of, how it guards its sector and what it leaves when cleared are Game §neutrals.outposts, Game §neutrals.guarding and Game §neutrals.clearing.

### Derelicts [§map.derelicts]

- **Derelicts lie in the sectors between the homes and the middle:** in some of each player's flank, near and contested sectors, which the seed draws, and in the between sectors, each mirrored across the center. The rich corners and the center have none from the seed, since their strongholds leave a wreck when cleared. How many lie in each kind of sector, what each pays and the hull it lies as are data ([Catalogue: derelicts](Reference/Catalogue.md#derelicts); ADR-072 decision 8).
- **A sector holds at most one derelict from the seed**, which can lie in a sector a camp guards (ADR-072 decision 8; ADR-074).
- **A derelict lies clear of its sector's node**, inside its sector, and the minimum gap from every obstacle, start and other derelict (ADR-072 decision 8).
- **Some name a research topic.** A share of the mirrored pairs, drawn by the seed, each name a topic drawn from the whole research table, the same for both of the pair ([Catalogue: derelictResearchPercent](Reference/Catalogue.md#derelictResearchPercent); ADR-072 decision 8).
- What a derelict is and how it is salvaged are Game §neutrals.derelicts and Game §neutrals.salvage.

---

## The economy [§economy]

The game has one resource, Ore. Mining Rigs on ore asteroids earn it, salvage adds to it, and everything a player builds, queues, researches or upgrades is paid for with it. Each asteroid holds a reserve that runs dry, so a player's income has to move outward over a match, and how far it can move is decided by territory (Game §territory). A fleet cap, bought with the Command Station's level, bounds how many warships the Ore can turn into.

### Ore [§economy.ore]

- **Ore is the only resource.** Structures, ships, Constructors, research topics and structure levels all cost it, and nothing else is spent (ADR-016 decisions 4 and 6; ADR-017 decision 1; ADR-064 decision 2).
- **Each player starts with the starting Ore** ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-016 decision 3).
- **The server counts it in hundredths**, so that an income that is not a whole number of Ore a tick is paid in full; a player sees its Ore in whole Ore, rounded down (ADR-016 decisions 1 and 11). Where the HUD shows it is Game §interface.hud.ore.

### Costs [§economy.costs]

- **A structure is paid for when the order is given.** Its whole cost is taken at once and its site appears; an order the player cannot pay for is refused and costs nothing (ADR-016 decision 4).
- **A ship, a Constructor or a research topic is paid for when it starts.** It waits at the front of its queue until the player can pay, and what is queued behind it waits too (ADR-016 decision 6; ADR-017 decision 1). A warship also waits for room under the fleet cap (Game §economy.fleetcap).
- **A structure's next level is paid for when it is ordered** (ADR-064 decision 2; Game §structures.levels.upgrade).
- **Nothing is refunded, and nothing paid for can be canceled.** A site destroyed before it is finished, a structure destroyed while its next level is being built, and a topic under way when its Lab is destroyed all lose their Ore (ADR-016 decision 6; ADR-064 decision 5; ADR-017 decision 1).
- **Repair and salvage cost nothing** (ADR-016 decision 5; ADR-075 decision 3; Game §neutrals.repair).
- What each thing costs is data: [Catalogue: structures](Reference/Catalogue.md#structures), [Catalogue: constructor](Reference/Catalogue.md#constructor), [Catalogue: hulls](Reference/Catalogue.md#hulls), [Catalogue: drives](Reference/Catalogue.md#drives), [Catalogue: weapons](Reference/Catalogue.md#weapons), [Catalogue: modules](Reference/Catalogue.md#modules) and [Catalogue: research](Reference/Catalogue.md#research).

### Mining Rigs [§economy.rigs]

- **A Constructor builds a Mining Rig on an ore asteroid.** The order snaps to the asteroid whose edge is within 40 m of the point given (ADR-016 decision 4).
- **One rig stands on an asteroid.** It stands at the asteroid's center and covers it, so Constructors and attackers reach it from the asteroid's edge, and it blocks nothing the asteroid does not already block (ADR-016 decisions 4 and 8).
- **A rig may be ordered only onto an asteroid in a sector its player holds** (ADR-056 decision 5; Game §territory.gives).
- **A rig earns nothing until it is finished** (ADR-016 decision 5).
- **The reserve is the asteroid's, not the rig's.** A destroyed rig frees its asteroid for either player, and a rig built on it again mines what is left (ADR-035 decision 2).
- Its numbers are data ([Catalogue: structures](Reference/Catalogue.md#structures)).

### Income [§economy.income]

- **A finished rig earns its asteroid's yield every second**: home, near, contested or rich, each with its own rate ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-036 decision 3).
- **Research raises it.** Improved Extraction multiplies every rig's income (ADR-017 decision 2; Game §research.upgrades).
- **Territory takes its share.** A rig earns all of its income in a sector its player holds, the cut-off share in a sector that is cut off, and nothing in a sector that is suppressed or that its player does not hold (ADR-056 decision 5; Game §territory.gives).
- **A dry asteroid pays the trickle** (Game §economy.reserves).
- **A player's income is the sum of its rigs'.** It is paid every tick, a tick's share at a time, with what is left over carried to the next tick, so no fraction is lost (ADR-016 decisions 1 and 11). The income a player sees is what its rigs earn now, with research, territory and the trickle taken into account (ADR-016 decision 1).

### Reserves and the trickle [§economy.reserves]

- **Every ore asteroid holds a reserve** of Ore ([Catalogue: sectorKinds](Reference/Catalogue.md#sectorKinds), [Catalogue: oreAsteroids](Reference/Catalogue.md#oreAsteroids); ADR-035 decision 1).
- **A rig draws from its asteroid what it earns.** When the reserve is gone, the rig earns the trickle, a share of its yield, for the rest of the match, and the trickle draws nothing. A reserve's last tick still pays the full rate ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-035 decisions 2 and 3).
- **A rig that earns less draws less.** A rig in a cut-off sector draws only what it earns there, and one that earns nothing, in a sector suppressed or not held, draws nothing (ADR-056 decision 5).
- **Improved Extraction drains a reserve faster**, since it raises what the rig earns. **Deep Core Survey stretches it**: its player's rigs draw less from the reserve for the same Ore, from what is left as well as what is to come. Its effect is its researcher's alone, on whatever asteroid that player mines (ADR-035 decision 3; ADR-033 decision 4).
- **Nothing refills.** An asteroid that has run dry pays the trickle for good (ADR-035).
- **A player knows a reserve as it last saw it.** The figure is brought up to date whenever the asteroid is in sight of one of the player's ships or structures, or in a sector it holds that is not suppressed; an asteroid it has never seen has no figure (ADR-035 decision 4; ADR-056 decision 9). A rig's panel says how much Ore is left, or that its asteroid has run out and it earns a trickle, and the minimap draws a dry asteroid in rust (ADR-035 decision 5; ADR-043 decision 8; Game §interface.minimap).

### The bank [§economy.bank]

- **The bank is the Ore a player holds.** Its rigs and its salvage fill it, and what it starts and builds empties it (Game §neutrals.salvage).
- **It has no limit, and nothing drains it.** There is no upkeep on a fleet and no other charge over time (ADR-071; ADR-016). Ore that the fleet cap and the queues leave unspent stays in the bank.
- **In a universe, a seat that restarts keeps its bank**, raised to the starting Ore when it holds less (ADR-084 decision 4; Game §universe.restart). The universe log records each seat's bank every hour (ADR-082 decision 3; Game §universe.log).

### The fleet cap [§economy.fleetcap]

- **Each warship takes command points by its hull** ([Catalogue: hulls](Reference/Catalogue.md#hulls)). A Constructor takes none (ADR-071 decision 1).
- **The Command Station's level sets the cap** ([Catalogue: structures](Reference/Catalogue.md#structures)), so a player buys a larger fleet by upgrading its station (ADR-071 decision 1; Game §structures.station). A player without a Command Station has level 1's cap (ADR-071 decision 1).
- **A warship counts from the moment its job starts.** A player's points are those of its warships and of every warship job one of its Shipyards has started and paid for, so two Shipyards cannot both start the last ship that fits (ADR-071 decision 2).
- **A job that would go over the cap waits.** It waits at the front of its Shipyard's queue, neither paid for nor started, and the jobs behind it wait too, as they do behind a job short of Ore (ADR-071 decision 3; ADR-016 decision 6).
- **A queued job is never refused for the cap**, so a player can queue ahead of a battle's losses, and **the cap never removes a ship**: a player whose station falls keeps every ship it has, and starts no warship that would take it over level 1's cap (ADR-071 decision 3).
- **The player sees it.** The snapshot carries its points and its cap, each hull's points and each station level's cap (ADR-071 decision 4). The HUD's fleet line and the production window show them, and say when the Shipyards wait for the cap (ADR-066 decision 1; ADR-071 decision 4; Game §interface.hud.status).
- The pirates have no cap, since they build nothing (ADR-073 decision 1; Game §neutrals.pirates).

---

## Territory [§territory]

Territory is what the two players fight over between their bases. Each sector of the map has a node (Game §map.sectors), and a player holds a sector by standing a Relay or its Command Station on the node. Holding a sector is what lets a player mine its asteroids and build Shipyards in it, shows the player the whole sector, and, in a match, counts toward domination. Territory grows outward from home, one adjacent sector at a time, and an enemy hurts it without taking it by suppressing a Relay or cutting a sector off. Every match plays it, since the map has sectors (ADR-056 decision 1).

### Holding a sector [§territory.holding]

- **A sector is held by whoever owns the finished Relay or Command Station on its node** (ADR-056 decision 3).
- **A home sector is one a Command Station holds.** Each player holds its home from the first tick, since its station stands on its start, the home's node (ADR-056 decision 3; Game §map.starts).
- **A Relay under construction holds nothing yet**, though it already takes its node (ADR-056 decisions 3 and 4).
- **A node nobody holds counts for no one**, and nor does one the pirates guard (ADR-057 decision 3; Game §neutrals.guarded).
- **Territory is worked out at the end of every tick**, once every ship has moved, and whenever a Command Station or a Relay is placed whole (ADR-056 decision 8).

### Claiming a node [§territory.claim]

- **A Constructor claims a node by building a Relay** in its sector, and the order puts the Relay on the node wherever in the sector it points ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-056 decision 4).
- **A claim is refused, at no cost,** when:
  - something already stands on the node, such as another Relay, a site or a camp's wreck (ADR-056 decision 4; ADR-074 decision 6);
  - no sector adjacent to it is the player's (ADR-056 decision 4; Game §territory.lattice);
  - a pirate structure stands in the sector (ADR-073 decision 6; Game §neutrals.guarded);
  - the player has taken as many nodes as its node cap allows (ADR-056 decision 4; Game §territory.cap).
- **The game shows where a claim would be taken now.** While the player places a Relay, its ghost is green only on such a node, and the world and the minimap mark each node the player could claim, all by the one rule, `CanClaim` (ADR-081 decision 2; Game §interface.marks).
- **A Relay is unarmed**, so a node is held against an attack by what stands beside it (ADR-056 decision 2; Game §structures.kinds).

### The lattice [§territory.lattice]

- **A Relay can only be built in a sector adjacent to one its player holds.** Territory therefore grows outward from the home sector, and is taken back along a front, never leapfrogged (ADR-056 decision 4).
- **A suppressed Relay still counts for adjacency**, since it still holds its sector (ADR-056 decision 4).
- **Any Relay can be attacked**, however deep in the enemy's territory. A raid can suppress a sector or destroy its Relay, but it can claim the node only if the sector is next to its own (ADR-056 decision 4; Game §combat.structures).

### The node cap [§territory.cap]

- **The Command Station's level caps the nodes a player may take**, its home included ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-064 decision 11; Game §structures.station).
- **A Relay site counts toward the cap from the moment it is ordered**, as it takes its node (ADR-056 decision 4).
- **The cap refuses claims; it never takes a node away** (ADR-056 decision 4).
- **A player without a Command Station has level 1's cap.** It keeps every Relay it has, and claims no new node while it holds that many or more (ADR-056 decision 4).

### What holding gives [§territory.gives]

- **Ore belongs to sectors.** A Mining Rig may be ordered only onto an asteroid in a sector its player holds, a suppressed one included, and a rig earns only while its player holds the sector: all of its income when the sector is linked to home, the cut-off share when it is cut off, and nothing while it is suppressed (ADR-056 decision 5; Game §economy.income).
- **Production follows territory.** A Shipyard or a Repair Bay may be ordered only in a sector its player holds, however far from home, so that production moves forward as the territory does. So may a Research Lab and a Defence Platform, so that neither creeps into another player's sectors. One that stands in a sector its player loses keeps working (ADR-056 decision 5; ADR-075 decision 1; Game §structures.building).
- **A held sector is in sight.** A sector its player holds and that is not suppressed is in that player's sight, all of it: its ships and structures, its asteroids' reserves and what is destroyed in it (ADR-056 decision 9; Game §sight.range).

### Suppression [§territory.suppression]

- **A Relay is suppressed while an enemy warship is within the suppression radius of it and none of its owner's warships is** ([Catalogue: territory](Reference/Catalogue.md#territory)). Distances are between centers, as weapon range is (ADR-056 decision 6).
- **Only warships suppress.** A Constructor never does. A pirate warship suppresses as an enemy's does (ADR-056 decision 6; ADR-073 decision 6).
- **A Command Station is never suppressed**, so a home sector always earns and is always in sight (ADR-056 decisions 6 and 9).
- **A suppressed sector earns nothing**, and its Relay sees only as far as an unarmed structure does ([Catalogue: sight](Reference/Catalogue.md#sight); ADR-056 decisions 5 and 9).
- **A suppressed Relay is still held.** It keeps its sector for the lattice, for the path home and for domination, until it is destroyed (ADR-056 decisions 4 and 7; ADR-057 decision 3).
- **Suppression lasts while the enemy stays**: it is worked out again at the end of every tick (ADR-056 decision 8).

### Being cut off [§territory.cutoff]

- **A held sector is cut off when its holder holds no path of adjacent sectors from it to one of its home sectors.** Suppressed sectors keep the path, so only a lost sector breaks one (ADR-056 decision 7).
- **A cut-off sector earns the cut-off share** of its rigs' income ([Catalogue: territory](Reference/Catalogue.md#territory)) until the link is restored (ADR-056 decision 5). It is still held, still in sight and still counts for domination (ADR-056 decision 9; ADR-057 decision 3).
- **A player whose Command Station has fallen has no home sector**, so every sector it still holds is cut off (ADR-056 decision 7; Game §universe.loss).

### Losing a sector [§territory.loss]

- **A sector is lost when the Relay or Command Station on its node is destroyed.** The node is free again, for either player to claim by the same rules, and the rigs in the sector stop earning until their player holds it again (ADR-056 decisions 3 and 5).
- **A destroyed Relay leaves nothing behind**; only a pirate outpost leaves a wreck (ADR-074 decision 6; Game §neutrals.clearing).

### What every player sees [§territory.shown]

- **Every player sees all of the territory, fog or not**: every sector's holder, and whether it is suppressed, cut off or guarded by pirates. The territory is the map both sides play for, and domination counts it. The Relays themselves are seen only as any structure is (ADR-056 decision 10; ADR-073 decision 7).
- **The world, the minimap and the territory panel show it**, and the alerts tell a Relay suppressed or attacked and an enemy entering a held sector (ADR-081 decisions 3 and 7; ADR-057 decision 8; ADR-059 decision 1; Game §interface.alerts).

### Tickets and domination [§territory.domination]

- **In a match, each player starts with tickets** when the bases are placed ([Catalogue: territory](Reference/Catalogue.md#territory); ADR-057 decisions 1 and 4).
- **Holding fewer nodes drains them.** At the end of every drain interval, each player loses the drain's tickets for each node it holds fewer than the player that holds the most. The map's size does not change the rate, and at equal nodes nothing drains (ADR-057 decisions 2 and 3).
- **What counts:** every sector a player holds, its home and its suppressed and cut-off sectors included. A node nobody holds, one the pirates guard, and a Relay site still being built count for no one (ADR-057 decision 3; ADR-056 decision 3).
- **A player out of tickets loses.** The match ends once, with the simulation running on, and records that it ended by domination. A player who loses its production and its last tickets in the same tick loses by production (ADR-057 decisions 5 and 6; Game §session.end).
- **Every player sees both players' tickets** and the drain's numbers, and the territory panel says who the drain takes from, how fast, and when they run out (ADR-057 decisions 7 and 8; Game §interface.hud.territory).
- **A universe has no tickets and no domination**: a universe ends only when its owner stops it (ADR-084 decision 2; Game §universe.overview).

---

## Structures [§structures]

A player's base is its structures. Its Command Station stands on its start from the first tick, and its Constructors build everything else: Shipyards for warships, a Research Lab, Mining Rigs for Ore, Defence Platforms to hold what it has, Relays to claim territory and Repair Bays to keep its fleet. Three kinds grow. The Command Station, the Shipyard and the Research Lab are upgraded a level at a time, each level building itself while the structure keeps working, and what a level gives makes an upgraded structure a target worth a raid: it is lost with the structure. What a structure costs is Game §economy, how it fights Game §combat, and how a Constructor is ordered to build Game §orders.

### The kinds [§structures.kinds]

- **There are seven kinds of structure** (ADR-016 decision 2; ADR-056 decision 2; ADR-075 decision 1):
  - the **Command Station**, the base, which builds Constructors and carries the Defence gun (Game §structures.station);
  - the **Shipyard**, which builds warships of the player's saved designs (Game §structures.shipyard);
  - the **Research Lab**, which researches (Game §structures.lab; Game §research);
  - the **Mining Rig**, which stands on an ore asteroid and earns its Ore (Game §economy.rigs);
  - the **Defence Platform**, a Defence gun that does not move (Game §structures.platform);
  - the **Relay**, which claims and holds a sector's node, unarmed (Game §territory.claim);
  - the **Repair Bay**, which repairs the ships near it (Game §neutrals.repair.bay).
- **Every kind's numbers are data**: its hit points, armor, footprint, cost and the Constructor time it takes ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-008 decision 2; ADR-016 decision 2). Both players build from the same numbers. The footprints are final (owner, 2026-10-01; ADR-016 decision 2).
- **The armed structures, the Relay and the Repair Bay have armor; the Shipyard, the Research Lab and the Mining Rig have none**, so a raid on production, research or a rig meets no armor ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-008 decision 2; ADR-016 decision 7).
- **A structure is never repaired by itself.** Constructors repair it on an order (Game §neutrals.repair.constructors).
- **The pirates' outposts stand round Defence Platforms**, at the base numbers, with no research (ADR-073 decisions 1 and 3; Game §neutrals.outposts).

### Building a structure [§structures.building]

- **A Constructor builds every kind but the Command Station.** An order to build a Command Station is refused (ADR-016 decision 4; ADR-037 decision 2). How the order is given, and the ghost that shows where a structure may stand, are Game §orders.constructors.
- **The build menu offers every kind the selected Constructors can build, with its cost.** A button that cannot be pressed for a reason other than its cost says why in its cost's place: the Research Lab's says ONE PER PLAYER once the player has one, and the Relay's NODE CAP at the node cap. A button the player only cannot afford is dim with its cost (ADR-043 decision 4; ADR-056 decision 11; Game §interface.hud.buttons).
- **The structure is paid for when the order is given.** Its whole cost is taken at once, and an order the player cannot pay for is refused at no cost (ADR-016 decision 4; Game §economy.costs).
- **Where a structure may stand** (ADR-016 decision 4):
  - its footprint stays inside the map's edge, and overlaps no asteroid, no asteroid field, no structure of either player and no derelict (ADR-074 decision 1);
  - a Mining Rig stands instead where Game §economy.rigs says;
  - a Relay snaps to its sector's node (ADR-056 decision 4; Game §territory.claim);
  - a Mining Rig, a Shipyard, a Repair Bay, a Research Lab and a Defence Platform may stand only in a sector the player holds, so production and defense follow territory (ADR-056 decision 5; ADR-075 decision 1; Game §territory.gives);
  - how many Research Labs a player may have is Game §structures.lab.
- **Ships do not stop a placement.** A ship that the new site covers is pushed out of it (ADR-016 decision 4; ADR-054 decision 8).

### Sites [§structures.sites]

- **A site appears when the order is given.** It blocks movement from then on and can be attacked, and it starts with a tenth of its hit points (ADR-016 decision 4).
- **Constructors build it.** A Constructor works once its footprint is within 20 m of the site's. One Constructor takes the structure's time, and each further one on the site adds a share of one more ([Catalogue: constructor](Reference/Catalogue.md#constructor); ADR-016 decision 5). Rapid Construction raises the rate (ADR-033 decision 4; Game §research.upgrades). Further Constructors join a site as they join a repair (Game §orders.constructors).
- **The site's hit points rise with the work, from a tenth to full**, so damage taken while it is built stays taken (ADR-016 decision 5).
- **A structure under construction does nothing but stand, block and take damage.** It does not produce, research, fire, repair ships, earn or hold a sector (ADR-016 decision 5; ADR-056 decision 3; ADR-075 decision 3). A Research Lab site and a Relay site count toward their limits (Game §structures.lab; Game §territory.cap).
- **A site destroyed before it is finished loses its Ore**; nothing is refunded (ADR-016 decision 6; Game §economy.costs).

### Hit points and armor [§structures.hitpoints]

- **A structure's hit points are its kind's, raised by its player's research and by its level** ([Catalogue: structures](Reference/Catalogue.md#structures), [Catalogue: rules](Reference/Catalogue.md#rules); ADR-033 decision 4; ADR-064 decision 4).
- **Reinforced Structures applies to every structure its player has**, when it is placed and when the topic finishes. One standing keeps its share of its hit points, so an undamaged one gains the whole upgrade (ADR-033 decision 4; Game §research.upgrades).
- **Armor never changes** (Game §combat.hits).

### Blocking [§structures.blocking]

- **Every structure but the Mining Rig blocks movement** as its footprint's circle; a rig stands on its asteroid, which already blocks. Ships path round them, and ships whose way a new structure blocks search again (ADR-016 decision 8; Game §orders.movement).
- **A destroyed structure stops blocking**, and the paths round it are brought up to date (ADR-016 decision 8; ADR-054 decisions 5 to 7; Game §combat.destruction).
- **A structure's footprint never changes with its level**, so neither does where it may stand or how ships path round it (ADR-064 decision 8).

### Production [§structures.production]

- **The Command Station builds Constructors and a Shipyard builds warships.** Each holds a queue of up to five jobs (ADR-016 decisions 6 and 10). Only a finished one takes a job.
- **A Shipyard queues only the player's saved designs, of a hull its level builds** (ADR-016 decision 6; ADR-064 decision 9; Game §ships.designs; Game §structures.shipyard).
- **A job waits at the front of its queue until it can start.** The front job starts when the player can pay for it, and a warship's also when it fits under the player's fleet cap. It is paid for then. Until then it waits, and the jobs behind it wait too (ADR-016 decision 6; ADR-071 decisions 2 and 3; Game §economy.fleetcap).
- **A job takes its design's build time, which is its hull's, or the Constructor's** ([Catalogue: hulls](Reference/Catalogue.md#hulls), [Catalogue: constructor](Reference/Catalogue.md#constructor); ADR-016 decision 6; ADR-014 decision 1). Automated Shipyards and Fleet Automation speed a Shipyard's jobs, the one under way included, and not the Command Station's (ADR-017 decision 2; Game §research.upgrades). The next job starts on the tick after one ends (ADR-016 decision 6).
- **A finished ship appears just beyond its producer's footprint, on the side facing the map's center**, moved clear of obstacles, and stands there until it is ordered. There is no rally point (ADR-016 decision 6).
- **Nothing cancels a job.** A producer destroyed takes its queue with it, and the Ore of the job under way (ADR-016 decision 6; Game §economy.costs).
- **A producer keeps producing while its next level is built** (Game §structures.levels.upgrade).
- **The player queues a job in the production window**, which shows one producer's queue and what it can add, and says when the front job waits for Ore or for the fleet cap. A job the player only cannot afford now is dim with its cost (ADR-031 decision 6; ADR-043 decision 4; Game §interface.windows.production). The selection panel of a producer says what it is building, or what it waits for (ADR-066 decision 4; Game §interface.hud.selection).

### Armed structures [§structures.armed]

- **The Command Station and the Defence Platform carry the Defence gun** ([Catalogue: structureWeapons](Reference/Catalogue.md#structureWeapons)). Once built, each fires by the rules ships fire by (ADR-016 decision 7; Game §combat.structures).
- **The Command Station gains guns with its levels** (ADR-064 decision 11; Game §structures.station).
- **Defence Autoloader raises the gun's fire rate**, on every armed structure its player has (ADR-033 decision 4; Game §research.upgrades).
- **Where the Defence gun's range stands among the weapons' is Game §combat.range.**
- **Every other kind is unarmed** and fights only by being shot at (ADR-016 decision 7; ADR-056 decision 2).

### Levels [§structures.levels]

- **The Command Station, the Shipyard and the Research Lab have levels**, from 1 to their kind's top level ([Catalogue: structures](Reference/Catalogue.md#structures)). No kind goes past level 5, the art's last, and every other kind stays at level 1 (ADR-064 decision 1).
- **What a level gives is its kind's**: nodes, guns and fleet for the Command Station (Game §structures.station), hulls for the Shipyard (Game §structures.shipyard), and a research tier or a second research slot for the Lab (Game §structures.lab).

#### Upgrading [§structures.levels.upgrade]

- **A structure is upgraded one level at a time.** The player orders the next level from the structure's selection panel; no level is skipped (ADR-064 decisions 2 and 7).
- **The level is paid for when it is ordered**, and nothing is refunded (ADR-064 decision 2; Game §economy.costs).
- **An upgrade is refused, at no cost** (ADR-064 decisions 2 and 10):
  - for anything that is not one of the player's own structures, or a kind without levels;
  - for a site still under construction;
  - for a structure whose next level is already being built;
  - at the kind's top level;
  - for a Research Lab whose next level requires topics the player has not researched, before its Ore is looked at;
  - when the player cannot pay.
- **A level builds itself over its time.** No Constructor works on it and no research shortens it (ADR-064 decision 3).
- **The structure keeps working while its next level is built:** it produces, researches, fires and repairs ships throughout (ADR-064 decision 3).
- **A Constructor repairs a damaged structure while it is upgraded**, as it repairs any other; its order ends when the structure is whole (ADR-064 decision 4; Game §neutrals.repair.constructors).

#### What a level adds [§structures.levels.gives]

- **Each level above the first adds a share of the kind's base hit points**, which adds to research's share rather than multiplying it ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-064 decision 4).
- **A level lands whole.** Its hit points arrive when the level is in, and the structure keeps the share of them it had (ADR-064 decision 4).
- **A level never adds armor**, and never changes the footprint (ADR-064 decisions 4 and 8).

#### Losing a level [§structures.levels.loss]

- **A destroyed structure takes its levels with it**, and the Ore of a level under way. One built where it stood is a new structure, at level 1 (ADR-064 decision 5).

#### Seen by others [§structures.levels.shown]

- **Whoever sees a structure sees its level**, and how far its next level has come (ADR-064 decision 6).
- **An enemy structure out of sight is remembered at the level last seen**, so an upgrade made out of the player's sight shows only once the player sees the structure again (ADR-064 decision 6; Game §sight.memory).
- **A structure is drawn at its level**, within level 1's ground (ADR-064 decision 8; Game §look.models.size).

#### The panel [§structures.levels.panel]

- **A structure that grows is named with its level**, "Shipyard 01 · L2", and while a level is being built its panel says "Upgrading to L3, 41%" (ADR-064 decision 7).
- **The player's own finished structure has a line for what the next level gives**, and an Upgrade button, "Upgrade to L3", with the level's time and cost (ADR-064 decision 7):
  - a Command Station's line names the nodes the level lets the player hold, the Defence guns it brings, its fleet cap and its hit points (ADR-064 decision 11; ADR-071);
  - a Shipyard's names the hulls it adds and its hit points, "L2: Medium hulls" (ADR-064 decision 9);
  - a Research Lab's names its tier or its second slot and its hit points, and the topics it still needs, "L2 needs Improved Extraction" (ADR-064 decision 10).
- **The button is dim while a level is being built**, with "UPGRADING · 41%" and a bar along its foot that fills from left to right as the level builds; dim with NEEDS RESEARCH while a Lab's level waits for topics; and dim on its cost while the Ore is short (ADR-064 decisions 7 and 10).
- **At the top level the panel says "Top level"** and has no button (ADR-064 decision 7).

### The Command Station [§structures.station]

- **Each player's Command Station stands on its start from the first tick**, built and armed, with the starting Constructors in a row in front of it, facing the map's center. It stands on its home sector's node and holds that sector (ADR-016 decision 3; ADR-056 decision 3; Game §map.starts; Game §territory.holding).
- **No Constructor builds one.** A Command Station lost in a match is lost for good; in a universe, a seat that restarts is given a new one (ADR-016 decision 4; ADR-037 decision 2; Game §universe.restart).
- **It is the only place Constructors come from** (ADR-016 decision 6; Game §ships.constructor). A player without one keeps the Constructors it has and builds no more (ADR-037 decision 2).
- **Its level sets three caps** ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-064 decision 11; ADR-071 decision 1):
  - the nodes its player may hold (Game §territory.cap);
  - the Defence guns it carries;
  - the command points its player's warships may take (Game §economy.fleetcap).
- **Each further gun is the station's gun, on a rhythm of its own**, firing at the target the station holds (ADR-064 decision 11; Game §combat.structures).
- **It repairs the ships near it**, as a Repair Bay does (ADR-075 decision 3; Game §neutrals.repair.base).
- **A player without a Command Station has level 1's caps**, has its finished Shipyards revealed to its enemy, and has lost once it also has no finished Shipyard (ADR-037 decisions 1 and 3; ADR-056 decision 4; ADR-071 decision 1; Game §universe.loss).
- **What it is for:** armed and armored, it destroys a handful of early raiders that would otherwise end a match in its first minutes (ADR-016 decision 7).

### The Shipyard [§structures.shipyard]

- **A Shipyard builds warships of its player's saved designs** (ADR-016 decision 6; Game §structures.production).
- **Its level sets the hulls it builds.** The tuning data names the hulls it builds at level 1 and the hull each further level adds, and it builds the hulls of its level and every level below ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-064 decision 9).
- **A job whose hull is above the Shipyard's level is refused.** A job already queued is never taken out, since a standing Shipyard's level only rises (ADR-064 decision 9).
- **A player needs both the research and the level for the Large hull:** the Large Hull topic decides whether a Large design can be saved, and a Shipyard's level where one is built (ADR-064 decision 9; Game §research.unlocks).
- **A Shipyard may be built only in a sector its player holds**, and one that stands in a sector its player loses keeps working (ADR-056 decision 5; Game §territory.gives).
- **Shipyards are numbered** in the order they are finished, 01, 02 and on, for each player, and a number is not reused. Only their owner sees a Shipyard's number and the ships it has built (ADR-016 decision 6).
- **It repairs the ships near it** (ADR-075 decision 3; Game §neutrals.repair.base).
- **The designer and the production window say what a Shipyard cannot build:** "Needs Shipyard L2" in the designer, and a design's code followed by "NEEDS L2" in the production window (ADR-064 decision 9; Game §ships.designer.queue).

### The Research Lab [§structures.lab]

- **A player may have one Research Lab**, one under construction counting (ADR-016 decision 4).
- **What it researches, and when, is Game §research.**
- **Its levels open research tiers, and a further level gives a second research slot** ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-064 decision 10; Game §research.tiers; Game §research.slots).
- **A level of the Lab may require research.** The upgrade is refused until its topics are researched, and the panel names them (ADR-064 decision 10).
- **A Lab destroyed loses its levels,** and a Lab built again starts at level 1, so it researches only the first tier until it is upgraded again (ADR-064 decision 5; ADR-033 decision 5). What its research loses is Game §research.lab.

### The Defence Platform [§structures.platform]

- **A Defence Platform is a Defence gun that does not move**, armored, with no levels ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-016 decision 7; ADR-064 decision 1).
- **It is how a player fortifies a node, a rig or its base.** A lone platform outlasts a light raid and destroys it, and breaking one takes heavier weapons that take its fire while they do, or the weapon that outranges it (ADR-016 decision 7; Game §combat.range).

---

## Ships [§ships]

Every warship is a design: a hull, a drive and a weapon, and a module or none. The player puts designs together in the designer, saves them, and has a Shipyard build ships of them. A design's numbers follow from its components, research changes them for the player who has it, and the best design depends on what the enemy fields. The Constructor is the one ship of no design: it builds, repairs and salvages, and fights no one. How ships move is Game §orders.movement, what they fire at Game §combat, and what unlocks a component Game §research.

### Warships and Constructors [§ships.kinds]

- **A ship is a warship or a Constructor.** A warship is built at a Shipyard to one of its player's saved designs; a Constructor has no design and is built at the Command Station (ADR-016 decision 6; Game §structures.production).
- **Both players draw on the same components**: the same hulls, drives, weapons and modules, with the same numbers, unlocked by the same research ([Catalogue: hulls](Reference/Catalogue.md#hulls), [Catalogue: drives](Reference/Catalogue.md#drives), [Catalogue: weapons](Reference/Catalogue.md#weapons), [Catalogue: modules](Reference/Catalogue.md#modules); ADR-008 decision 3; ADR-017 decision 4).
- **A warship takes command points by its hull, and a Constructor none** (ADR-071 decision 1; Game §economy.fleetcap).
- **A ship's design reads on sight:** its hull by its model, its drive by its exhaust's color and its weapon by its shots (ADR-019 decision 3; ADR-034 decision 1; Game §look.exhaust; Game §look.shots). Every player's snapshot carries the components of each warship it sees, whoever owns it (ADR-020 decision 3).

### Hulls [§ships.hulls]

- **A hull sets a ship's hit points, armor, speed, turn rate, footprint, cost, build time and command points** ([Catalogue: hulls](Reference/Catalogue.md#hulls); ADR-014 decision 1; ADR-010 decision 8; ADR-071 decision 1).
- **There are three: Small, Medium and Large.**
  - **Small** is cheap, quick to build and fragile.
  - **Medium** is the all-rounder, armored enough that small hits break on it.
  - **Large** is slow, dear and very tough.
- **A design's build time is its hull's** ([Catalogue: hulls](Reference/Catalogue.md#hulls); ADR-014 decision 1).
- **The footprint is the hull's size** for moving, forming up and what a splash reaches, not its mesh's outline. The footprints and the turn rates are final (owner, 2026-10-01; ADR-010 decision 8; Game §combat.splash).
- **The Large hull is unlocked by research**, and a Shipyard builds a Medium or a Large hull only from the level that adds it (ADR-017 decision 4; ADR-064 decision 9; Game §structures.shipyard).
- **Research raises every hull's hit points and speed** for its player (ADR-017 decision 2; ADR-033 decision 4; Game §research.upgrades).

### Drives [§ships.drives]

- **A drive multiplies its hull's speed, hit points and turn rate, and adds its cost** ([Catalogue: drives](Reference/Catalogue.md#drives); ADR-014 decision 1). It has no armor of its own and no mesh: a ship is drawn with its hull's model (ADR-014 decision 1; Game §look.models.sets).
- **The drive trades mobility for durability:**
  - the **Ion Drive** is fast, turns quickly and is lightly protected, and is open from the start;
  - the **Fusion Drive** is slow, much tougher and dearer;
  - the **Pulse Drive** is the raider's: the fastest and the most fragile.
- **Research unlocks the Fusion and the Pulse Drive** (ADR-017 decision 4; Game §research.unlocks).

### Weapons [§ships.weapons]

- **A weapon sets its hit, its fire interval, its range, its splash and its cost** ([Catalogue: weapons](Reference/Catalogue.md#weapons); ADR-014 decisions 1 and 12).
- **The Mass Driver and the Lance are open from the start; research unlocks the Missile Rack, the Flak Battery and the Rail Cannon** (ADR-017 decision 4; Game §research.unlocks).
- **What each is for:**
  - the **Mass Driver**: short range, fast fire and cheap; strong against light hulls, and armor blunts it;
  - the **Lance**: long range, a slow and heavy hit that armor barely slows; much of it is wasted on a Small hull;
  - the **Missile Rack**: the longest reach, with a splash; it breaks defended positions and close formations;
  - the **Flak Battery**: a splash at a middle range; it breaks a swarm of Small hulls and still tells against a Medium hull's armor;
  - the **Rail Cannon**: a very heavy, very slow hit that outranges the Lance; it kills heavy hulls, and most of it is wasted on a Small one.
- **A design carries one weapon, a turret that fires in any direction** (ADR-014 decision 9; Game §combat.turrets).
- **How the weapons' ranges and the Defence gun's stand against each other is Game §combat.range.**
- **Research raises a weapon's fire rate, never its hit or its range** (ADR-017 decisions 2 and 4; Game §research.upgrades).

### Modules [§ships.modules]

- **A design has an optional fourth slot, the module, empty by default** (ADR-058 decision 3). A design carries one module at most (ADR-058).
- **The one module is the Sensor Array.** A ship with one sees as far as the module lets it, whatever its weapon, and the module slows it and adds its cost ([Catalogue: modules](Reference/Catalogue.md#modules); ADR-058 decisions 1, 4 and 5; Game §sight.range).
- **No research unlocks a module**: every module is open from the start (ADR-058 decision 2).
- **The same hull, drive and weapon with and without a module are two designs.** A starting design has none (ADR-058 decision 3).

### The Constructor [§ships.constructor]

- **The Constructor is a fixed ship** with no weapon, no module and no design. Its hit points, armor, speed, footprint, turn rate, cost and build time are data, and final ([Catalogue: constructor](Reference/Catalogue.md#constructor); owner, 2026-10-01; ADR-016 decision 2).
- **Each player starts with the starting Constructors** in front of its Command Station ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-016 decision 3).
- **Only the Command Station builds one.** A player that has lost its station keeps the Constructors it has and builds no more (ADR-016 decision 6; ADR-037 decision 2; Game §structures.station).
- **It builds structures, repairs its player's ships and structures, and salvages derelicts**; it attacks nothing (ADR-016 decisions 5 and 9; ADR-074 decision 4; Game §orders.constructors; Game §neutrals.salvage).
- **To an enemy's guns it is a ship like any other** (Game §combat.targeting).
- **Research raises its speed, and its building and repair**, but not its hit points: hull research does not reach it (ADR-017 decision 2; ADR-033 decision 4; Game §research.upgrades).
- **It retreats at a quarter of its hit points**, as a warship does by default (ADR-075 decision 4; Game §orders.retreat).

### Designs [§ships.designs]

#### What a design is [§ships.designs.stats]

- **A design is a hull, a drive, a weapon, and a module or none** (ADR-014 decision 1; ADR-058 decision 3).
- **Its numbers are derived from its components, never written down for it** (ADR-014 decision 1; ADR-017 decision 5; ADR-058 decision 4):
  - its hit points are its hull's, with its player's hull research, times its drive's factor;
  - its armor and its footprint are its hull's;
  - its speed is its hull's, with its player's speed research, times its drive's factor and its module's;
  - its turn rate is its hull's times its drive's factor;
  - its cost is its components' costs added;
  - its build time is its hull's;
  - its hit, fire interval, range and splash are its weapon's, with its player's fire-rate research;
  - its module's sight, where it has one, is how far it sees whatever its weapon (Game §sight.range).
- **The server and the designer derive a design's numbers by one function**, from the components as the player has them, so the designer shows what a ship will fight with (ADR-017 decision 5).
- **Research changes a design for its player alone**, and every warship of it follows (ADR-017 decision 2; Game §research.upgrades).

#### Saving a design [§ships.designs.saving]

- **A ship is built only from a saved design** (ADR-023; ADR-016 decision 6).
- **A new design is refused** when one of its hull, drive and weapon is not yet open to the player, or when the player already has a design of the same components (ADR-017 decisions 4 and 6).
- **A saved design's components never change.** It can be renamed; other components are a new design, so the ships already built of it stay what they are (ADR-017 decision 6).
- **No design is ever deleted**, so a player's list grows with every combination it builds (ADR-023).
- **A design carries a retreat threshold**, which every ship built to it starts with. Saving it under its own name with a new threshold updates it, and the ships built already keep theirs (ADR-075 decision 7; ADR-088 decision 3; Game §orders.retreat.threshold).

#### Names [§ships.designs.names]

- **A design's name is 1 to 32 printable ASCII characters, and not all spaces.** The designer and the server use the same check (ADR-017 decision 6).
- **Unnamed, a design is called by its components** joined by plus signs, "Small+Ion+Mass Driver", with its module's name after a fourth, "Small+Ion+Mass Driver+Sensor Array" (ADR-058 decision 6; ADR-069 decision 3).
- **The designer names a new design** by its components, with the module's initials in place of its name where the whole would be too long. A module on the hull, drive and weapon of a saved design without one that has a short name of its own takes that name with the initials, "Lancer+SA". A name the player types wins (ADR-069 decision 5).
- **A design's code is its components' initials**, "S·I·MD", with its module's after them, "S·I·MD·SA", in the designer's chips and the production window alike (ADR-058 decision 6).
- **A name is only a name**: it changes no component, code or number (ADR-069 decision 4).

#### Starting designs [§ships.designs.starting]

- **Every player starts with a design of every combination of the components no research unlocks, saved**, in hull, drive and weapon order (ADR-014 decision 2).
- **The tuning data gives them short names**, such as Swarm and Lancer, and they are saved under them; a starting design the data does not name keeps its components' name ([Catalogue: startingDesigns](Reference/Catalogue.md#startingDesigns); ADR-069 decisions 1 and 3). A player can rename them (ADR-069 decision 4).
- **A Medium starting design waits for a Shipyard at the level that builds Medium hulls**, so the first warships of a match are Small (ADR-064 decision 9; Game §structures.shipyard).

### The designer [§ships.designer]

The designer is the window in which a player puts designs together, saves them and queues them. Its layout is Game §interface.windows.designer; what it does is here.

#### The window [§ships.designer.window]

- **One designer holds all of a player's designs**, since designs belong to the player and not to a Shipyard (ADR-017 decision 7).
- **It opens from a Shipyard's Ship designer button or by key, and the match keeps running while it is open** (ADR-031 decision 6; ADR-017 decision 7).
- **Its queue goes to a target Shipyard**, one of the player's finished Shipyards, which its arrows step through by number. Opening the designer with a Shipyard selected, or selecting one while it is open, makes that Shipyard the target (ADR-017 decision 7; ADR-068 decision 1).
- **Without a finished Shipyard it still designs and saves**, and Queue is dim (ADR-017 decision 7).

#### Picking the parts [§ships.designer.parts]

- **A design is picked a slot at a time:** a hull, a drive, a weapon, and a module or None, which every design starts with (ADR-017 decision 7; ADR-058 decision 7).
- **Clicking a saved design's chip loads it** (ADR-017 decision 7).
- **A locked component cannot be picked**; its card names the topic that unlocks it, "RESEARCH · LARGE HULL" (ADR-017 decision 7; Game §research.unlocks).
- **When the picks are a saved design, the designer shows that design's name** and its Save reads SAVED; otherwise it names the picks as a new design (ADR-017 decision 7; Game §ships.designs.names).

#### The figures [§ships.designer.figures]

- **Performance bars** show hit points, armor, speed, range, sensors, cost and build time. Each bar is measured against the best that any design reaches, locked components included, so that the scale does not shift as research opens parts. The build time is at the player's Shipyard speed (ADR-017 decision 7; ADR-058 decision 7).
- **A card for each hull gives the design's damage a second after that hull's armor**, per ship and per 100 Ore, since Ore is what a counter is bought with (ADR-017 decision 7).
  - Each card is rated against the best any design does to that hull: good from two thirds of it, fair from a third, and poor below. The production window's cards carry the same rating (ADR-017 decision 7; ADR-068 decision 3).
  - A weapon with a splash is counted for every ship of a formation of that hull that one hit reaches, and the card says how many (ADR-014 decision 12; Game §combat.splash).
- **Hovering a card previews it.** The bars and the damage cards show the design with that component in place, and the change against the design picked; the help line says so (ADR-017 decision 7; ADR-058 decision 7).
- **A locked card previews what its topic buys.** Hovered, it shows the design it would make at the player's research as it stands, and the help line names the topic that unlocks it; it still takes no click, and keeps its hatching and its research line (ADR-017 decision 7).

#### Saving and queuing [§ships.designer.queue]

- **Save saves a new design without queuing it.** Rename renames a saved design whose name was changed, and reads UPDATE when only its retreat changed (ADR-023 decision 5; ADR-017 decision 7; ADR-088 decision 3).
- **The retreat row sets the design's retreat** (ADR-088 decision 3; Game §orders.retreat.threshold).
- **The count of ships runs from one to as many as the target Shipyard's queue has free**, and Queue adds that many separate jobs, its figure their cost together (ADR-023 decision 2; ADR-017 decision 7).
- **Queue on parts that are not yet a saved design saves them first**, and queues the ships once the server has the design, so a new design is one click from its first ship. A save the server refuses queues nothing (ADR-023 decisions 1 to 4).
- **Queue is live** while the target Shipyard has room in its queue and builds the design's hull, and the player's Ore covers one ship (ADR-023 decision 1; ADR-064 decision 9). For a hull above the Shipyard's level it is dim and reads "Needs Shipyard L2" in place of the build time, and the design can still be saved (ADR-064 decision 9).
- **A queued job still waits at the front for Ore and for the fleet cap** (Game §structures.production).

#### Typing a name [§ships.designer.name]

- **A click on the name field gives it the keyboard**, and no order, control group or camera key reads it meanwhile (ADR-017 decision 8; Game §interface.windows.designer).
- **Enter saves, and Esc drops what was typed.** A click anywhere but the field ends typing and keeps the text, and so does closing the window (ADR-017 decision 8).
- **The field takes only the characters a name may have**, up to its limit (ADR-017 decisions 6 and 8).

---

## Combat [§combat]

Ships and armed structures fight by a few fixed rules that the server plays every tick. Weapons are turrets that fire on the move, a ship fires at the nearest enemy in its range, every hit lands, and armor takes a share off each one. There is no accuracy, tracking or damage type: a fight is decided by damage after armor, shots to kill, range and how much is in the field, the figures the designer shows (Game §ships.designer.figures). What a fight looks like is Game §look.shots and Game §look.explosions, and what a player sees of the enemy is Game §sight.

### Who fights [§combat.sides]

- **Everything with hit points is in combat**: every ship and every structure, a site under construction included. Asteroids, asteroid fields and derelicts have none, and nothing fires at them (ADR-014 decision 10; ADR-016 decision 4; ADR-074 decision 1).
- **An enemy is anything of another owner.** The pirates are every player's enemy, and never fight each other. Nothing fires at its own side (ADR-073 decision 1; ADR-014 decision 12).
- **What fires:** every warship, with its design's weapon, and the finished Command Station and Defence Platform, with the Defence gun. A Constructor, a Shipyard, a Research Lab, a Mining Rig, a Relay and a Repair Bay are unarmed, and fight only by being shot at (ADR-016 decisions 7 and 9; Game §structures.armed).

### Weapons are turrets [§combat.turrets]

- **A weapon fires in any direction, and keeps firing while its ship moves**, so a turn rate matters to movement only (ADR-014 decision 9; ADR-010 decision 8).
- **Combat comes before movement in a tick:** ships fire from where they stand, then move (ADR-014 decision 9).
- **Kiting is allowed.** Firing on the move lets a faster ship with a longer gun hold a slower one at bay (ADR-014 decision 9).

### Targeting [§combat.targeting]

- **A ship fires at the nearest enemy ship in its range, and at the nearest enemy structure only when no enemy ship is in range** (ADR-014 decision 8).
- **It keeps its target until the target is destroyed or leaves its range**, even when a nearer enemy comes into range, or an enemy ship while it fires at a structure (ADR-014 decision 8).
- **A ship on an attack order fires at its ordered target while that is in range**, and otherwise at what the rule above picks, so it fires on its way in (ADR-014 decisions 8 and 9; Game §orders.direct).
- **A Constructor is a ship to the guns**, and is picked before any structure (ADR-014 decision 8).
- **Fog of war never changes what anything fires at** (Game §sight.range).
- **The armed structures pick their targets by the same rule** (ADR-016 decision 7).

### Range [§combat.range]

- **Range is measured between centers**, so a big target is no easier to reach (ADR-014 decision 7).
- **Every range is data, and no research changes one** ([Catalogue: weapons](Reference/Catalogue.md#weapons), [Catalogue: structureWeapons](Reference/Catalogue.md#structureWeapons); ADR-017 decision 4; ADR-033).
- **The Defence gun outranges the Lance and falls short of the Missile Rack,** which the tests hold of the data (ADR-016 decision 7), so Missile Racks break a Defence Platform from beyond its reach. Where every other weapon stands is the data's ([Catalogue: weapons](Reference/Catalogue.md#weapons)).

### Firing [§combat.firing]

- **A weapon fires once each fire interval** ([Catalogue: weapons](Reference/Catalogue.md#weapons)). Its reload counts in thousandths of a tick, so an interval that is not a whole number of ticks still fires at its average rate (ADR-014 decision 4).
- **A cold weapon's first shot comes at a random moment within its interval**, so a group's volleys do not land together. A weapon is cold when its ship is new, or has had no target for a whole interval past its reload (ADR-014 decision 5).
- **A ship that loses its target only for a moment keeps its rhythm.** A weapon that was ready and waiting fires as soon as it has a target, and its next shot comes a whole interval later; finding a target never makes a ship fire sooner than its last shot allows (ADR-014 decision 5).
- **Fire-rate research shortens the interval** (Game §research.upgrades).

### Hits and damage [§combat.hits]

- **Every hit is instant and lands.** The projectile or beam on the screen is presentation, and nothing dodges it. There is no flight time, accuracy, tracking or damage type (ADR-014; Game §look.shots).
- **Shots are chosen from where everything stands at the start of the tick, and land together at its end.** No shot depends on which ship was handled first, and a target can be hit more than it needs, so focus fire overkills (ADR-014 decision 6).
- **A hit does the larger of a quarter of its damage and its damage less the target's armor** (ADR-014 decision 3). Armor blunts small hits most and heavy hits least, and never takes more than three quarters of one.
- **Hit points, armor and damage count in hundredths**, so the quarter of a hit is never lost; the HUD shows whole hit points, rounded up, so that a ship with a sliver left does not read as dead (ADR-014 decision 3; Game §interface.hud.selection).
- **Armor never changes in a match:** no research and no level raises it (ADR-033; ADR-064 decision 4).
- **There is no friendly fire** (ADR-014).

### Splash [§combat.splash]

- **The Missile Rack and the Flak Battery have a splash** ([Catalogue: weapons](Reference/Catalogue.md#weapons); ADR-014 decision 12).
- **A splash hits every other enemy ship and structure whose center is within its radius of the target's center**, measured from where everything stood at the start of the tick. Each takes the full hit after its own armor, and the hits land with the others at the end of the tick (ADR-014 decision 12).
- **A splash has no falloff**, never hits the shooter's own side, and never hits what has no hit points (ADR-014 decision 12).
- **What a splash reaches depends on how close the enemy stands.** A formation stands its ships three footprint radii apart, so a splash reaches into a formation of Small hulls, and into Medium or Large ones only where a fight presses them closer (ADR-014 decision 12; ADR-010 decision 4; Game §orders.movement).
- **A splash hit reveals its shooter as any hit does** (Game §sight.reveal).

### Armed structures [§combat.structures]

- **A finished Command Station or Defence Platform fires its Defence gun by the ship rules:** the same targeting, a cold gun's first shot at a random moment, and hits that land at the end of the tick ([Catalogue: structureWeapons](Reference/Catalogue.md#structureWeapons); ADR-016 decision 7).
- **A structure under construction does not fire** (ADR-016 decision 5).
- **A Command Station's further guns each keep a rhythm of their own**, firing at the target the station holds, so they do not fire together (ADR-064 decision 11; Game §structures.station).
- **The pirates' Defence Platforms fire as any armed structure does**, at the base numbers (ADR-073 decisions 1 and 5; Game §neutrals.outposts).

### Standing and closing [§combat.closing]

- **Two orders stop a ship to fire.** An attack-moving ship stands while an enemy is in its range, and a ship on an attack order closes on its target and stands while the target is in range; how each closes is Game §orders.direct (ADR-014 decision 9).
- **Every other ship fires on the move**, a retreating one included (ADR-014 decision 9; Game §orders.retreat.going).
- **A ship standing to fire keeps its range when its own side pushes it** (Game §orders.movement).

### Destruction [§combat.destruction]

- **A ship or structure whose hit points reach nothing is destroyed at the end of the tick and leaves the simulation.** Every order on it ends, and so does every target (ADR-014 decision 6).
- **A destroyed structure stops blocking**, and the paths round it are brought up to date (Game §structures.blocking).
- **What is lost with it:** a structure's levels and an upgrade under way (Game §structures.levels.loss), a producer's queue (Game §structures.production), and a Research Lab's queue and topic under way (Game §research.lab). Nothing is refunded (Game §economy.costs).
- **A ship that survives a hit below its retreat threshold turns for home**, checked once the tick's hits have landed (Game §orders.retreat.going).
- **Ships and structures leave no wreck**, except the last structure of a pirate outpost, which leaves a derelict to salvage (ADR-074 decision 6; Game §neutrals.clearing).
- **A ship does not improve by fighting:** there is no veterancy (Game §limits).

### What a player sees of a fight [§combat.seen]

- **Each snapshot says what happened in its tick:** each shot, from where to where, with its splash and the gun it left from, and each entity destroyed, with where it was; and every entity's hit points. The client draws the effects from them (ADR-014 decision 11; Game §look.shots; Game §look.explosions).
- **A ship that hits a player is seen by that player for a while after each hit** (Game §sight.reveal). A shot is seen when its shooter or its target is, and a destruction when it is in sight (Game §sight.seen).
- **Health bars, and the alerts for what is lost, are the HUD's** (Game §interface.marks; Game §interface.alerts).

---

## Research [§research]

A player's Research Lab works through a queue of topics, each paid for in Ore when it starts and taking its own time. A topic either raises a rate or unlocks a component, and what a player researches decides which designs it can build. The topics come in three tiers, and the Lab's levels open the second and the third. What research does is its player's alone.

### The Research Lab [§research.lab]

- **A player researches in its Research Lab** (ADR-017 decision 1; Game §structures.lab).
- **The Lab researches only once it is finished**, and keeps researching while its next level is built (ADR-017 decision 1; ADR-064 decision 3).
- **The queue belongs to the Lab.** A Lab destroyed takes its queue with it, and the topic under way is lost with its Ore. Research already finished stays finished (ADR-017 decision 1).
- **A Lab destroyed also takes its levels**, and the tiers they opened close with them (Game §structures.lab; Game §research.tiers).

### Topics [§research.topics]

- **Each topic has a name, a tier, an Ore cost, a research time, the topics it requires, and one effect** ([Catalogue: research](Reference/Catalogue.md#research); ADR-033 decision 1; ADR-008 decision 4).
- **An effect raises one rate, or unlocks one component** (ADR-017 decisions 2 and 4; Game §research.upgrades; Game §research.unlocks).
- **No topic requires one of a later tier**, and none requires itself (ADR-033 decision 2; ADR-008 decision 5).
- **Both players research the same topics** with the same numbers (ADR-008 decision 3). The pirates research nothing (ADR-073 decision 1).

### The queue [§research.queue]

- **A Lab queues up to five topics** (ADR-017 decision 1).
- **A topic is refused, at no cost** (ADR-017 decision 1; ADR-033 decision 2):
  - by a Lab that is not the player's or not yet finished;
  - when it is researched already or in the queue already;
  - when one of the topics it requires is neither researched nor ahead of it in the queue;
  - when it is of a tier the Lab's level has not opened;
  - when the queue is full.
- **A topic may follow what it requires in the queue**, since one Lab researches them in order (ADR-017 decision 1).
- **The front topic starts once what it requires is researched and the player can pay.** It is paid for then, and until then it waits at the front, and the topics behind it wait too (ADR-017 decision 1; Game §economy.costs).
- **A topic takes its research time**, and the next starts on the following tick (ADR-017 decision 1).
- **Nothing cancels a topic, and nothing refunds one** (ADR-017).
- **A salvaged derelict can recover part of a topic's time** (ADR-074 decision 5; Game §neutrals.salvage).

### Tiers [§research.tiers]

- **The topics are in three tiers** (ADR-033 decision 1). The first is open from the start (ADR-033 decision 5).
- **A level of the Research Lab opens each further tier.** The tuning data names the level that opens each tier and the topics that level requires before it can be built ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-033 decision 2; ADR-064 decision 10; Game §structures.lab).
- **No topic opens a tier** (ADR-033 decision 2).
- **A topic of a tier not open is refused**, and one queued at the right level stays queued (ADR-033 decision 2).
- **A player's open tier is the highest that its finished Lab's level opens**, and the first without a Lab (ADR-033 decision 5).

### Two topics at once [§research.slots]

- **A Lab at the level that gives a second research slot researches the front two topics of its queue at once** ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-017 decision 1; ADR-064 decision 10).
- **The second starts once the first has started and once what it requires is researched**, not only queued ahead of it, so it never runs a topic before what that topic needs. It is paid for when it starts, and takes its own time (ADR-017 decision 1).
- **When the first finishes, the second's progress moves to the front** (ADR-017 decision 1).
- **The queue stays five topics long** (ADR-017 decision 1).

### Upgrades [§research.upgrades]

- **An upgrade applies on the tick its topic finishes**, to every design, ship and structure of its player, those already standing included (ADR-017 decision 2).
- **An upgrade is a factor on the tuning data's number, and topics on one rate add their percentages** rather than multiplying ([Catalogue: research](Reference/Catalogue.md#research); ADR-017 decision 2; ADR-033 decision 3).
- **What can be upgraded** (ADR-017 decision 2; ADR-033 decision 4):
  - a Mining Rig's income (Game §economy.income);
  - every hull's hit points;
  - one weapon's fire rate;
  - the Shipyards' build speed;
  - every structure's hit points;
  - the Defence gun's fire rate;
  - every ship's speed;
  - the Constructors' rate of building and repair;
  - how long the ore asteroids last under its player's rigs (Game §economy.reserves).
- **How each lands:**
  - **Hit points.** A ship or structure keeps the share of its hit points it had, so an undamaged one gains the whole upgrade. Hull research reaches warships only, not Constructors or structures; structure research reaches every structure, and adds to a level's share (ADR-017 decision 2; ADR-033 decision 4; ADR-064 decision 4).
  - **Fire rate.** It shortens the interval from the next shot; a reload under way is not cut short (ADR-017 decision 2).
  - **Speed.** Every design and warship of the player follows it, and so do its Constructors. Turn rates do not change, and a move under way keeps its pace until the next order (ADR-033 decision 4).
  - **Build speed.** It speeds a Shipyard's job under way too, and not the Command Station's Constructors (ADR-017 decision 2).
  - **The Constructors' rate.** It speeds their building, their repair and their salvage alike, and not a structure's level, which builds itself (ADR-033 decision 4; ADR-074 decision 4; ADR-064 decision 3).
- **An upgrade changes a rate, never the size of a hit, armor or a range.** The tuning loader refuses one that would (ADR-017 decision 4; ADR-033).

### Unlocks [§research.unlocks]

- **A component is open to a player when no topic unlocks it, or when that player has researched a topic that does.** An unlock is its researcher's alone (ADR-017 decision 4).
- **Research unlocks the Large hull, the Fusion and Pulse Drives, and the Missile Rack, Flak Battery and Rail Cannon** ([Catalogue: research](Reference/Catalogue.md#research); ADR-017 decision 4). No module needs research (ADR-058 decision 2).
- **A new design must use components open to its player** (ADR-017 decision 4; Game §ships.designs.saving).
- **The Large hull needs a Shipyard's level as well:** the topic lets a player save a Large design, and a Shipyard at the level that builds Large hulls builds it (ADR-064 decision 9; Game §structures.shipyard).
- **In the designer, a locked component's card names the topic that unlocks it** (ADR-017 decision 7; Game §ships.designer.parts).

### Where research is shown [§research.shown]

- **The player queues research in the research window.** It shows the Lab's queue, and lists every topic not yet researched or queued, tier by tier, each with what it does, its cost, its tier and its time (ADR-017 decision 9; ADR-033 decision 5; Game §interface.windows.research).
- **A topic that cannot be queued yet says what it needs:** the topics it requires that are neither researched nor queued, and for a tier not open the Lab's level that opens it, "RESEARCH LAB L2" (ADR-033 decision 5).
- **A topic is offered while the Lab is finished with room in its queue and the player's Ore covers it**; one the player only cannot afford is dim with its cost (ADR-043 decision 4).
- **The status panel under the Ore names the topic under way and its progress**, says when it waits for Ore, or that the Lab is idle while a topic is open to it (ADR-066 decisions 1 and 2; Game §interface.hud.status).
- **The AI and a deputy research by the same rules** (Game §ai.base; Game §ai.deputy).

---

## Pirates, salvage and repair [§neutrals]

Beside the two players, the map holds two kinds of neutral content. Pirates guard outposts on some nodes, keep their sectors from being claimed, and leave a wreck when they are cleared. Derelicts, wrecks with no owner, lie in the sectors between the homes and the middle, and a player's Constructors salvage them for Ore and for research. Where both are placed is Game §map. A damaged ship is repaired by Constructors or at a base, for nothing.

### Pirates [§neutrals.pirates]

- **The pirates are a neutral owner, not a player.** They have no Ore, no production, no research and no vision of their own, and they send no commands: a rule of the simulation moves them (ADR-073 decision 1).
- **They are every player's enemy.** Every rule that treats another player's ships and structures as the enemy's treats theirs so too: targeting, splash, attack orders, a hold's answer, suppression, and fog of war, under which they are seen and remembered as an enemy is. All outposts share the one owner, so pirates never fight each other (ADR-073 decision 1).
- **They fight at the base level**, with none of any research's upgrades (ADR-073 decision 1).
- **Their ships never retreat**, and nothing repairs them (ADR-075 decisions 3 and 4).
- They are drawn with the Tarkan set's meshes in violet, and are violet on the minimap (ADR-073 decision 10; Game §look.models.sets).

### Outposts [§neutrals.outposts]

- **An outpost is a camp or a stronghold**, each a number of Defence Platforms and of ships of set designs. A stronghold is the larger, and guards a rich corner or the center ([Catalogue: pirates](Reference/Catalogue.md#pirates); ADR-073 decision 3; Game §map.outposts).
- **An outpost stands round its node, facing the map's center.** Its Defence Platforms stand in a row through the node, across the line to the center, 140 m apart, and a camp's one platform stands on the node. Its ships stand in a ring 120 m round the node, the first on the side facing the center. So an outpost and its mirror stand alike, turned half a turn (ADR-073 decision 4).
- **The center's stronghold is its own mirror**, and its Lance ship stands nearer player 1's start than player 2's: the one asymmetry of the map (ADR-073 decision 4).
- **Its Defence Platforms fire** as any armed structure does (ADR-073 decision 5; Game §combat.structures).

### Guarding [§neutrals.guarding]

- **An outpost guards its node.** Once a second its ships act as one: they take as their quarry the player's ship or structure nearest the node within the guard distance, attack-move on it, drop it once it is gone or beyond the chase distance from the node, and then go back to the node ([Catalogue: pirates](Reference/Catalogue.md#pirates); ADR-073 decision 5).
- **Pirates never leave their sector and never raid.** The chase stays inside the sector, and the server refuses to place an outpost whose chase would leave it (ADR-073 decisions 4 and 5).
- **They see through fog of war**, as a Defence gun does (ADR-073 decision 5).
- **Whatever comes within the guard is a quarry**, a passing fleet or a lone Constructor alike, whatever it came for (ADR-073 decision 5).

### Guarded sectors [§neutrals.guarded]

- **A sector is guarded while a pirate structure stands in it** (ADR-073 decision 6).
- **A guarded sector cannot be claimed.** A Relay ordered there is refused, so its asteroids earn nothing for anyone and no Shipyard can stand in it (ADR-073 decision 6; Game §territory.claim).
- **Its node counts for no one** in domination (ADR-073 decision 6; Game §territory.domination).
- **Ships do not guard a sector.** Once an outpost's structures have fallen, the ships it has left do not stop a claim, but they suppress a Relay there as any enemy warship does (ADR-073 decision 6; Game §territory.suppression).
- **Every player knows from the start which sectors are guarded**, as it knows each sector's holder, and the Relay's ghost is red in one. The pirates themselves stay under fog until they are seen (ADR-073 decision 7).

### Clearing an outpost [§neutrals.clearing]

- **An outpost is cleared when its last structure falls**, and it never comes back (ADR-073 decision 8).
- **It leaves a wreck** where its last Defence Platform fell: a derelict that pays the outpost's wreck Ore and names no topic ([Catalogue: pirates](Reference/Catalogue.md#pirates); ADR-074 decision 6).
- **A camp's wreck covers its node**, so a Relay waits until the wreck is salvaged. A camp is taken in three steps: fight it, salvage its wreck, then claim the node (ADR-074 decision 6).
- **Every player is told** when a guarded sector's last pirate structure falls (ADR-075 decision 10; ADR-059 decision 1; Game §interface.alerts).

### Derelicts [§neutrals.derelicts]

- **A derelict is a wreck.** It has no owner and no hit points, and nothing can fire at it (ADR-074 decision 1).
- **It holds Ore, and may name a research topic** (ADR-074 decision 1; Game §map.derelicts).
- **It blocks no path, but no structure may stand on one**, and the ghost of a structure placed over one is red (ADR-074 decision 1).
- **Under fog it is treated as an enemy's structure:** it is seen within sight, remembered out of sight with what it holds, and forgotten once its place is in sight without it (ADR-074 decision 7; Game §sight.memory).
- **The pointer over one says what it holds:** its Ore, its topic, how far its salvage has come, and how long ago the player last saw it when it is only remembered (ADR-074 decision 10; ADR-081 decision 4; Game §interface.hud.hint).
- It is drawn with its hull's model in gray, and light gray on the minimap (ADR-074 decision 10; Game §look.models.sets).

### Salvage [§neutrals.salvage]

- **Only Constructors salvage**, on an order to salvage a derelict their player sees or remembers; a warship cannot. How the order is given is Game §orders.constructors (ADR-074 decisions 4 and 10).
- **Salvage is work, as building a site is.** One Constructor takes the salvage's work time; each further one adds the share of one more that building does; and each crew works at its own player's Rapid Construction rate ([Catalogue: salvage](Reference/Catalogue.md#salvage), [Catalogue: constructor](Reference/Catalogue.md#constructor); ADR-074 decision 4; ADR-016 decision 5; ADR-033 decision 4).
- **Two players' crews on one derelict work on the same salvage**, and the crew that finishes it is paid (ADR-074 decision 4).
- **When the work is done, the derelict is gone** and the player is paid its Ore at once (ADR-074 decision 4; Game §economy.bank).
- **A named topic recovers part of its research time** ([Catalogue: salvage](Reference/Catalogue.md#salvage); ADR-074 decision 5; Game §research.queue):
  - if a Lab of the player's is researching it, that much comes off at once;
  - otherwise the player keeps it, whatever the topic's tier, and the topic starts that far along when it starts;
  - the topic still costs its full Ore;
  - a topic already researched gains nothing, and a second recovery of one still waiting adds nothing;
  - the research window marks a recovered topic SALVAGED.

### Repair [§neutrals.repair]

A damaged ship or structure is never repaired by itself. Constructors repair on an order, and a base repairs the ships near it with no order. Repair costs nothing (ADR-016 decision 5; ADR-075 decision 3).

#### By Constructors [§neutrals.repair.constructors]

- **Constructors repair their player's own ships and structures**, on a repair order; how the order is given is Game §orders.constructors (ADR-016 decision 5).
- **Each Constructor restores a share of its target's full hit points every second** ([Catalogue: constructor](Reference/Catalogue.md#constructor)), so two repair twice as fast as one. Its player's research raises the rate, as it raises building (ADR-016 decision 5; ADR-033 decision 4).
- **A structure whose next level is being built is repaired as any other** (ADR-064 decision 4).
- The order ends when the target is whole, or destroyed (ADR-016 decision 5).

#### At a base [§neutrals.repair.base]

- **A player's built Repair Bays, its Command Station and its Shipyards repair the ships near them**, with no order ([Catalogue: shipRepair](Reference/Catalogue.md#shipRepair); ADR-075 decisions 2 and 3).
- **Each repairer takes the most damaged of its player's ships in reach**, up to a number of ships, reach measured from its footprint to theirs. A ship one repairer takes is not taken by another in the same tick, so two repairers side by side repair twice as many (ADR-075 decision 3).
- **It repairs any damaged ship of its player's**, warship or Constructor, retreating or not, at a share of the ship's full hit points a second. Research does not raise it (ADR-075 decision 3).
- **A Relay repairs nothing**, and nor does any other structure (ADR-075 decision 3).
- A ship below its retreat threshold goes back to a repairer by itself (Game §orders.retreat).

#### The Repair Bay [§neutrals.repair.bay]

- **The Repair Bay is a structure for repairing ships at the front.** A Constructor builds it, only in a sector its player holds, and one that stands in a sector its player loses keeps repairing ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-075 decision 1; Game §territory.gives).
- **It has no gun and no levels** (ADR-075 decision 1; Game §structures.levels).
- **A retreating ship goes to a Repair Bay first**, before the Command Station or a Shipyard (ADR-075 decision 5; Game §orders.retreat.going).
- It is drawn with the Shipyard's model, darker (ADR-075 decision 10; Game §look.models.sets).

---

## Orders [§orders]

A player commands its ships by orders, and only by orders. An order is given to a selection now (direct orders), kept by the server for a group until it is given another (standing orders), or kept until something happens (scheduled orders). Beside them, every ship carries a retreat that the server runs on its own: below its threshold, a ship goes back to be repaired. What a ship fires at is Game §combat; what a structure, a queue or a Research Lab is ordered to do is Game §structures, Game §ships and Game §research.

### How an order is given [§orders.giving]

- **An order is a command to the server, never a change.** The server checks it against ownership, cost and legality, applies it at the start of its next tick, or refuses it (ADR-002 decision 2). The player's own game, the AI, a deputy and a scheduled order all give orders the same way (ADR-002 decision 7; ADR-079 decision 2; ADR-080 decision 5).
- **An order leaves as soon as it is given,** and the screen shows the simulation one tick behind the newest snapshot, so a ship visibly answers about a tick after the server applied the order (ADR-013 decision 3; ADR-002 decision 5).
- **A refused order is not obeyed, and the player who gave it is told why,** in words, as an alert; a scheduled order refused as it fires says so as it fires (ADR-094; ADR-080 decision 5; Game §interface.alerts).
- **Orders go to the selected ships.** A selected structure takes no ship order (ADR-013 decision 5).

### Selecting [§orders.selection]

- **A click selects** the nearest of the player's ships within 12 pixels of the pointer, or within its footprint on the screen where that is larger. A click on none of its ships but on one of its structures selects that structure alone, for its panel and its windows. A click on none of its own but on another player's structure or the pirates', seen or remembered, selects that alone, for its panel, which offers no button and takes no order (ADR-013 decision 5; ADR-081 decision 4). A click on nothing clears the selection (ADR-013 decision 5).
- **A drag of more than 6 pixels is a box,** which selects the player's ships inside it, and only ships (ADR-013 decision 5). A press on a panel or a window of the HUD is the HUD's and selects nothing, but a drag begun in the world ends wherever it is let go (ADR-015 decision 5).
- **Shift adds** a ship or a box to the selection, and Shift-clicking a selected ship takes it out. A ship added with Shift joins ships, never a structure (ADR-013 decision 5).
- **A second click on the same ship within 400 ms** selects every ship of the player's on the screen of that ship's design and hull (ADR-013 decision 5).
- **Ctrl and a digit store the selection as a control group; the digit recalls it,** and a second tap within 400 ms centers the camera on it (ADR-013 decision 5).
- **A click on a ship's bar in the selection panel** selects that ship alone and centers the camera on it (ADR-088 decision 4; Game §interface.hud.selection).
- **Ships that are gone, or are not the player's, drop out of the selection and the control groups;** another player's or the pirates' structure selected alone stays while it is seen or remembered, and joins no control group (ADR-013 decision 5).
- **The selection is drawn as a ring round each selected ship,** green, or amber while attack-move or a standing order waits for its click (ADR-067 decision 1; Game §interface.marks).

### Direct orders [§orders.direct]

- **Right-click orders on the button's press,** with the selection's ships (ADR-013 decision 5):
  - with Constructors selected, on one of the player's own ships or structures that is unfinished or damaged, the Constructors build or repair it, and the rest of the selection moves there (Game §orders.constructors);
  - with Constructors selected, on a derelict, the Constructors salvage it, and the rest of the selection moves there (Game §orders.constructors);
  - otherwise, on an enemy ship, the selection attacks it; on no ship but an enemy structure, it attacks that. The pirates are every player's enemy (ADR-013 decision 5; ADR-073);
  - otherwise the selection moves to the point on the ground.
- **Right-click on the minimap** moves the selection to the point clicked (ADR-013 decision 5).
- **Move:** the ships go to the point, in formation (Game §orders.movement), and fire at what comes in range on the way, since weapons are turrets (ADR-014 decision 9; Game §combat.turrets).
- **Attack:** the selection's warships close on one enemy ship or structure and fire at it until it is destroyed; Constructors in the selection take no part (ADR-016 decision 9).
  - The target must be one the player sees, or an enemy structure it remembers; any other attack is refused (ADR-024 decision 7).
  - An attack on a ship ends once no entity of the player's sees it. An attack on a remembered structure goes where the player saw it (ADR-024 decision 7).
  - The ships close along a route the group searched once, each at its own top speed, stand while the target is in range, and path again together when it has moved 40 m from where they last pathed to, at most once a second (ADR-014 decision 9; ADR-047 decision 3).
- **Attack-move:** A, then a left-click on the ground, sends the ships to the point; a ship that meets an enemy stops at its own weapon's range and fires, and goes on once none is in range. Constructors in the selection go along (ADR-013 decision 5; ADR-014 decision 9).
- **Stop:** S drops the ships' orders, and they hold where they are (ADR-013 decision 5; ADR-002 decision 2).
- **An order waiting for its click is canceled** by Esc, once no window is open for Esc to close, or by a right-click, which gives its own order; A, H or T arms its own order in place of the one waiting (ADR-059 decision 7; ADR-013 decision 5).
- **A selection with a warship offers its orders as buttons** beside the selection panel, Attack-move, Hold sector, Patrol and Stop, and a button does what its key does (ADR-088 decision 2; Game §interface.hud.buttons). The keys are listed in the Controls window ([Controls](Reference/Controls.md); Game §interface.windows.controls).

### Constructors' orders [§orders.constructors]

- **A Constructor attacks nothing.** It takes part in moves and attack-moves, and in no attack, standing order or scheduled order other than a rig's (ADR-016 decision 9; ADR-059 decision 3; ADR-080 decision 4).
- **Build:** a build button arms a placement while the selection holds a Constructor. The next left-click on the ground orders the selected Constructors to build there, and Shift keeps the placement armed for another. Right-click or Esc cancels it. The ghost that follows the pointer is green where the structure may stand and red where not, and a Mining Rig snaps to its asteroid, which must be one the player has seen (ADR-013 decision 5; ADR-046 decision 8). Once the selection has been given an order, its build buttons are folded into one Build button, which opens them again (ADR-088 decision 2; Game §interface.hud.buttons). What may stand where, and what it costs, are Game §structures.
- **Build and repair:** a right-click on one of the player's own ships or structures that is unfinished or damaged sends the selected Constructors to work on it until it is built and whole, or destroyed. Further Constructors join a site the same way (ADR-016 decision 5; Game §structures.sites).
- **Salvage:** a right-click on a derelict the player sees or remembers sends the selected Constructors to salvage it (ADR-074 decisions 4 and 10; Game §neutrals.salvage).

### How ships carry out a move [§orders.movement]

- **Ships path round obstacles,** the asteroids, the fields and every structure but a Mining Rig. A ship whose way a new structure blocks searches again at once (ADR-010 decisions 1 and 2; ADR-016 decision 8).
- **A group ordered as one moves as a formation:** a grid of slots round the destination, facing the way the group travels, three footprint radii of its widest ship apart, the ships ahead taking the front slots. The group searches one route, and its ships keep lanes beside it round each corner (ADR-010 decision 4; ADR-047 decisions 1 and 2).
- **The group arrives together.** Every ship goes at its path's length over the time its slowest ship needs, so none goes faster than it can and none arrives early (ADR-010 decision 4).
- **An order for more than 32 ships sets off in the tick after it is given,** its paths planned over the two ticks, its ships holding until then and setting off together (ADR-032 decision 3).
- **A destination inside an obstacle moves to its edge,** and one beyond the map's edge is pulled inside it (ADR-010 decision 3).
- **Ships fly the way they face and turn as they go.** A ship never turns on the spot; it slows into a turn, down to half its speed with its waypoint abeam or behind, and never flies too fast to reach its waypoint on an arc (ADR-039 decisions 1 to 3).
- **Ships stop dead at their destination, and stand still to fire on an attack-move,** with no momentum (ADR-010 decision 7; ADR-014 decision 9).
- **Ships part rather than collide.** A ship standing to fire keeps its range when its own side pushes it, giving way only sideways round its target, so a group closing on an enemy spreads into an arc at its range (ADR-010 decision 7).

### Standing orders [§orders.standing]

- **A standing order is kept by the server for a group of warships until it is ended,** so it holds while the player looks elsewhere or is away (ADR-059 decision 3).
- **Hold a sector:** H, then a left-click, has the selection's warships hold the sector clicked in. It is refused on a map without sectors, or for a point in none (ADR-059 decisions 3 and 7).
  - The group goes to the sector's node. Once a second it attack-moves on the nearest enemy ship its player sees in the sector, unless it is already headed within 150 m of it or a ship of it is firing. Once no enemy is left and it stands idle more than 200 m from the node, it goes back to the node (ADR-059 decision 4).
  - A held sector is seen whole, so a group holding its own sector sees all of it (ADR-059 decision 4; Game §sight.range).
- **Patrol:** T, then a left-click, has the selection's warships patrol between the point clicked and the middle of where they were ordered from. Each time the whole group stands idle at one end, it attack-moves to the other (ADR-059 decisions 3, 4 and 7).
- **The group acts as one,** once a second, by the group orders the game has: its attack-moves are laid as a formation (Game §orders.movement). A group still planning a large order's paths waits for them (ADR-059 decision 4).
- **Constructors take no part,** and a selection of Constructors alone is refused (ADR-059 decision 3).
- **A move, an attack-move, an attack or a stop ends a ship's standing order,** and a new hold or patrol replaces it. The orders the standing order itself gives do not end it (ADR-059 decision 5).
- **A ship that retreats keeps its standing order.** Its group goes on without it, and once whole it rejoins the group when the group next moves (ADR-059 decision 5; ADR-075 decision 6).
- **Only the owner sees a ship's standing order;** the selection panel says "Holding a sector" or "On patrol" (ADR-059 decision 6).

### Scheduled orders [§orders.scheduled]

A scheduled order is an order given in advance that the server carries out later, when its trigger fires, whoever is playing the seat then. It is how a player who will be away plans: "attack-move to North at 02:00, unless they have reinforced".

#### What one holds [§orders.scheduled.parts]

- **One trigger, one action and at most one condition, given to a selection** (ADR-080 decision 4). Nothing nests, and no order names more than one of each (ADR-080).
- **Triggers:** a time of day on the player's own clock (Game §orders.scheduled.time); or, in one sector the player holds and picks, enemy ships entering it, a Relay of the player's suppressed or under attack there, or a Mining Rig of the player's lost there (ADR-080 decision 4). An event trigger fires only on an event raised after the order was given: enemy ships already in the sector fire it only when they come again (ADR-080 decision 4).
- **Actions:** move, attack-move, hold the sector and patrol, which go to a sector's node; attack, whose target is an enemy or pirate structure or ship the player knows of in that sector; and build a Mining Rig on an asteroid with Ore left, as far as the player knows (ADR-080 decisions 4 and 9). Warships take the first five, and Constructors the rig. The target is fixed when the order is given (ADR-080 decision 4).
- **The condition** is none, or a number of command points from 0 to 60 in steps of two: the most that the enemy and pirate warships the player sees in the action's sector may take. More than that when the order fires, and its ships hold the sector they are in, the middle of them, instead (ADR-080 decisions 4 and 9). Command points are Game §economy.fleetcap.
- **A scheduled order is refused** without a warship, or without a Constructor for a rig; for a trigger's sector that is not the map's or a hold outside every sector; for a condition below zero; for another player's ship; and as the orders of its actions are (ADR-080 decision 4).

#### How it fires [§orders.scheduled.firing]

- **The server keeps it, as it keeps a standing order, with the ships that wait on it** (ADR-080 decision 5).
- **At the end of the tick its trigger fires, it gives its ships its action** through the path a player's order takes, and is gone: a scheduled order fires once (ADR-080 decision 5).
- **Its event says what it did:** as given, held back by its condition, or refused, as when an attack's target is gone or a rig's sector is not held. The alert tells it every time (ADR-059 decision 1; ADR-080 decision 5; Game §interface.alerts).
- **A ship waits on one scheduled order at a time.** Any order the player gives it takes it out, and so does a new scheduled order; setting its retreat does not. An order none waits on any more is dropped, and so is one whose ships are all gone, so the orders are bounded by the ships (ADR-080 decision 5).
- **The player can cancel a scheduled order** from the orders window: it is gone at once and never fires, and its ships keep what they were doing (ADR-080 decision 5).
- **Giving a scheduled order gives its ships nothing to do until it fires:** they keep their orders, their standing order and their retreat until then (ADR-080 decision 5; ADR-075 decision 6).
- **A deputy leaves alone a ship that waits on one,** and the server fires it whoever plays the seat (ADR-079 decision 6; ADR-080 decision 5; Game §universe.handover).
- **Only its owner sees it** (ADR-080 decision 5).

#### A time of day [§orders.scheduled.time]

- **A time of day is the player's clock:** the zone the player's system is set to, with its summer time; where the system cannot say the zone, UTC (ADR-080 decisions 6 and 9).
- **The game turns the hour and minute picked into the next such moment on that clock.** A moment a clock skips as summer time starts is the moment it skips at, and one it reads twice as summer time ends is the first (ADR-080 decisions 6 and 9).
- **The server turns the moment into a tick as the order arrives,** the first tick at or after it, at most a week ahead. The tick a game writes is never trusted, so the universe's log replays (ADR-080 decision 6; ADR-009 decision 3).
- **A time of day is a moment, not a recurring hour.** An order for 02:00 fires once, at the next 02:00; another night needs another order. A pause while the server is down pushes it late: it fires at its tick (ADR-080 decision 6; Game §universe.persistence).

#### The orders window [§orders.scheduled.window]

- **O opens and closes the orders window** (ADR-080 decision 9; ADR-031; [Controls](Reference/Controls.md)).
- **It lists the seat's scheduled orders,** the first six, each with how many ships wait on it and a button that cancels it, and says beside the list that any order to their ships takes them out (ADR-080 decisions 5 and 9).
- **Its form gives the selection a new one, a field at a time with arrows:** the trigger; a time of day's hour, and its minute in steps of five, or the held sector an event trigger watches; the action; the sector whose node a move, attack-move, patrol or hold goes to; an attack's or a rig's target in that sector; and the condition (ADR-080 decision 9).
- **It says when the order fires on the player's clock and how long until then,** and its button says why it cannot give the order while it cannot (ADR-080 decision 9).
- **The form starts at an attack-move at 02:00,** and keeps its picks while the window is closed, for the match (ADR-080 decision 9).
- **The selection panel names the scheduled order a ship waits on,** "Scheduled:" and the order in words, or how many orders its selection waits on (ADR-080 decision 9).

### Retreat [§orders.retreat]

#### The threshold [§orders.retreat.threshold]

- **Every ship carries a retreat: never, below half its hit points, or below a quarter** (ADR-075 decision 4). A quarter is the default, so that a fight to the last ship is a choice (ADR-075 decision 4).
- **A design carries one,** and every ship built to it starts with it. A Constructor starts at a quarter. The pirates' ships, and a battle matchup's, never retreat (ADR-075 decision 4; ADR-083 decision 2).
- **The selection's retreat is a row of three,** RETREAT over "25%", "50%" and "Never", under its buttons: the first ship's setting lit, or none lit and MIXED beside the label while the ships differ. A press sets every selected ship to it, Constructors included, and gives them no order (ADR-088 decision 3; ADR-075 decision 7).
- **The designer's retreat is the same row,** between Rename and the ship count, and a press sets the design's. Saving sends it; a saved design whose retreat changed under the same name is saved with UPDATE. Ships built already keep theirs (ADR-088 decision 3; ADR-075 decision 7; Game §ships.designer.queue).

#### Going back [§orders.retreat.going]

- **A hit that leaves a ship below its threshold sends it back to be repaired,** unless it is already going. It is checked once the tick's hits have landed, splash included, so a ship the hit destroys does not go (ADR-075 decision 5).
- **The ship's attack or work order ends, and it moves to its repairer,** firing on the move (ADR-075 decision 5; ADR-014 decision 9).
- **Its repairer is the nearest Repair Bay of its player's, by straight distance; without one the Command Station; without that the nearest Shipyard.** A tie goes to the older structure. With none at all, the ship does not retreat and fights on (ADR-075 decision 5).
- **Each ship goes on its own,** to a point 40 m beyond its repairer's footprint on the side it comes from, so that every one stops within the repair's reach (ADR-075 decision 5).
- **Once a second, a retreating ship whose repairer has fallen goes to the next one** by the same rule (ADR-075 decision 5).
- **A repairer repairs the ships near it whether they retreat or not;** how many, how far and how fast are Game §neutrals.repair.base ([Catalogue: shipRepair](Reference/Catalogue.md#shipRepair); ADR-075 decision 3).
- **The player sees it go:** the selection panel counts the ships retreating to be repaired (ADR-075 decision 10), a retreating ship's bar has a line round it (ADR-088 decision 4), and an alert says "Ship retreating" (ADR-059 decision 1; Game §interface.alerts).

#### What ends a retreat [§orders.retreat.end]

- **A retreat ends when the ship is whole.** It then waits where it is, or, on a standing order, rejoins its group when the group next moves (ADR-075 decision 6; ADR-059 decision 5).
- **Any order the player gives the ship ends its retreat,** and so does setting it never to retreat. A scheduled order ends it only when it fires, and setting the threshold to a half or a quarter leaves it going (ADR-075 decisions 6 and 7; ADR-080 decision 5).
- **The next hit that leaves it below its threshold starts it again,** so a damaged selection sent back into a fight turns round ship by ship as each is hit (ADR-075 decision 6).
- **A ship never returns to the fight by itself after a repair,** other than by its standing order (ADR-075).

---

## Fog and memory [§sight]

A player sees what its own ships and structures see, all of each sector it holds that is not suppressed, and an enemy that has just hit it. It remembers the enemy structures and the derelicts it has seen, as it last saw them, until it sees their place again. The fog is the server's: each player's snapshot holds only what that player may see, so no game, the AI's included, is ever sent more (ADR-002 decision 4; ADR-024 decisions 3 and 6).

### Where the fog is [§sight.where]

- **Every skirmish and every universe is played under fog of war,** and so are the measurement runs (ADR-024 decision 2). A battle matchup is fought in the open (ADR-083 decision 2; Game §session.matchup).
- **A snapshot under fog says so,** and the game draws the fog only then (ADR-024 decision 6).
- **The AI plays under the same fog** as the player (ADR-020 decision 17; Game §ai.client).

### How far a thing sees [§sight.range]

- **An armed ship or structure sees its weapon's range and the sight margin beyond it.** Anything without a weapon sees the unarmed sight ([Catalogue: sight](Reference/Catalogue.md#sight), [Catalogue: weapons](Reference/Catalogue.md#weapons), [Catalogue: structureWeapons](Reference/Catalogue.md#structureWeapons); ADR-024 decisions 1 and 3). So a Command Station and a Defence Platform see past the Defence gun's reach, and a Constructor, a Shipyard, a Research Lab, a Mining Rig, a Relay and a Repair Bay see the unarmed sight.
- **A structure under construction sees as an unarmed one,** since it carries no weapon until it is finished (ADR-024 decision 3).
- **A ship with a Sensor Array sees the further of its module's sight and its own** ([Catalogue: modules](Reference/Catalogue.md#modules); ADR-058 decision 5; Game §ships.modules).
- **Sight is a circle on the plane,** measured from the seer's center to the other's edge (ADR-024 decision 3). Nothing on the map blocks it: there is no line of sight, height or terrain (ADR-024).
- **Sight follows the weapon, so research never changes it:** no research extends a weapon's range (ADR-017; ADR-033; Game §research.upgrades).
- **A held sector is seen whole.** All of each sector the player holds that is not suppressed is in its sight. A suppressed sector's Relay sees only as far as an unarmed structure does, and a home sector, which is never suppressed, is always seen whole (ADR-056 decision 9; Game §territory.gives).
- **Fog never changes what anything fires at.** Every armed entity sees beyond its weapon's range, so whatever it can shoot it sees, and targeting never asks what is in sight: a battle plays tick for tick the same with fog and without (ADR-024 decision 4; Game §combat.targeting).

### What a player sees [§sight.seen]

- **Its own ships and structures,** always, each with how far it sees (ADR-024 decision 6).
- **The map's asteroids and fields,** always, fog or not (ADR-024 decision 3; Game §map.plane).
- **The territory,** always, fog or not (Game §territory.shown).
- **An enemy ship or structure while it is in sight:** within the sight of one of the player's own, or in a held sector that is in sight. The pirates' ships and structures, and the derelicts, are seen as an enemy's are (ADR-024 decision 3; ADR-073 decision 1; ADR-074 decision 7).
- **What it sees of an enemy:** where it stands, its hit points, how far it is built, a structure's level and an upgrade under way, and a warship's hull, drive, weapon and module. It never sees an enemy structure's queue or research, nor a ship's orders (ADR-024 decision 6; ADR-020 decision 3; ADR-064 decision 6; ADR-079 decision 5).
- **What a derelict holds,** its Ore and the topic it names, once the derelict is in sight (ADR-074 decision 7; Game §neutrals.derelicts).
- **The Ore left in an asteroid in sight,** which it keeps as it last saw it (Game §economy.reserves).
- **A shot,** when the player sees its shooter or its target, and **a destruction,** when it happens within the player's sight (ADR-024 decision 6).
- **The pirates keep no sight of their own.** They guard their sector by distance, through the fog, as a Defence gun fires (ADR-073 decisions 1 and 5; Game §neutrals.guarding).

### A shooter is revealed [§sight.reveal]

- **An enemy that hits a player's ship or structure is seen by that player** for the reveal time after the hit, wherever it fires from ([Catalogue: sight](Reference/Catalogue.md#sight); ADR-024 decisions 1 and 3). So a shooter is seen while it keeps hitting, but one whose weapon fires less often than the reveal lasts is seen only for the reveal time after each hit (ADR-024 decision 1).
- **Each hit starts the reveal again,** and a splash hit reveals the shooter to every side it hits (ADR-024 decision 4).
- **A revealed ship is seen, not remembered:** once the reveal ends and nothing of the player's sees it, it is gone from the player's view (ADR-024 decisions 5 and 6).

### Memory [§sight.memory]

- **A player remembers every enemy structure and every derelict it has seen,** as it last saw it: where it stands, its hit points, how far it is built, its level, what a derelict holds, and the tick of the last snapshot that showed it (ADR-024 decision 5; ADR-081 decision 1; ADR-064 decision 6; ADR-074 decision 7).
- **Ships are not remembered.** An enemy ship is shown only while it is seen (ADR-024 decisions 5 and 6).
- **A memory changes only when the place is seen again,** a revealed Shipyard aside (below). An upgrade, damage or building done out of the player's sight shows once the player sees the structure again (ADR-064 decision 6; ADR-024 decision 5).
- **A memory is forgotten once its place is in sight and it is not seen there,** which is how a player learns that a structure is gone (ADR-024 decision 5).
- **The last production is revealed.** While a player has no Command Station, each of its finished Shipyards is added to every other player's memory at the end of each tick, or brought up to date there, unless that player sees it. A Shipyard still a site is not revealed (ADR-037 decision 3; Game §universe.loss).
- **What a player has seen and remembers is part of the simulation's state.** It replays from the command log, a universe's save keeps it, and the first snapshot after a seat is taken again shows everything the player sees and remembers (ADR-024 decision 5; ADR-077 decision 3; ADR-079 decision 5; ADR-081 decision 1; Game §universe.persistence).
- **Where the ground has been seen before is the player's game's own record,** kept for as long as its match is on screen. A game that joins a universe, or joins it again, starts that record afresh; the server's memory of structures, derelicts and reserves is whole all the same (ADR-024 decision 9).

### What fog does to orders [§sight.orders]

- **An attack needs a target the player sees, or an enemy structure it remembers.** Any other target is refused (ADR-024 decision 7).
- **An attack on a ship ends once nothing of the attacker's side sees the ship.** An attack on a remembered structure goes where the player saw it (ADR-024 decision 7).
- **A salvage order needs a derelict the player sees or remembers** (ADR-074 decision 4; Game §neutrals.salvage).
- **A structure ordered onto something the player cannot see,** such as an enemy's rig on an asteroid, is refused as any blocked placement is, and the player is told only that it cannot stand there (ADR-024 decision 7; ADR-094 decision 4).
- **The player's game orders a Mining Rig only onto an asteroid it has seen:** the rig's ghost is red over one never seen, and a click there orders nothing. The server itself takes such an order (ADR-046 decision 8).
- **What a player sees decides what its standing and scheduled orders answer.** A group holding a sector answers the enemy ships its player sees in it (ADR-059 decision 4). A scheduled order's condition counts the enemy and pirate warships its player sees in the action's sector (ADR-080 decision 4). An enemy is seen entering a held sector only where its player sees it, so a warship that suppresses a Relay from beyond the Relay's own sight enters unseen, and the suppression is the event (ADR-080 decision 2; Game §orders.scheduled; Game §interface.alerts).

### How the fog is drawn [§sight.drawn]

- **The game keeps a grid of 20 m cells over the map.** A cell is clear while its center is within the sight of one of the player's own ships or structures, or inside a sector the player holds that is not suppressed; it is dimmed once it has been seen before, and dark where it never has (ADR-024 decision 9). The grid is brought up to date once a snapshot, only where something changed (ADR-052 decision 1).
- **In the main view, the fog darkens the ground and everything that stands in the fog,** blended between cells so that no cell's edge shows. Outside the map there is no fog: it is the sky (ADR-024 decision 9; ADR-052 decision 3).
- **The ground's fog is darker than the minimap's.** Never seen covers 0.9 of what is under it, in both. Seen before covers 0.55 on the minimap, where telling explored from never seen matters for scouting, and 0.8 on the ground, so that the edge of sight is where the grid and the stars go out (ADR-024 decision 9; ADR-081 decision 6).
- **The minimap draws the same fog over its marks** (ADR-052 decision 4; Game §interface.minimap).
- **The territory is drawn over the fog,** in the main view and on the minimap (ADR-081 decisions 3 and 7; Game §territory.shown), and so are the alerts' marks on the minimap (ADR-059 decision 2; Game §interface.alerts).
- **What the player only remembers is drawn as a memory, over the fog.** In the main view a remembered structure or derelict is its lines alone, with no faces, no health or build bar and no footprint ring, in its side's color halfway to gray, at 0.6. On the minimap it is a cross in its side's color, 8 units across, while what the player sees is a filled square under the fog (ADR-081 decision 4; Game §interface.minimap).
- **A memory says how old it is,** under the pointer (ADR-081 decision 4; Game §interface.hud.hint).
- **The grid covers a map of at most 512 cells a side,** 10,240 m at 20 m a cell (ADR-052 decision 2).

---

## The AI [§ai]

The AI is a computer player that plays the game by the player's rules. It plays the second seat of a skirmish at the difficulty the menu named, the seat of an AI empire in a universe, and, as a deputy, the seat of a player who is away from a universe. All three are one library, `Opponent`, playing from the snapshots a player's own game reads and answering with the commands a player gives (ADR-002 decision 7; ADR-020 decision 1; ADR-079 decisions 3 and 6). In a battle matchup the AI is its battle behavior instead (ADR-083 decision 4; Game §session.matchup).

### A player like any other [§ai.client]

- **The AI is a client.** It reads its own player's snapshot and answers with commands, which the server checks against ownership, cost and legality as it checks the player's, and applies or refuses (ADR-002 decisions 2 and 7; ADR-020 decision 1). It cannot read the server's state: its library can include only the protocol (ADR-002 decision 7).
- **It sees only what its player sees.** Fog of war binds it as it binds the player: it answers the warships it has seen, attacks what it sees or remembers, and knows an outpost or a derelict only once it has seen it (ADR-020 decision 17; ADR-076 decision 1; Game §sight).
- **It reads every warship it sees by its components,** whoever owns it, since a design's identifier names a design in its owner's list only (ADR-020 decision 3).
- **It watches every snapshot and decides once a second.** It looks at every snapshot for shots on its structures, since a shot is in the snapshot of its tick only, and gives its orders at most once a second (ADR-020 decision 1).
- **It ignores the refusals it is told.** The server tells the AI its refusals as it tells the player (Game §interface.alerts), and the AI keeps track of where it sent each Constructor instead, taking a site it ordered as refused when the site has not appeared 3 s later (ADR-020 decision 7; ADR-094 decision 6).
- **No difficulty cheats.** The AI never has more Ore, sight or hit points than its rules give any player; a harder AI plays its hand differently, never with a larger one (ADR-065).
- **It never kites** (ADR-020 decision 8).
- **In a skirmish the AI is player 2,** drawn with the Tarkan set, and the game plays it beside the player's own client, handing it every snapshot of its seat (ADR-020 decisions 1 and 10; Game §look.models.sets; Game §session.skirmish).

### Difficulties [§ai.difficulty]

- **The AI plays at Easy, Normal or Hard:** a skirmish at the difficulty the menu's button named, and a universe's AI empire at its seat's difficulty (ADR-065 decisions 1 and 4; ADR-079 decision 7; Game §session.menu).
- **Each difficulty is a settings file of its own:** Normal is `Opponent.json`, and `OpponentEasy.json` and `OpponentHard.json` stand beside it, each whole and read as strictly as the tuning data. The server never reads them: they are how one player plays, not the match's rules (ADR-065 decision 1; ADR-020 decision 2).
- **Every difficulty plays the same rules; only the numbers differ.** How Easy and Hard differ from Normal is ADR-065 decisions 2 and 3, and each file's numbers stand side by side in [Catalogue: Opponent](Reference/Catalogue.md#opponent).
- **A deputy plays from Normal's file,** whatever the difficulty of the universe's AI empires (ADR-079 decision 6).

### Its base [§ai.base]

- **The AI lays its base plan at its first decision,** round its Command Station (ADR-020 decision 5):
  - rigs on the ore asteroids nearest its station ([Catalogue: homeAsteroids](Reference/Catalogue.md#homeAsteroids));
  - a Shipyard, a Research Lab and a Defence Platform to the sides and front of the station, toward the map's center;
  - rigs on the next nearest asteroids, leaving out any nearer the enemy's base than its own, each with a Defence Platform beside it on the side of its base ([Catalogue: contestedAsteroids](Reference/Catalogue.md#contestedAsteroids));
  - its share of Defence Platforms round the base for its first Shipyard (below);
  - and its rally, toward the map's center from the station, where its reserve gathers ([Catalogue: rallyDistanceMeters](Reference/Catalogue.md#rallyDistanceMeters)).
- **It takes the enemy's base to be across the map's center from its own** until it sees the enemy's Command Station, since the map is point-symmetric (ADR-020 decision 17; Game §map.placement).
- **Each place keeps a gap clear of everything else,** so that ships still pass ([Catalogue: structureGapMeters](Reference/Catalogue.md#structureGapMeters)). A place that is taken is found again by searching outward in rings, it is checked again when its structure is ordered, and a structure with no place left is given up while the rest of the plan goes on (ADR-020 decision 5).
- **It builds its plan in order,** at most two Constructors to a site, the nearest idle ones first, and the rest go on to the next site. A site with nobody on it gets the idle Constructors. A destroyed structure of the plan is built again. Constructors left over repair a damaged structure, one each (ADR-020 decisions 5 and 7).
- **Ships wait for the plan's Ore.** No warship is queued while the plan's next structure waits for Ore (ADR-020 decision 5). Its scouts are the exception (ADR-020 decision 13).
- **It keeps its Constructors** at its settings' count, those its Command Station has queued counted ([Catalogue: constructors](Reference/Catalogue.md#constructors); ADR-020 decisions 2 and 7).
- **Shipyards join the plan as its income grows.** It plans its N-th Shipyard once its income reaches N times its settings' income for a Shipyard, and a planned one waits while the income is below its share, as after a rig is lost ([Catalogue: incomePerShipyardOrePerSecond](Reference/Catalogue.md#incomePerShipyardOrePerSecond)). They stand behind the station, then behind it to either side, each round of three farther out, at the nearest clear place (ADR-020 decision 6). A Shipyard stands only in a sector it holds (ADR-056 decision 5; Game §territory.gives).
- **It fortifies its base for each Shipyard:** its settings' Defence Platforms round the base, each built once its income reaches that Shipyard's share ([Catalogue: homePlatformsPerShipyard](Reference/Catalogue.md#homePlatformsPerShipyard)). They stand toward the map's center, 260 m from the Command Station at turns of 0, ±0.5 and ±1 radian, then in a ring 70 m farther out (ADR-041 decision 3).
- **It follows the ore.** When one of its rigs' asteroids runs dry, it plans a rig, with a Defence Platform beside it, on the nearest asteroid it knows still holds ore, outside the enemy's home. An asteroid it has never seen counts as holding ore. A dry rig stays, for its trickle (ADR-020 decision 5; ADR-035 decision 4; Game §economy.reserves).
- **On a map with sectors, a rig waits for its sector.** A rig in a sector the AI does not hold is planned after a Relay on that sector's node, which is built once the sector is next to its territory. A rig or a Relay in a sector the enemy holds or the pirates guard waits, without holding up the rest of the plan, and so does a Relay whose node a wreck covers. A Shipyard, a Research Lab, a Defence Platform or a Repair Bay is placed and ordered only in a sector it holds; a Defence Platform beside a contested rig waits for the rig's sector, as the rig does (ADR-020 decisions 13 and 19; ADR-076 decision 8; Game §territory.lattice).
- **It researches in its settings' order:** at each decision, the first topic of the order not yet researched whose prerequisites are done and whose tier its Lab has opened, while its Lab has a slot free ([Catalogue: researchOrder](Reference/Catalogue.md#researchOrder); ADR-020 decision 14; Game §research.queue).

### What it builds [§ai.counters]

- **It answers the enemy's commonest design.** At every review it finds the most common design, by hull, drive and weapon, among the enemy warships it has seen since the last review, and builds the first of that design's answers that it has unlocked, or its default design when it has none ([Catalogue: reviewIntervalSeconds](Reference/Catalogue.md#reviewIntervalSeconds), [Catalogue: counters](Reference/Catalogue.md#counters), [Catalogue: defaultDesign](Reference/Catalogue.md#defaultDesign); ADR-020 decisions 8 and 17).
- **A tie goes to the lowest hull, then drive, then weapon** (ADR-020 decision 8).
- **Having seen no warship, it keeps what it builds,** which at the start of a match is its default design (ADR-020 decision 17).
- **Its answers are data:** each enemy design's answers, in order of preference (ADR-020 decisions 2 and 8).
- **The pirates are no fleet to answer:** their warships are not counted (ADR-076 decision 7; Game §neutrals.pirates).
- **It saves a design it does not have** before it queues it, and keeps its settings' jobs in each Shipyard's queue that can build the design ([Catalogue: shipyardQueueJobs](Reference/Catalogue.md#shipyardQueueJobs); ADR-020 decisions 8 and 14).
- **Its designs keep the default retreat,** so its ships turn for home at the default threshold (ADR-076 decision 9; Game §orders.retreat.threshold).

### Structure levels [§ai.levels]

- **Its Command Station grows for the nodes it wants.** A Relay of its plan that the node cap holds back has the station's next level ordered in its place in the plan's order, and the station goes no higher for nodes while it wants no further one. Which claim waits for which level follows from the station's node caps ([Catalogue: structures](Reference/Catalogue.md#structures); ADR-020 decision 14; Game §territory.cap).
- **It grows for the fleet too.** The station is also upgraded when the fleet cap would hold back a round of its Shipyards, a ship of its production design from each, since a level takes a while to build ([Catalogue: Command Station levels](Reference/Catalogue.md#command-station-levels); ADR-020 decision 15; Game §economy.fleetcap).
- **Its Shipyards grow to the hull it builds.** A Shipyard below the level its production design's hull needs is upgraded, one Shipyard and one level at a time, and the design is queued only at Shipyards of that level. Its default design is a Medium hull, so its first Shipyard is upgraded before its first warship of that design; its scout, a Small hull, is built at level 1 (ADR-020 decision 14; Game §structures.shipyard).
- **Its Research Lab grows for its research order.** The Lab is upgraded when the next topic of its order is of a tier the Lab has not opened, once what the level requires is researched, and to the level that gives a second slot once its open tier has reached its settings' tier while two topics of its order are left ([Catalogue: secondSlotTier](Reference/Catalogue.md#secondSlotTier); ADR-020 decision 14).
- **An upgrade it cannot yet pay for holds back new production,** so that the Ore gathers for it. An upgrade never takes the Ore of a structure of its plan that waits for a Constructor (ADR-020 decision 14).

### Its fleet [§ai.fleet]

- **Its warships gather in reserve.** On a map with sectors the reserve holds a sector with a standing order: the sector it holds next to one the enemy holds nearest the rally, or else the rally's own sector while it holds it. Otherwise it gathers at the rally (ADR-020 decisions 8 and 13; Game §orders.standing). Its scouts, its raid, its detachment and a ship that is retreating are not part of the reserve (ADR-020 decision 13; ADR-076 decision 9).
- **It defends every structure it has.** A shot on any of its structures, built or a site, sends the reserve there with an attack-move, and the reserve goes back once no shot has come for its settings' hold ([Catalogue: defenseHoldSeconds](Reference/Catalogue.md#defenseHoldSeconds)). A reserve ship is sent again only when where it goes, or whether it holds a sector there, changes (ADR-020 decision 8).
- **The main attack.** Once the reserve holds the attack group's size, the whole reserve joins the attack group. The size grows by its settings' share for each research tier its Lab has opened past the first ([Catalogue: attackGroupShips](Reference/Catalogue.md#attackGroupShips), [Catalogue: attackGroupGrowthPerTier](Reference/Catalogue.md#attackGroupGrowthPerTier); ADR-041 decision 1; ADR-020 decision 8).
  - **The fleet cap can make it smaller.** When the cap leaves room for fewer ships of its production design's hull than that, beside its scouts and its raid, the group is its settings' share of that room, and never less than one ship ([Catalogue: attackCapShare](Reference/Catalogue.md#attackCapShare); ADR-041 decision 1; Game §economy.fleetcap).
  - **On a map with sectors it waits for a lead in nodes** ([Catalogue: attackNodeLead](Reference/Catalogue.md#attackNodeLead)), or else for a reserve larger by its settings' share, or for the whole room the cap leaves it if that is less ([Catalogue: attackWithoutLeadShare](Reference/Catalogue.md#attackWithoutLeadShare)). A node nobody holds counts for neither side, and it counts the nodes from its snapshot, since every player sees who holds each (ADR-041 decision 5; ADR-056 decision 10; Game §territory.shown).
- **It goes for production first.** The attack group ranks the enemy structures it sees or remembers: Shipyards, then the Command Station, then the rest. It attack-moves on the nearest of the best rank to the group's center, each level a structure has above the first counting as its settings' distance nearer ([Catalogue: attackLevelMeters](Reference/Catalogue.md#attackLevelMeters)). It keeps a target until the target is gone, or until a structure of a better rank comes to light (ADR-041 decision 6).
  - **Knowing none, it goes to look,** across the map's center from its own base (ADR-041 decision 6; ADR-020 decision 17).
  - **Its ships within 500 m of the target's edge are ordered to attack it,** once each, since an attack-move ends at each ship's place in the group's formation, which round a large structure can stand out of the ship's range (ADR-041 decision 7).
  - **It never sends the group at the pirates' structures** (ADR-041 decision 6; ADR-076 decision 7).
- **It falls back from a losing attack.** An attack that has lost its settings' share of the ships it set out with goes back to the rally with a move order, out of the fight, and its ships rejoin the reserve, which then waits before it attacks again, however many ships it has ([Catalogue: retreatLossShare](Reference/Catalogue.md#retreatLossShare), [Catalogue: regroupSeconds](Reference/Catalogue.md#regroupSeconds)). The losses count against the group's size when it last grew, and a ship that turns for home to be repaired counts as one lost (ADR-041 decision 2; ADR-076 decision 9).

### Territory [§ai.territory]

- **It claims the free sector next to its territory nearest its base,** once its first Shipyard stands: a Relay on the node, and a rig on each of the sector's asteroids, which wait for the Relay. It claims one sector at a time, once every Relay it has planned stands or waits on the enemy, and at most its settings' count beyond the sectors its rigs take it to ([Catalogue: claimSectors](Reference/Catalogue.md#claimSectors); ADR-020 decision 13; Game §territory.lattice).
- **It claims nothing the pirates guard.** It plans neither a Relay nor a rig in a guarded sector, and a claim that turns out to be guarded is dropped and another made in its place (ADR-020 decision 13; ADR-076 decision 7).
- **It fortifies its front.** A sector it holds next to one the enemy holds, its home aside, is its front, and the Relay there gets its settings' Defence Platforms beside it, toward its base, once ([Catalogue: frontPlatforms](Reference/Catalogue.md#frontPlatforms); ADR-020 decision 13).
- **It keeps a scout,** of its scout design, which carries a Sensor Array ([Catalogue: scouts](Reference/Catalogue.md#scouts), [Catalogue: scoutDesign](Reference/Catalogue.md#scoutDesign)). A scout is queued ahead of its warships and of a structure waiting for Ore, so that the first sets out in the opening minutes, and it is not part of the reserve (ADR-020 decision 13; ADR-058 decision 5).
  - **First it explores its own half,** for its settings' time from the match's or the universe's first tick, so an AI empire made afresh later in a universe sends its scout round the enemy's flanks at once ([Catalogue: scoutOwnHalfSeconds](Reference/Catalogue.md#scoutOwnHalfSeconds)). It moves, rather than attack-moves, from node to node of the sectors nearer its base than the enemy's, its home aside, the nearest first. A guarded sector it looks at from a lookout 650 m from its node toward its base, or nearer when its base is nearer, outside the reach in which the pirates attack and within its Sensor Array's sight ([Catalogue: pirates](Reference/Catalogue.md#pirates), [Catalogue: modules](Reference/Catalogue.md#modules)). A lookout is turned up to 0.7 radians either way to keep clear of asteroids and structures, and dropped when none is clear (ADR-076 decision 4).
  - **Then it tours the enemy's flanks,** attack-moving from one sector next to the enemy's home to the next, so that it meets what is there and suppresses the Relays there while it lives. It takes the enemy's home to be across the map's center from its own (ADR-020 decision 13; Game §territory.suppression).
- **It raids.** While its reserve holds at least twice its raid's size, that many of the reserve's smallest hulls, which are the fastest, attack-move on the nearest enemy sector, not the enemy's home, by whose node it sees no enemy or pirate warship within 600 m ([Catalogue: raidShips](Reference/Catalogue.md#raidShips)). Those that come near the sector's Relay attack it (ADR-020 decision 13).
  - **A raid ends** once the sector stops being the enemy's, or once it has lost its settings' share of its ships. Its ships then rejoin the reserve, and the next raid waits its settings' interval ([Catalogue: raidLossShare](Reference/Catalogue.md#raidLossShare), [Catalogue: raidIntervalSeconds](Reference/Catalogue.md#raidIntervalSeconds); ADR-020 decision 13).
- **Its ties are mirrored.** Two places as near as each other are told apart by the side of the line from its base to the map's center each lies on, so that the AIs of both seats choose mirrored places (ADR-020 decision 13).

### Pirates, salvage and repair [§ai.neutrals]

- **It knows an outpost by what it has seen of it.** For each guarded sector it keeps the most Ore it has seen there, the pirates' ships at their design's cost and their Defence Platforms at the platform's, and the most platforms; more than one platform is a stronghold. A sector the pirates stop guarding is forgotten, and an outpost it has never seen is never a target (ADR-076 decision 1; Game §neutrals.outposts).
- **It clears an outpost by detachment.** It takes ships from its reserve, the nearest to the outpost first, until their Ore reaches its settings' margin times the outpost's, and with too few ships it waits ([Catalogue: pirateMargin](Reference/Catalogue.md#pirateMargin)). Its target is the nearest to its base of the outposts it has seen next to its territory: any camp, and a stronghold only when no unguarded free sector next to its territory is left and it is not at its node cap (ADR-076 decision 2).
  - **The detachment attack-moves on the outpost's node,** ordered on again every 10 seconds, until the pirates stop guarding the sector, and its ships then rejoin the reserve (ADR-076 decision 2).
  - **It falls back** to the rally once it has lost the raids' share of its ships, and the next waits the raids' interval (ADR-076 decision 2).
  - **One detachment at a time,** chosen after the raid, so that a raid comes first (ADR-076 decision 2).
- **Otherwise it leaves the pirates alone:** its other groups do not attack their structures. Its fleets are not routed round the outposts, so a group whose way passes within an outpost's guard of its node fights the outpost there (ADR-076 decision 7).
- **It salvages with one Constructor at a time.** Once it has as many Mining Rigs as its home asteroids, its nearest idle Constructor goes to the derelict nearest its base that it knows of, in a sector it holds or in a free sector next to one it holds, never in a sector the pirates guard. It goes ahead of the contested rigs of its plan (ADR-076 decisions 5 and 8; Game §neutrals.salvage).
  - **A trip ends** when the derelict is gone. A trip not done in 300 seconds, or whose Constructor is lost or turns for home, is dropped, and a derelict given up for time is never chosen again. The Constructor on a trip is no one's crew, so the plan's builds go on without it (ADR-076 decision 5).
- **It builds a Repair Bay behind its front.** Once it has a finished Shipyard, it plans a Repair Bay 220 m behind its front sector's node, toward its base. When its front moves to a sector without a Bay, a new one is planned there, and the Bays already built stay. A Bay in a sector it has lost waits (ADR-076 decision 6; Game §neutrals.repair.bay).
- **It leaves its retreating ships alone.** It gives a ship that is going back to be repaired no order, since any order would end the retreat. A retreating Constructor leaves its crew, and is given no work until it is whole (ADR-076 decision 9; Game §orders.retreat).

### From any state [§ai.anystate]

- **The AI keeps nothing a server could save,** so an AI made afresh, or one handed an empire it did not build, plays on from the snapshot it is given (ADR-020 decision 16).
- **Its plan adopts what stands.** A slot for a Shipyard, a Research Lab, a Defence Platform or a Repair Bay takes an own structure of that kind that no slot holds, the nearest within 400 m of the slot's place, rather than a new place beside it. A rig's and a Relay's slots stand where their structures already do. The Relays of the sectors it holds beyond its home become its claims, and count against its claims (ADR-020 decision 16).
- **Its player's ships join its reserve** (ADR-020 decision 16).
- **Without a Command Station** its plan is laid round its oldest Shipyard, or else its oldest structure; with no structure at all it waits (ADR-020 decision 16).
- **It plays on without its Command Station.** It keeps its Shipyards' queues full, researches, builds its plan, repairs with the Constructors it has, and plans new Shipyards round where its station stood. It queues no Constructors (ADR-020 decision 18; Game §universe.loss).

### AI empires [§ai.empire]

- **An AI empire is the AI playing a universe's seat of its own, to win,** at its seat's difficulty (ADR-079 decisions 3 and 7; Game §universe.seats).
- **The server plays it on its own thread.** After each tick, once every player has been sent its snapshot, the AI empire is handed its seat's snapshot, and its orders are applied at the next tick and logged as a player's are, so a universe replays from its log without it (ADR-079 decision 2).
- **Made afresh after the server restarts,** it plays on from the universe the server restored, though not as it would have (ADR-079 decision 3; ADR-020 decision 16).
- **When its seat restarts after a loss,** it lays its plan afresh round its new Command Station (ADR-084 decision 6; Game §universe.restart).

### The deputy [§ai.deputy]

- **A deputy keeps an absent player's empire running and plays for nothing more.** When it plays the seat and when it hands it back is Game §universe.handover. It decides once a second while it plays, and plays from the Normal AI's file for its research order and how long its defense holds (ADR-079 decision 6).
- **It learns while it watches.** While the player plays the seat, the deputy gives no order, and learns what each Shipyard last queued and where the player's rigs stand (ADR-079 decisions 1 and 6).
- **Research.** A Research Lab with a slot free takes the next topic of the AI's research order, and once that is done, any topic it can take (ADR-079 decision 6; Game §research.queue).
- **Production.** A finished Shipyard whose queue is empty is given again the design it last queued. One it never saw build, as after the server restarted, is given the design most of its player's warships are of, if its level can build it. It keeps one job queued, which waits for the Ore and the fleet cap as any job does. When the cap holds a Shipyard's next ship back, it upgrades the Command Station once the Ore is there (ADR-079 decision 6; Game §economy.fleetcap).
- **Constructors.** It keeps as many as its player had when it took the seat, counting those the Command Station has queued (ADR-079 decision 6).
- **Rigs.** It rebuilds a Mining Rig it saw stand, on an asteroid that holds ore as far as it knows, in a sector its player holds and the pirates do not guard, with the two nearest idle Constructors (ADR-079 decision 6).
- **Repairs.** Each damaged structure, and each site with nobody on it, gets the nearest idle Constructor (ADR-079 decision 6).
- **Defense.** Enemy warships in a sector its player holds, the pirates' among them, draw its idle warships, to the sector with the most of them; so does whatever fires on one of its player's structures in such a sector. Idle means no order, no standing order, not retreating and waiting on no scheduled order. They attack-move there. The defense ends once the attack has not been seen for the defense's hold ([Catalogue: defenseHoldSeconds](Reference/Catalogue.md#defenseHoldSeconds)), or at once when the sector is lost, and each defender then moves back to where it stood (ADR-079 decision 6; ADR-080 decision 5).
  - **Any enemy warship draws every idle warship,** a passing scout among them, so one held sector can be stripped to answer another (ADR-079 decision 6).
- **What it never does.** It never attacks, raids, claims a sector, clears pirates or salvages. A scheduled order of its player's does those things, and the server fires it whoever plays the seat (ADR-079 decision 6; ADR-080 decision 5; Game §orders.scheduled).
- **It leaves the player's orders standing.** It gives no order to a ship that is retreating, that has an order or a standing order, or that waits on a scheduled order (ADR-079 decision 6; ADR-080 decision 5).
- **Each turn starts afresh.** The Constructors it keeps, its repairs and its defenders belong to the turn. What it learned while watching, each Shipyard's last design and where the rigs stood, it keeps, and nothing of it is saved (ADR-079 decisions 2 and 6).

---

## Universe [§universe]

A universe is a match that does not end. It runs on a dedicated server that the owner keeps going (Game §session.server), keeps running while its players are away, saves itself every minute, and comes back from a restart with what it had. A player who loses in it starts again. How a player joins one is Game §session.join; the rule of loss, which a match and a universe share, is here.

### What a universe is [§universe.overview]

- **A universe is played on the map a skirmish is played on,** under fog of war, with its pirates and derelicts placed from the universe's seed, and a seat for each of the map's starts ([Catalogue: starts](Reference/Catalogue.md#starts); ADR-078 decision 2; ADR-024 decision 2; Game §map).
- **A universe's rules are its own, set when it is made and saved with it** (ADR-084 decision 1).
- **Domination is off in a universe.** No player has tickets, none drain, and the territory panel shows neither (ADR-084 decision 2; ADR-057 decisions 4 and 8; Game §territory.domination).
- **No match ends in a universe.** A player who loses restarts (Game §universe.restart), and the universe runs until the owner stops its server (ADR-084 decision 2; ADR-078 decision 1).
- **A universe's rules differ from a match's in two things only,** domination and what a loss does; the economy, territory, the fleet cap, pirates, salvage and repair play in a universe as they do in a skirmish (ADR-084 decisions 1 to 4).

### Seats [§universe.seats]

- **Each of the map's starts is a seat,** and each seat is a player's or an AI empire's, as the universe's settings say (ADR-078 decision 2; ADR-079 decision 7).
- **An AI empire is the AI, playing its seat to win** at its difficulty, on the server (ADR-079 decisions 3 and 7; Game §ai.empire). It has no token and no join file (ADR-079 decision 7).
- **A player's seat is taken with its token,** 128 bits from the system's cryptographic source, which the universe's settings and the seat's join file hold as plain text. Anyone who has the file can take the seat: that is the trust of a circle of friends (ADR-078 decision 3).
- **A seat is taken at any time while the universe runs, and again after a dropped connection,** and the newest connection holds it (ADR-078 decision 4; Game §session.join).
- **A seat is played by one controller at a time:** its player, its deputy, or, for an AI seat, the AI empire (ADR-079 decisions 3 and 4; Game §universe.handover).

### The hand-over [§universe.handover]

- **A player's seat is its deputy's until the player first takes it** (ADR-079 decision 4).
- **The player plays the seat while its newest connection is open, and for 60 s of ticks after the server sees that connection gone.** A connection the game closes is seen gone at once; one whose game vanished is seen gone once the transport's idle timeout ends it. After the 60 s the deputy plays the seat until the player takes it again, which hands it back at once (ADR-079 decision 4). So a connection dropped and taken up again within the minute never sees the deputy.
- **The deputy is a keeper:** it keeps the empire running and defends its held sectors, and goes no further (ADR-079 decision 6; Game §ai.deputy).
- **Orders are the server's, whoever plays the seat.** What the deputy ordered stands until the player changes it, and the player's standing and scheduled orders go on while the deputy plays: the deputy leaves the player's orders standing (Game §ai.deputy), and the server fires a scheduled order whoever plays the seat (ADR-079 decision 2; ADR-080 decision 5; Game §orders.scheduled.firing).
- **While the player plays, the deputy only watches;** what it learns is Game §ai.deputy (ADR-079 decision 1).

### What the player missed [§universe.away]

- **While the deputy plays a seat its player has held, the server keeps the seat's report of the time away:** the ships and structures built and lost, the sectors gained and lost as the player comes back to them, a sector gained and lost again being in neither, and the last 16 scheduled orders that fired (ADR-080 decision 7).
- **The player's first snapshot after it takes the seat again carries the report, once.** A player's first join of a seat carries none, since the deputy played it from the universe's start for an empire the player never had (ADR-080 decision 7; ADR-079 decision 4).
- **The away window opens by itself when the report arrives:** how long the player was away, what was built and lost, the sectors gained and lost by name, and each order that fired, as its alert says it. Closing the window drops the report (ADR-080 decision 9; Game §interface.windows.orders).
- **The report is saved with the universe,** and so is which seats have been held, so a restart while the player is away loses none of it (ADR-080 decision 7; ADR-077 decision 5).
- **It tells what the seat's events tell:** what the deputy built and lost and the orders that fired, not what the enemy did beyond it (ADR-080).

### Saves and recovery [§universe.persistence]

- **A universe is a folder:** its settings, its saves, its command log and its universe log (ADR-077 decision 7; ADR-078 decision 2; ADR-082 decision 1).
- **Every 60 s of ticks the server saves the whole universe.** It encodes the state between two ticks, and a thread of its own writes the save to a new file and renames it into place, so a killed process never leaves half a save. The newest three saves are kept (ADR-077 decisions 3 and 7).
- **Every tick's commands are written to the universe's log before the tick applies them,** and handed to the system before the next tick runs (ADR-077 decision 7).
- **Stopping the server writes a last save** (ADR-078 decision 1).
- **On start, a universe comes back from its newest save that reads whole, and the commands logged since it,** applied tick by tick as fast as they go. It comes back at the tick after the last command it logged, and saves there at once (ADR-077 decision 8). A killed process so loses only the ticks after the last one that applied a command, never more than those since its last save, and a lost machine keeps only what the system had written (ADR-077).
- **On the same build, the universe that comes back is the universe that ran, bit for bit;** on another build that lays the universe out alike, it is a universe that follows the same orders (ADR-009 decision 1; ADR-077).
- **A universe pauses while its server is down.** It comes back at the tick it left off at, and does not run the hours it missed (ADR-077 decision 9; ADR-009 decision 4). A scheduled order whose time of day fell in the pause fires late, at its tick (ADR-080 decision 6).
- **A save loads only into the universe it came from,** made with the same tuning data and map files, and only in a build that lays the universe's state out as the build that wrote it did. Any other save is refused, and so is one cut short or altered; a folder whose saves all fail to read is refused, never started anew (ADR-077 decisions 5, 6 and 8).
- **The players the server hosts keep nothing in a save.** An AI empire made afresh after its server restarts plays on from its seat's snapshot, though not as it would have; a deputy starts its turn afresh (ADR-079 decisions 2 and 6; ADR-020 decision 16).

### The universe log [§universe.log]

- **The server writes a universe log beside the saves,** a line a record, every time in ticks: the universe's start and each recovery, each save and how long it held up the server's thread, each hand-over between a player and its deputy, each scheduled order fired, each loss, each wait for a start and each restart with how long it waited past its hour, every hour each seat's record, and each battle (ADR-082 decision 1).
- **A seat's hour** is its Ore, its income, its command points against its fleet cap, its nodes and its topics researched, with the seconds of the hour its player and its deputy played it (ADR-082 decision 3).
- **A battle** is an engagement between two seats in one sector, ended by 30 seconds without a shot there; a side was present when its player played the seat at any of its shots. Shots at or by the pirates are no battle (ADR-082 decision 2).
- **The log is not saved:** a recovery writes where the universe came back from, and a battle the server did not see begin is not recorded (ADR-082).

### Losing [§universe.loss]

- **A player whose base was placed loses when it has neither a Command Station nor a finished Shipyard.** The server checks it every tick. A Shipyard under construction keeps nobody in, nor do Constructors, Research Labs, Mining Rigs, Relays or Defence Platforms (ADR-037 decision 1).
- **A Command Station is never built:** a Constructor's order to build one is refused, and only a Command Station builds Constructors. A player that loses its station keeps the Constructors it has, and its Shipyards go on building warships (ADR-037 decision 2; Game §structures.station).
- **A player without a Command Station has its finished Shipyards revealed** to every other player, through fog of war, as remembered structures, so that the end of a match is not a search of the map for one building (ADR-037 decision 3; Game §sight.memory).
- **A player without a Command Station keeps every node and ship it has,** under level 1's node cap and fleet cap (ADR-056 decision 4; ADR-071 decision 1; Game §territory.cap; Game §economy.fleetcap).
- **In a match, a loss ends the match** (Game §session.end); **in a universe, the seat restarts** (Game §universe.restart).

### Restarting [§universe.restart]

- **A player who loses in a universe is told so in the tick it does.** Its banner says "Empire fallen", and under it "It restarts at your start in" and the time left, or "It restarts once your start is clear of the enemy" while it waits, with the way back to the menu (ADR-084 decisions 3 and 7). Neither the alerts nor the report of a time away tell it (ADR-084 decision 8).
- **An hour of ticks after it lost, its seat restarts at its own start, once the start is free** (ADR-084 decision 4), and at no other start (ADR-084).
- **A start is free while no other player holds its home sector, has a structure standing in it, or has a warship in it** (ADR-084 decision 4). A pirate's ship and a Constructor do not hold a start. While the start is not free, the seat waits, and it restarts in the first tick that finds the start free; a restarted base is not spared after that (ADR-084).
- **A restarted seat gets a Command Station on its start and the starting Constructors in front of it,** placed as at the universe's start, and its Ore is raised to the starting Ore when it has less ([Catalogue: rules](Reference/Catalogue.md#rules); ADR-084 decision 4).
- **It keeps everything else:** its research, its designs, its Ore above the starting Ore, and its ships and structures that still stand (ADR-084 decision 4). A rig in a sector it still holds goes on earning while it waits (ADR-084).
- **A player that finishes a Shipyard of its own while it waits stands again,** and does not restart (ADR-084 decision 4).
- **A lost seat saved while it waits restarts on the same tick** as in the universe that never stopped, since its start and the tick it lost are part of the universe's state (ADR-084 decisions 3 and 5).
- **An AI empire lays its plan afresh round its new Command Station; a deputy plays on** (ADR-084 decision 6; Game §ai.empire).

---

## Interface [§interface]

The game draws its own interface over the battlefield: a HUD anchored to the edges and corners of the screen, windows that float over it, alerts that come and go, and marks laid on the world itself. A match never waits for it; it runs on whatever is open (Game §session). What the menu offers, what the banner at a match's end says and what the tag in the top-right corner names are Game §session; what an order does is Game §orders; how the scene under the interface is drawn is Game §look. Every key the game reads is listed in [Controls](Reference/Controls.md), and every color the interface uses in [Colors](Reference/Colors.md).

### The screen [§interface.screen]

- **The game fills the primary monitor, borderless, and Alt+Enter toggles a window.** The first window has a 1280×720 inside, centered on its monitor; after that Alt+Enter goes back to the frame the window last had, until the game closes. A window that is full screen covers its monitor again when the display changes. Nothing about the window is kept between runs (ADR-006 decision 1).
- **Full screen, there is no close button;** Alt+F4 closes the game, as the menu's Quit does, and a window's close button does too (ADR-006).
- **The interface is laid out on a reference frame of 1920×1080 units and scaled as one to fit the screen.** Each element is anchored to a corner, an edge or the middle, and on a screen that is not 16:9 the whole frame still fits, the spare room going between the anchors (ADR-006 decision 5). The player can make it larger (Game §interface.scale).
- **Text is drawn at its final size in pixels**, so it is sharp at every size of screen and every scale (ADR-015 decision 3; ADR-030 decision 1).
- **The interface is drawn over the finished scene, and never glows** (ADR-050 decision 2; ADR-091 decision 1).
- **A press on the interface is the interface's.** A press on a panel of the HUD or on a window does nothing in the world; a release always reaches the world, so a drag begun there ends wherever it is let go. Windows are tested front to back, then the HUD, then the world (ADR-015 decision 5; ADR-031 decision 4).
- **The game needs its fonts installed:** Segoe UI, Bahnschrift, and Cascadia Mono or else Consolas, as Windows 11 has them. A face none of whose families is installed stops the game with a message that names it (ADR-030 decision 2).

### Scale [§interface.scale]

- **Ctrl+= makes the interface larger and Ctrl+- smaller**, on the keys that read = and - on a US keyboard, in steps of 100%, 110%, 125%, 150%, 175% and 200% of the size that fits the screen (ADR-070 decision 2).
- **A step up is taken only while the windows of a fixed size still fit the screen between its margins:** in a match, the designer laid out with every component of the match, locked or not, the Controls window, the orders window and the away window; on the menu, the Controls window alone. The production and research windows grow with what they hold, and are not counted (ADR-070 decision 3).
- **The scale comes down to the largest step that fits** whenever the screen changes size and whenever a match starts or ends, so no window of a fixed size opens taller than the screen (ADR-070 decision 4).
- **The scale lasts until the game closes, and starts at 100%.** Nothing is written to disk (ADR-070 decision 5).
- **What is drawn in the world keeps the screen's own scale,** whatever the player sets: the stars, the bars over ships and structures, and the selection's ring (ADR-070 decision 7; ADR-088 decision 5).

### The interface's look [§interface.style]

#### Faces and text [§interface.style.type]

- **Titles are in Bahnschrift bold, semi-condensed; labels and names in Bahnschrift semibold; figures in Cascadia Mono, or Consolas where Cascadia Mono is not installed** (ADR-030 decisions 1 and 2; ADR-062 decision 1).
- **Each face has one size, in reference units:** a title 22, a name 16, a label 13, a figure 14, a large figure 28 and a detail 13 (ADR-062 decisions 1 and 2).
- **Labels are set in small spaced capitals** (ADR-030 decision 4).
- **Text is printable ASCII, the middle dot · and the multiplication sign ×.** Any other character shows as `?` (ADR-030 decision 3). A minus is written as a hyphen (ADR-057 decision 8).
- **Every text stands at 4.5:1 or more against what it is drawn on** (ADR-062 decision 3).
- **A line that would run past what holds it is cut short with three dots;** a research topic's effect is broken between words to its card's width (ADR-061 decision 3).

#### Panels, cards and buttons [§interface.style.panels]

- **Every window and every panel of the HUD has an opaque dark navy body and a bracket at each corner;** a window adds a hatched title bar and a close box (ADR-031 decision 3; ADR-043 decision 1; ADR-068 decision 1).
- **A button is a card:** a card's face and edge, its label in the name face, and any cost as Ore's gem and figure at its right (ADR-043 decision 4).
- **A button that cannot be pressed is dim.** When the reason is not its cost it says why in place of the cost, such as ONE PER PLAYER, NODE CAP, NEEDS RESEARCH or UPGRADING · 41%; a button the player only cannot afford is dim with its cost (ADR-043 decision 4; ADR-056 decision 11; ADR-064 decision 7).
- **A button that gives an order or opens a window shows its key's cap,** A, H, T and S for the orders and P, D and R for production, the designer and research (ADR-088 decision 2; ADR-068 decision 4).
- **What the pointer is over lights:** an enabled button's edge, and a status line on the cards' color. Lines whose click does the same light together. Nothing else changes under the pointer: every word, place and click is the same (ADR-088 decision 1).

#### Colors [§interface.style.colors]

- **One color, one meaning.** Each player's is its set's, as the world draws it, whichever seat views it: player 1's blue and player 2's orange-red (Game §look.models.sets). The pirates' is violet and Ore's gold, so the minimap and the territory read without a legend (ADR-043 decision 7; ADR-073 decision 10; ADR-085).
- **Ore is written one way everywhere:** its gold cut gem and the figure grouped in thousands, in the stockpile, a window's Ore box, a card's cost and a button's cost (ADR-043 decision 3; ADR-085 decision 3).
- **A warning is a chip:** its words in the windows' navy on a filled tag of the warning's color. It marks an idle Research Lab or Shipyard that could start work now, an income of nothing, the newest alert, a drain that takes the player's tickets, and a server gone quiet (ADR-085 decision 1; ADR-066 decision 2; ADR-057 decision 8; ADR-086 decision 2). Nothing else stands on a filled tag of that color.
- **Two warnings are plain text in the warning's color,** since no enemy figure stands near them: a design name the designer refuses, in its own field, and the menu's notice (ADR-085 decision 1).
- **Every health bar fills green above half its hit points, amber above a quarter and red below:** the bars over ships and structures and the selection's bars alike (ADR-088 decision 5).
- **A bar of the HUD's own fills in its blue,** on the player's own buttons, queues and status lines; the build bar over any side's structure in the world is a neutral gray (ADR-085 decision 2; ADR-066 decision 3).

### The HUD [§interface.hud]

The HUD stays where it is and never moves with the camera (ADR-015 decision 4; ADR-043 decision 1):

- the Ore at the top left, and under it one column of the status, territory and alerts panels;
- the hint at the top middle, and the banner under its place once a match ends (Game §session.end);
- the tag that says where the match runs at the top right (Game §interface.hud.tag);
- the minimap at the bottom left (Game §interface.minimap);
- the selection at the bottom middle, with its buttons against its right edge.

#### The column under the Ore [§interface.hud.column]

- **The Ore, status, territory and alerts panels are one width,** wide enough for the widest status line and the longest research topic, and never narrower than the alerts need. What a panel holds never moves the column's edge; a line that will not fit is cut short (ADR-066 decision 3).
- **The status panel's place is kept for its lines whether it has them or not,** so the territory and the alerts under it stand still as a Research Lab or a Shipyard is finished (ADR-066 decision 3).

#### Ore [§interface.hud.ore]

- **The stockpile is written from the panel's left:** the gem stays put and the figure grows to its right (ADR-043 decision 3). It is whole Ore (Game §economy.ore).
- **The income a second stands at the panel's right edge,** after a plus sign and before "/s", to a tenth when it is not whole. An income of nothing is the chip "+0/s", since then nothing the player spends comes back (ADR-043 decision 3; ADR-085 decision 1).

#### Status [§interface.hud.status]

The status panel says what production and research are doing, a line each, and each line is a place to click (ADR-066 decision 1).

- **The Research Lab's line,** once the player has a finished Lab: the topic under way over a bar of how far it has come, how many more are queued at the bar's end, "+2", and "· waiting for Ore" while the front topic waits for its Ore. Idle, it reads "Research Lab IDLE", the IDLE a chip, while a topic is open to the Lab, and "Research: every open topic done" when none is. A topic is open while it is not researched, its tier is one the Lab has opened and every prerequisite is researched; its Ore is not asked. A click opens the research window (ADR-066 decisions 1 and 2; Game §interface.windows.research).
- **The Shipyards' line,** once the first is finished, says first whether the fleet cap holds them back: a Shipyard's front warship waits for the cap, or the fleet has room for none of the player's saved designs. Then it reads "Shipyards at the fleet cap", followed by "· Station L" and the next level with the points that level adds to the cap, or by "· Station upgrading," and how far that level has come while it is built, or by nothing at the station's top level, all in plain text. A click selects the Command Station and moves the camera to it, where its panel offers the upgrade (ADR-066 decisions 1 and 2; Game §economy.fleetcap).
- **Otherwise the Shipyards' line counts them,** "Shipyards · BUILDING 3 · WAITING 1 · IDLE 1". A Shipyard waits when its queue holds a job and the front job has made no progress. IDLE is a chip only when an idle Shipyard could start a ship now: a saved design of a hull it builds fits under the cap and the player has its Ore. A click opens the production window at the first idle Shipyard by number, or at the first Shipyard when none is idle (ADR-066 decisions 1 and 2).
- **The fleet's line,** under the Shipyards' while the match sets a cap: "Fleet" beside a bar of the command points the warships take against the cap, with the points and the cap at its end. Its click is the Shipyards' line's (ADR-066 decision 1).
- **The Command Station has no line here,** and a Lab or a Shipyard under construction has none either; their selection panels say what they do (ADR-066).
- **The status panel shows a standing state, and stays;** an alert is a message about an event, and fades (ADR-066 decision 5).

#### Territory [§interface.hud.territory]

On a map with sectors, the territory panel stands under the status panel's place, in up to three rows (ADR-057 decision 8; ADR-056 decision 11). What the rows count is Game §territory.domination.

- **"Nodes":** beside it, in the labels' color, the Command Station's node cap out of the map's nodes, "cap" and "of", or only "of" the map's nodes without a cap; and at its right the player's nodes against the enemy's, "3 : 2", the player's figure in its color and the enemy's in theirs. A sector the pirates guard counts for neither (ADR-057 decision 8; ADR-073 decision 10).
- **"Tickets":** both players' tickets, the same way (ADR-057 decision 8).
- **The drain:** who it takes tickets from, how many a minute and when they run out, "Enemy", the rate "a minute · out in" and the time, in the text's color; the same after "You" as a chip when it takes the player's; or "No drain" while the nodes are level (ADR-057 decision 8).
- **A universe has no tickets,** and so neither of the last two rows (ADR-057 decision 8; ADR-084 decision 2).

#### The selection panel [§interface.hud.selection]

- **It stands at the bottom middle, as wide as its longest line,** between 280 and 560 units. Its first line is in the title face and the others in the name face (ADR-043 decisions 2 and 5).
- **Ships:** one ship is titled with its design's name; ships of one design with their count and the design's name once, "7 × Lancer+SA", and two Constructors "2 × Constructor"; ships of several designs with their count, "12 ships", over a line for each design, "3 × Swarm", the most numerous first, at most five lines, and "and 2 more designs" for the rest (ADR-088 decision 4). Then the selection's hit points against their whole, "Hit points 3,599 / 4,095", in whole points rounded up (ADR-088 decision 4; ADR-014 decision 3).
- **Two to 24 ships have a bar each** under the lines, in rows of twelve, design by design in the lines' order. Each fills from the left by its ship's share of hit points; a retreating ship's bar has a line round it in the text's color; a click on a bar selects that ship alone and centers the camera on it. One ship, or more than 24, has the selection's one bar (ADR-088 decision 4).
- **What the ships are doing:** "Holding a sector" or "On patrol" for a standing order, how many are retreating to be repaired, and the scheduled order they wait on (ADR-059 decision 6; ADR-075 decision 10; ADR-080 decision 9; Game §orders.standing).
- **A structure selected alone,** the player's own or another player's (Game §orders.selection), is named by its kind, a Shipyard by its number too (Game §structures.shipyard), and a kind that grows by its level, "Shipyard 01 · L2". Under it: how far it is built while under construction, "Upgrading to L3, 41%" while a level is built, and its hit points against their whole (ADR-064 decision 7).
- **Another player's structure that the player only remembers** says how long ago it was seen, "Last seen 4:12 ago", and what follows as it was then: "34% built when seen", "Hit points 1,200 / 3,000 when seen". Another player's structure offers no button and says nothing of its queue (ADR-081 decision 4; ADR-066 decision 4).
- **A Mining Rig's panel gives its asteroid's Ore left,** as far as the player knows it, or "Ore run out: it earns a trickle" (ADR-035 decision 5; Game §economy.reserves).
- **A Relay's or a Mining Rig's panel says what its sector earns:** "North: held", "suppressed, it earns nothing", "cut off, it earns half" or "not held, it earns nothing" (ADR-056 decision 11; Game §territory.gives).
- **The player's own finished Command Station, Shipyard or Research Lab says what it is doing:** "Building Swarm · 62% · +2 queued", "Building Swarm · waiting for Ore", "· waiting for the fleet cap", "Researching Hull Plating · 62%", or "Idle" (ADR-066 decision 4).
- **A structure that grows says what its next level gives, and offers its Upgrade** (ADR-064 decision 7; Game §structures.levels.panel).

#### The buttons [§interface.hud.buttons]

- **The buttons' panel stands against the selection panel's right edge,** bottom-aligned, and the pair is centered at the bottom so that the two read as one. It moves right only where it would meet the minimap; with no selection beside it, it stands alone at the middle (ADR-088 decision 2).
- **A selection with a warship offers its orders first:** Attack-move, Hold sector, Patrol and Stop, Hold sector only on a map with sectors. The order armed for the next left-click is lit as a pick until it is given or canceled (ADR-088 decision 2; Game §orders.direct).
- **A selection with a Constructor offers a button for each structure it builds,** with its cost; the Relay only on a map with sectors. The Research Lab's is dim with ONE PER PLAYER once the player has one, and the Relay's with NODE CAP at the Command Station's cap (ADR-043 decision 4; ADR-056 decision 11; Game §orders.constructors).
- **An order folds the Constructors' build buttons into one Build button** (owner, 2026-10-25), since a structure is picked once for each order, where a warship's orders are given again and again to the same ships. Any order given to the selected ships folds them: a placement, a move from the world or the minimap, an attack, a repair or build, a salvage, an attack-move, a hold, a patrol, a stop or a scheduled order. A placement with Shift keeps them open, as it keeps the placement armed; setting the retreat and canceling a placement fold nothing. Build opens them again, and so does selecting again, the same ships included. The selection stays, and the warships' orders and the retreat stand as they were. Build has no key (ADR-088 decision 2).
- **The player's own finished structure offers its windows:** Production on the Command Station and on a Shipyard, Ship designer on a Shipyard, and Research on the Research Lab, and its Upgrade when it grows (ADR-031 decision 6; ADR-064 decision 7).
- **A selection of ships ends with its retreat,** a row of three under the buttons (ADR-088 decision 3; Game §orders.retreat.threshold).

#### The hint and the pointer [§interface.hud.hint]

- **While a structure is placed, a hint at the top middle says what a click does:** "Placing Shipyard: left-click to build, right-click to cancel", or, for a Relay at the node cap, "Relay: at the node cap; upgrade the Command Station to claim more" (ADR-015 decision 4; ADR-056 decision 11; Game §orders.constructors).
- **With the pointer over a derelict, the hint says what it holds:** its Ore, the topic whose research it recovers part of, how far its salvage has come, and how long ago it was seen when it is only remembered (ADR-074 decision 10; ADR-081 decision 4; Game §neutrals.derelicts).
- **With the pointer over a structure the player only remembers, the hint says what it is and how long ago it was seen:** "Shipyard · last seen 4:12 ago, 34% built". Without the server's tick rate the age reads "last seen earlier" (ADR-081 decision 4).
- **A placement's hint comes before the pointer's** (ADR-081 decision 4).

#### The tag [§interface.hud.tag]

- **The tag in the top-right corner says where the match runs,** as Game §session.skirmish and Game §session.join have it. It is as wide as its words, 16 units in from the screen's right edge; an address too long for it is cut short, and while it warns it is as wide as its widest warning, so its edge stands still as the seconds count (ADR-086 decisions 1 and 2).
- **It takes no click, and the designer covers it** while it is open (ADR-086 decisions 1 and 3).

### The minimap [§interface.minimap]

- **The minimap is a square of 300 units at the bottom left:** the map with +x to the right and +z up, as the camera first shows it (ADR-043 decision 8; ADR-015 decision 4).
- **A left press or drag on it moves the camera there; a right-click sends the selected ships there** (ADR-013 decision 5; Game §orders.direct).
- **Every entity is a mark at its size, or at a least size:** a ship a small square, a structure a larger one, in its side's color: its player's set's, as the world draws it, or the pirates' violet. The neutral marks are drawn first, so a rig's mark shows over its asteroid's (ADR-015 decision 4; ADR-043 decision 7; ADR-073 decision 10).
- **An ore asteroid is an outlined square in a darkened gold,** at least 8 units across, so it differs from an enemy's filled mark in shape as well as in color. One that has run dry is outlined in rust. An asteroid field is a dark gray square, darker than ore, and a derelict a light gray one (ADR-043 decisions 7 and 8; ADR-035 decision 5; ADR-074 decision 10).
- **The fog lies over the marks,** dark where the player has never seen and dimmed where it has seen before, as Game §sight.drawn has it, with the explored ground lighter here than on the ground of the world (ADR-052 decision 4; ADR-081 decision 6).
- **The territory is drawn over the fog, since every player sees it:** every sector's border, and each held or guarded sector washed faintly in its holder's color under the marks and outlined in it over the fog, its outline dashed while it is cut off, and the whole sector hatched in its holder's color while it is suppressed. While the player places a Relay, each sector whose node it could claim now is outlined inside its border in the player's own color. No node has a mark (ADR-081 decision 7; Game §territory.shown).
- **What the player only remembers is a cross in its side's color over the fog,** a derelict's in the derelicts' gray (ADR-081 decision 4).
- **Each alert's place is an outlined square in the warning's color,** over everything else (ADR-059 decision 2).
- **The camera's view is the ground the screen's four corners show, joined in turn,** so it shows the near side and the far and turns with the camera; what runs past the map is cut at the minimap's edge (ADR-015 decision 4).

### Marks in the world [§interface.marks]

- **The selection is a ring round each selected ship or structure,** 1.3 times its footprint, 3 reference units wide at any zoom at the screen's own scale: green, or amber while attack-move or a standing order waits for its click (ADR-067 decisions 1 and 2). A Mining Rig's selection ring is its asteroid's, since a click anywhere on the rock selects the rig (ADR-042 decision 4).
- **A drag box is its outline on the ground,** covering the ground the box on the screen covers (ADR-013 decision 5).
- **Every structure stands on a faint ring,** a line a pixel wide at 1.3 times its footprint in its side's color, which puts it on the ground and says its size (ADR-042 decision 1).
  - **The ring is for the far view.** It is at its resting strength, faint in its side's color, while its radius is at most 3% of the view's width, and gone from 6.5%, so up close the bases are not ringed (ADR-042 decision 1).
  - **It is at full strength, in its side's ship-line color, a little stronger in hue than the structure's own lines, under the pointer and under every structure while one is placed** (ADR-042 decision 2).
  - **A selected structure's selection ring takes its place** (ADR-042 decision 3).
  - **A Mining Rig's ring is its own footprint's, laid over its rock,** and shows only under the pointer and while a structure is placed, at full strength (ADR-042 decision 4).
  - **A structure the player only remembers stands on no ring** (ADR-081 decision 4).
- **Bars stand over ships and structures on the screen** (ADR-088 decision 5):
  - a health bar over one that is hurt, and over every ship and structure while Alt is held;
  - a build bar under it while a structure is unfinished, in a gray that is no side's (ADR-085 decision 2);
  - each 32 by 5 units at every zoom, centered over the top of its model on the screen however the camera has turned, filling from the left;
  - each on a back of its side's color, darkened, so a bar says whose it is;
  - under every panel and window, taking no click; none over what the player only remembers (ADR-081 decision 4).
- **A structure being placed is shown where it would stand,** its model and a band on the ground green where it may stand and red where not (ADR-013 decision 5; ADR-067 decision 4; Game §orders.constructors).
- **The territory is drawn over the fog,** since every player sees it (ADR-081 decision 3; Game §territory.shown):
  - every side of every sector once, a line a pixel wide, brighter than the grid and never wider;
  - inside each held or guarded sector, an outline in its holder's color at half its strength, a little in from the border, so that where two holders' sectors meet both outlines show;
  - a suppressed sector's outline dashed and a cut-off one's dotted, one both suppressed and cut off dashed; a guarded sector's in the pirates' color;
  - while the player places a Relay, two rings on each node it could claim now, in the color of its own lines, the outer the size of a Relay's footprint ring, so the Relay's ghost stands in them, and no mark on any other node (ADR-081 decision 3). Whether a node can be claimed is the one rule the Relay's ghost and the minimap use too (ADR-081 decision 2).
- **What the player only remembers is drawn as a memory over the fog** (Game §sight.drawn).

### Alerts [§interface.alerts]

- **Alerts are read from the events the server raises for the player** (ADR-059 decision 1; ADR-080 decision 2). Each names its sector:
  - "Relay suppressed: North" and "Relay under attack: North";
  - "Mining Rig lost: North";
  - "Enemy ships in North" or "Pirate ships in North", when an enemy or pirate warship is first seen in a sector the player holds;
  - "Ship retreating: North", when one of the player's ships starts back to be repaired;
  - "Pirates cleared: North", when a sector the pirates guarded is guarded no more;
  - "Order fired:", "Order held back:" or "Order refused:" and the scheduled order's action (Game §orders.scheduled.firing).
  - A place in no sector is "the field".
- **A command the server refuses is told as an alert, in the player's words,** to the player who gave it alone: "Not enough Ore", "Not in a held sector", "At the node cap: upgrade the Command Station". It names no sector and has no place. A scheduled order refused when it is given is told the same way (ADR-094).
- **One kind of alert in one sector is not raised again within 20 seconds;** a scheduled order is told every time it fires, and a refusal told again takes the older one's place at once (ADR-059 decision 1; ADR-094 decision 4).
- **Nothing else raises an alert:** what the player builds, the ships and structures it loses other than a Mining Rig, and the sectors it gains and loses go to the report of a time away, and an empire's fall to the banner (ADR-059 decision 1; ADR-084 decision 8; Game §universe.away).
- **The HUD lists the alerts of the last 8 seconds under the territory panel,** at most four, newest first, the newest a chip (ADR-059 decision 2; ADR-085 decision 1).
- **Each alert's place is marked on the minimap** over the fog; a refusal has none (ADR-059 decision 2; ADR-094 decision 4).
- **Space moves the camera to the newest alert that has a place,** whether or not it is still listed (ADR-059 decision 2; ADR-094 decision 4).

### Windows [§interface.windows]

#### How a window behaves [§interface.windows.behavior]

- **The designer, the production window, the research window, the Controls window, the orders window, the away window and the leave window float over the HUD** (ADR-031 decision 6). The HUD stays put under them (ADR-043 decision 1).
- **D, P, R, F1 and O open and close the designer, production, research, Controls and orders windows** in a match, and the buttons on a structure's panel open its windows. The away window opens by itself when the report of a time away arrives, and Esc opens the leave window in a universe (ADR-031 decision 6; Game §universe.away; Game §interface.windows.leave).
- **A window that shows a structure opens on the one selected.** Selecting another Shipyard while the designer is open makes it the designer's target, and selecting another producer while the production window is open shows it there (ADR-031 decision 6).
- **A window is dragged by its title bar** and kept on the screen: its whole title bar, and 120 units of its width. A smaller screen moves it back as well (ADR-031 decision 2).
- **A press on a window brings it to the front;** its × closes it, and Esc closes the front window and goes no further. With no window open, Esc reaches the controls, and in a universe, with nothing armed, opens the leave window; while the designer's name takes typing, the designer has Esc first (ADR-031 decision 5; ADR-017 decision 8).
- **Several windows can be open at once,** each drawn whole over the ones behind it (ADR-031 decision 4).
- **A window opens where it was left,** until the game closes: closing and opening it, or starting another match, puts it back there. Leaving a match closes every window. Nothing is written to disk (ADR-031 decisions 1 and 2).
- **Where a window first opens:** the designer at the top-right corner; the production and research windows side by side under the column's place, each in the first of the two places no open window holds, clear of the designer at 100%; the orders window at the right, at the same height; the Controls, away and leave windows in the middle of the screen (ADR-031 decision 2; ADR-062 decision 6).
- **A window has the size its content gives it;** none is resized, docked or minimized (ADR-031).

#### The production window [§interface.windows.production]

- **It shows one producer:** the Command Station, or a Shipyard by its number, between arrows that step through the player's finished producers, the Command Station first and then the Shipyards by number, "< SHIPYARD 05 >", and the player's Ore (ADR-068 decision 1; Game §structures.shipyard).
- **Its cards come first, two to a row,** and stay where they are as the queue grows, so one can be clicked again and again: the Command Station's Constructor, or each of the player's saved designs at a Shipyard (ADR-043 decision 6).
- **A design's card gives its name, its code and its cost, and a third line:** the build time at the player's Shipyard speed, and a short bar for each hull, marked S, M and L, of three segments lit as the designer rates the design against that hull, three for Good, two for Fair and one for Poor. The Constructor's card gives its build time. A design whose hull is above the Shipyard's level is dim, its code followed by "NEEDS L2" (ADR-068 decision 3; ADR-064 decision 9).
- **The queue stands under the cards,** a row for each job and no empty ones, under "QUEUE · 2 / 5": the front job's progress as a bar and a percentage, or "WAITING FOR ORE", or "WAITING FOR FLEET CAP". The window grows at its foot (ADR-043 decision 6; ADR-068 decision 1; Game §economy.fleetcap).
- **It is 478 units wide,** so that it, the research window and the designer stand side by side at the reference width (ADR-062 decision 6).

#### The research window [§interface.windows.research]

- **It shows the player's Research Lab and its Ore,** a card for each topic not researched or queued, ten to a page in two columns, tier by tier, under where the page stands, "TOPICS · 11-20 OF" and the number of topics, with arrows that scroll a row at a time (ADR-043 decision 6; ADR-068 decision 1; ADR-017 decision 9).
- **A topic's card gives its name, its cost and its effect, and then "TIER", its tier and its time,** with SALVAGED once a derelict has recovered part of it (ADR-074 decision 10; Game §neutrals.salvage).
- **A topic that cannot be queued yet is hatched and dimmed,** each thing it needs on a line of its own with its box: "NEEDS · RESEARCH LAB L2", then "+ HULL PLATING". Every card is as tall as the most lines any shown topic needs, so the cards keep their places as the page scrolls (ADR-061 decision 4; ADR-064 decision 10).
- **The queue stands under the cards, as production's does,** a second topic running beside the first showing its progress too (ADR-043 decision 6; ADR-064 decision 10).
- **It is 560 units wide** (ADR-062 decision 6).

#### The ship designer [§interface.windows.designer]

- **The designer is a window of the owner's mockup:** its title bar names the Shipyard its queue goes to between arrows, "< SHIPYARD 05 > · DESIGNER", over a header with that Shipyard's queue, "QUEUE · 2 / 5", over five slots, the ships it has built, "BUILT · 4", and the player's Ore (ADR-017 decision 7; ADR-068 decision 1).
- **Under the header it holds, top to bottom:** the name with its count of characters and Save; the saved designs as chips, three at a time with arrows; a row for each slot, hull, drive, weapon and module, of cards three to a line, a locked one hatched with the topic that unlocks it; the performance bars beside the damage per second after armor against each hull; and Rename, the design's retreat, the count of ships, and Queue with one ship's build time and the cost of all (ADR-017 decision 7; ADR-062 decision 4; ADR-088 decision 3).
- **What a design is, and what saving, renaming, queuing and typing a name do, are Game §ships.designer.**
- **It is 818 units wide,** and as tall as its rows make it (ADR-062 decision 4).

#### The Controls window [§interface.windows.controls]

- **F1 opens and closes it,** in a match, in the middle of the screen (ADR-068 decision 4).
- **It lists every key and mouse action the game reads in a match,** under three headings: selecting and orders, the camera, and the windows. Its lines are made from the keys the input code reads, so it cannot name a key the game does not read ([Controls](Reference/Controls.md); ADR-068 decision 4).
- **The keys that edit a design's name are not listed:** that is typing, not a control of the match (ADR-068).

#### The orders and away windows [§interface.windows.orders]

- **O opens the orders window,** which lists the seat's scheduled orders and gives the selection a new one; it is Game §orders.scheduled.window (ADR-080 decision 9).
- **The away window opens by itself when the report of a time away arrives,** and closing it drops the report; it is Game §universe.away (ADR-080 decision 9).

#### The leave window [§interface.windows.leave]

- **In a universe, Esc opens the leave window** once no window is open and nothing is armed: "Leave for the menu?", that the deputy keeps the empire going, and Leave universe and Stay side by side. Leave universe goes back to the menu, and Stay closes the window, as its × and Esc do; it is Game §session.join (ADR-078 decision 11).
- **A skirmish has none:** its way back to the menu is the banner's once the match ends (ADR-020 decision 9; Game §session.end).

### The camera [§interface.camera]

- **The camera looks down on the battlefield from an angle set by its zoom.** The zoom is the width of ground across the middle of the screen at the focus, the same on any shape of screen, and the camera looks down more steeply as it zooms out ([Catalogue: defaultViewWidthMeters](Reference/Catalogue.md#defaultViewWidthMeters), [Catalogue: minimumViewWidthMeters](Reference/Catalogue.md#minimumViewWidthMeters), [Catalogue: maximumViewWidthMeters](Reference/Catalogue.md#maximumViewWidthMeters), [Catalogue: pitchAtMinimumDegrees](Reference/Catalogue.md#pitchAtMinimumDegrees), [Catalogue: pitchAtMaximumDegrees](Reference/Catalogue.md#pitchAtMaximumDegrees); ADR-012 decisions 1 to 3).
- **A match opens on the player's own fleet,** the camera looking along +z, so the screen's right is +x and its top +z (ADR-012 decision 4).
- **The arrow keys pan, and so does the screen's edge and a middle-drag;** the pan is in view widths a second, so it feels the same at every zoom ([Catalogue: panViewWidthsPerSecond](Reference/Catalogue.md#panViewWidthsPerSecond), [Catalogue: edgeScrollMarginPixels](Reference/Catalogue.md#edgeScrollMarginPixels); ADR-012 decisions 3, 6 and 8). A middle-drag holds the ground under the pointer, and keeps going when the pointer leaves a window (ADR-012 decision 6).
- **The wheel zooms, a step a notch** ([Catalogue: zoomFactorPerNotch](Reference/Catalogue.md#zoomFactorPerNotch); ADR-012 decision 3).
- **Q and E turn the camera round its focus,** Q counterclockwise seen from above ([Catalogue: rotateDegreesPerSecond](Reference/Catalogue.md#rotateDegreesPerSecond); ADR-012 decision 3).
- **The focus stays over the map** ([Catalogue: focusLimitMeters](Reference/Catalogue.md#focusLimitMeters); ADR-012 decision 3).
- **While the game is full screen and in front, the pointer is held inside it,** and only then does the screen's edge pan, so it pans beside another monitor too. Alt+Tab lets the pointer go, and coming back holds it again (ADR-012 decisions 5 and 6).
- **The camera moves exactly with its input,** with no inertia or smoothing, and reads no input while the game is in the background (ADR-012 decision 7).
- **The minimap, Space and a click on a bar or a status line also move the camera** (Game §interface.minimap; Game §interface.alerts; ADR-088 decision 4; ADR-066 decision 1).

### Controls [§interface.controls]

- **Every key the game reads in a match is named in one place,** so a button's cap, the Controls window and the input code cannot disagree ([Controls](Reference/Controls.md); ADR-068 decision 4).
- **Selecting and ordering** are Game §orders.selection and Game §orders.direct: click, drag, Shift, double-click, the control groups, right-click, A, H, T and S (ADR-013 decision 5; ADR-059 decision 7).
- **The camera:** the arrow keys, Q and E, the wheel and a middle-drag (Game §interface.camera).
- **The windows:** D, P, R, O and F1 (Game §interface.windows.behavior).
- **Esc closes the front window,** with none open cancels what the controls have armed, and in a universe, with nothing armed, opens the leave window (ADR-031 decision 5; ADR-078 decision 11; Game §orders.direct).
- **Space moves the camera to the newest alert** (ADR-059 decision 2).
- **Alt, held, shows a bar over every ship and structure.** The game swallows the window's menu key, so Alt alone never stops the game in a menu; Alt+F4 and Alt+Enter still work (ADR-088 decision 5; ADR-006 decision 1).
- **Ctrl+= and Ctrl+- scale the interface** (Game §interface.scale).
- **The keys cannot be changed** (Game §limits).

---

## Look [§look]

The game is drawn in the vector look of the eighties: low-poly models with dark faces and bright lines on their ridges, over a black sky of stars and a faint grid, lit warm from above and cool from below, with a phosphor's glow round what is bright. The simulation has no height and no light; everything here is the client's presentation of what the server sends, and none of it changes a rule (ADR-002; ADR-029 decision 2). The interface drawn over the scene is Game §interface; the fog that darkens it is Game §sight.

### The scene [§look.scene]

- **Everything stands on one flat plane, drawn in 3D under a tilted camera.** Ships turn and move on the plane; nothing in the scene has a height the rules know of (Game §map.plane; Game §interface.camera).
- **The scene is drawn at the screen's own resolution,** four samples a pixel, so every line and silhouette is smoothed; there is no lower render resolution and no HDR (ADR-006 decision 4; ADR-040 decision 3).
- **A frame is drawn in this order:** the sky; the grid; every model's faces and the shards of what is breaking up; every model's lines; the structures' footprint rings, the selection and the placement ghost; the shots; the exhaust and the blasts' particles; the fog; and, over the fog, the territory and what the player only remembers. The glow is added as the frame is finished, and the interface is drawn over it (ADR-022 decision 5; ADR-027 decision 3; ADR-024 decision 9; ADR-081 decision 5; ADR-091 decision 1).
- **The menu stands over the sky and the empty grid** (ADR-022 decision 5; Game §session.menu).
- **What is drawn follows the view of the simulation one tick behind the newest snapshot,** so effects, explosions and spinning parts keep the match's time, whatever the frame rate (ADR-013 decision 3; ADR-026 decision 1; ADR-045 decision 4).

### The sky [§look.sky]

- **The sky is an even field of 50,000 stars over the whole sphere, on black,** with no galaxy, band, nebula or other diffuse light. It is the same in every match and every run (ADR-022 decision 1 and what it forecloses).
- **Its brightness is the natural one:** most stars are near the faintest, few are bright and very few very bright, each tinted between the colors of a 3,000 K and a 10,000 K star, and drawn as a soft point a little over a pixel wide that does not flicker as the sky turns (ADR-022 decision 2).
- **The brightest 25 are four-point crosses,** 6 to 16 pixels in radius, each a line a pixel wide along the screen's axes, upright whichever way the camera turns, and white or a hot star's blue-white, never warm, so no cross reads as a marker in Ore's gold (ADR-028 decisions 2 and 3).
- **The sky is at infinity:** it turns with the camera and never moves as the view pans, and it is sized on the reference frame, so it looks the same on every screen, whatever scale the player sets for the interface (ADR-022 decision 5; ADR-070 decision 7).
- **Stars do not twinkle or move** (ADR-022).

### The grid [§look.grid]

- **A grid of lines every 100 m covers the map,** each a pixel wide at any zoom, the weight of the models' ridges, in a dim blue-gray that is barely there (ADR-028 decision 1).
- **The grid is what shows the ground moving as the view pans, and where the map ends,** since the sky does not move (ADR-028 decision 1; ADR-022 decision 5).
- **Nothing marks a larger step of the grid,** and every line is alike (ADR-028 decision 1).
- **The sectors' borders are lines of their own over the fog,** brighter than the grid and never wider (ADR-081 decision 3; Game §interface.marks).

### Models [§look.models]

#### The sets [§look.models.sets]

- **Each side has a set of the same nine models:** the Small, Medium and Large hulls, the Constructor, and the Command Station, the Shipyard, the Research Lab, the Mining Rig and the Defence Platform. They are the owner's own low-poly models, one mesh each, and the Command Station, the Shipyard and the Research Lab have one for each of their five levels (ADR-018 decision 8; ADR-027 decision 1; ADR-045 decision 1).
- **Player 1 is drawn with the Human set in blue, player 2 with the Tarkan set in orange-red,** so the two sides are told apart by shape and by color (ADR-011 decision 3; ADR-013 decision 1).
- **The pirates borrow the Tarkan set's models, in violet** (ADR-011 decision 2; ADR-073 decision 10; Game §neutrals.pirates).
- **A derelict is drawn with its hull's model from the Human set, in a light gray of no side,** lying across its radius (ADR-011 decision 3; ADR-074 decision 10; Game §neutrals.derelicts).
- **A Relay is drawn with the Research Lab's model, brighter, and a Repair Bay with the Shipyard's, darker,** since neither has a model of its own (ADR-056 decision 11; ADR-075 decision 10).
- **No color of a side is a drive's, an effect's or a mark's:** the exhaust, the blasts and the HUD's warnings are in colors of their own (ADR-019 decision 3; ADR-026 decision 8; ADR-085).

#### Size and pose [§look.models.size]

- **A hull is drawn at its model's length, which follows its footprint** (ADR-011 decision 2). Every model's front is in its mesh, and it faces in the game as it does in Blender (ADR-018 decision 3; ADR-011 decision 2).
- **A structure is drawn across its kind's footprint, and every level within level 1's ground:** a higher level shows as more detail, never as more size (ADR-011 decision 2; ADR-064 decision 8; ADR-045 decision 1).
- **A structure is drawn at its level,** and a remembered one at the level it was last seen at (ADR-064 decisions 6 and 8; Game §sight.memory).
- **A structure stands back from the ships:** its side's color is taken toward gray and its faces are darker, so the ships are the brightest and most saturated things of their side's color (ADR-040 decision 4).
- **A structure under construction is drawn darker, and brightens as it is built** (ADR-040 decision 1).
- **The Research Lab's radar and rings spin,** each part about its own axis at its own steady rate, every Lab and every Relay in step with the others; a placement ghost spins too (ADR-045 decisions 2 and 4).
- **A model's faces take one of three paints,** by their materials' names in its source: team, its side's color; trim, the gray as light as that color; and light, a warm white that no light reaches, so that it glows. A structure under construction keeps its lights lit (ADR-095 decisions 1 and 4).

#### Rocks and fields [§look.models.rocks]

- **An asteroid is one of three low-poly rocks,** the small one for the smallest asteroids, the large one for the largest and the medium between, so their facets come out about the same size on the screen (ADR-027 decision 1).
- **A rock is drawn in gray, its faces darker than a ship's,** so the terrain stands back from the fleets; a field's rocks are darker than an ore asteroid (ADR-040 decision 1).
- **An asteroid field is drawn as a cluster of rocks laid out from its identifier:** a rock in the middle and a ring of five to eight round it, each turned its own way, so no two fields look alike and one field looks the same in every frame (ADR-087 decision 1).
- **Smaller rocks close the ring,** so no gap the eye sees is wider than the smallest hull's footprint. The field blocks its whole circle whatever the rocks show (ADR-087 decisions 2 and 3; Game §map.fields).
- **A Mining Rig stands on its legs on its rock,** tilted to fit the rock under its feet and lifted until no foot is in it, its drill going into the rock (ADR-044 decisions 1 to 4).

### Lines over faces [§look.lines]

- **Every model shows its ridges as lines a pixel wide:** each edge where its surface folds by more than 10°, and each edge with one face. A line is a pixel at any distance, so its weight never changes with the zoom (ADR-027 decisions 2 and 3).
- **The faces are dark and the lines bright, in the model's own hue:** a ship's line is its side's color made brighter and taken a little toward white, a light blue for the Human set and a light red for the Tarkan set, so the lines carry the shape and the faces stand behind them (ADR-040 decision 1).
- **A rock's lines are brighter than its faces but never outshine the fleets' lines** (ADR-040 decision 1).
- **A line is drawn a little toward the eye,** so it shows over the face it lies on and never stands off a silhouette (ADR-040 decision 2).
- **A line never hides another line** (ADR-027 decision 3).
- **A line takes its faces' paint:** a team line in its side's line color, a trim line in the trim's, and a light line in the light's; a line between two paints is team (ADR-095 decision 5).
- **An explosion's shards take the faces' color** (ADR-040 decision 4; Game §look.explosions).

### Light [§look.light]

- **A face is lit by a warm key from above and behind the default view's top-left, and a cool fill** that runs from a dim violet on a face that looks down to a cool blue on one that looks up. The edge between light and shade is soft (ADR-090 decision 1).
- **The light that reaches an open face is never below the vector look's floor, and occlusion only darkens a face,** so every model's lines stand over its faces wherever the camera looks (ADR-090 decision 3; ADR-092 decision 4). A face that looks straight up is lit nearly gray, so a mark that lies flat on the ground keeps its color (ADR-090 decision 3).
- **A recess is darker than what stands out:** each vertex carries how open it is to the light, baked from its model's own triangles, and a face is darkened by it, down to black where it is shut in wholly (ADR-092 decisions 1 and 4).
- **A line takes only the brightness of the light, never its color or the recess's darkness,** so a ridge always says whose a model is, and the grid, the borders and the rings keep their colors (ADR-090 decision 2; ADR-092 decision 4).
- **Nothing casts a shadow or darkens another entity:** a rig is not darkened by its rock, nor a ship by the ship beside it (ADR-092).

### Glow [§look.glow]

- **What is bright glows, as a vector screen's phosphor does:** the lines on a model's lit side, a model's lights, the selection's ring, the exhaust, the shots, the blasts and the brightest stars. The faces, the grid and the borders stay under it, and a rock's lines glow less than a ship's (ADR-091 decisions 1 and 3).
- **The glow stays close to what is bright,** brightening the faces a few pixels from a lit line and fading out beyond (ADR-091 decision 3).
- **The interface never glows** (ADR-091 decision 1).

### Banking [§look.banking]

- **Ships lean into their turns,** in proportion to how hard they turn: their speed times how fast their heading turns. A ship flying straight is level, and so is one turning on the spot (ADR-029 decision 1).
- **Each hull and the Constructor have their own lean and how fast they reach it,** so a Small hull leans hard and quickly and a Large one slowly, and every ship leans fully in a sharp turn on any drive (ADR-029 decision 4).
- **The lean rolls in and out smoothly,** the same at any frame rate, and a ship that leaves the view comes back into it level (ADR-029 decisions 2 and 3).
- **Everything on the hull rolls with it:** its guns, its exhaust, its lines and, if it is destroyed mid-turn, its shards. Its footprint, its selection ring and what a click picks stay flat on the plane, and its bars stay on the screen (ADR-029 decision 5; ADR-088 decision 5).
- **Ships never pitch:** the battlefield is flat (ADR-029).
- **The lean is the client's alone,** and the simulation does not know it (ADR-029). How a ship turns is Game §orders.movement.

### Exhaust [§look.exhaust]

- **Every ship's exhaust glows in its drive's color,** so a design's drive reads on sight: Ion cyan, Fusion magenta, Pulse a lime green, and the Constructor, which has no drive, a pale gray (ADR-019 decision 3).
- **Each nozzle shows a core and a plume of three puffs streaming behind it,** which grow longer and brighter as the ship goes faster, up to about an Ion Medium's cruise (ADR-019 decision 2).
- **An exhaust is light:** it adds to what is behind it, and a hull hides the part of it that lies behind the hull (ADR-019 decision 1).
- **An enemy's exhaust shows its drive as well,** since every snapshot carries each warship's drive (ADR-019 decision 4).
- **A model may carry beacons, lights that blink in its side's line color,** a fifth of a second in every 1.6 seconds, each entity at its own moment, so that a building says whose it is. A structure being built shows none, nor does one remembered (ADR-018 decision 9).

### Shots [§look.shots]

- **A shot leaves from its shooter's gun nearest its target,** where the view draws the ship in that frame, so a beam stays on a ship that moves while it fires. The Command Station spreads its guns' shots over the guns on its model (ADR-019 decision 5; ADR-064 decision 11).
- **The effects stand above the ships** they belong to, so no hull hides them (ADR-019 decision 5).
- **A shot starts with a flash at its gun** (ADR-034 decision 2).
- **Each weapon fires one of three looks** (ADR-034 decisions 1, 2 and 4):
  - **the Lance a beam,** joining the gun and the target at once in its shooter's side's color taken toward white, narrowing as it fades within a quarter of a second;
  - **the Rail Cannon a slug,** a thin white line that joins the gun and the target at once, whoever fires it, and fades where it stands, with a wide flash at both ends;
  - **every other weapon a tracer,** a short yellow streak that runs from the gun to the target.
- **A tracer lands as a spark,** and a weapon with a splash also throws an amber ring that runs out to its splash, so the player sees what it reached (ADR-034 decision 2; Game §combat.splash).
- **A shot is drawn only where the player may see it** (ADR-024 decision 6; Game §sight.seen).

### Explosions [§look.explosions]

- **A ship or a structure that is destroyed breaks into its own triangles,** placed and colored as it was drawn. The shards fly out from its middle, slowed by drag, each tumbling, and darken to black over three seconds, shrinking to nothing at the end. A structure breaks three times over and a ship once, and one explosion keeps at most 400 shards (ADR-026 decision 4).
- **A blast of particles goes with it,** reaching one and a half times its footprint's radius: a fireball born near white and cooling to a yellow-amber, debris in amber leaving gray smoke behind it, and, for a structure, a fast flash besides. The particles are diamonds of a hard-edged texture, and keep full brightness for most of their life before they fade (ADR-026 decisions 1 to 3 and 8).
- **No blast is in either side's color,** so a ship blowing up in its own fleet never reads as the enemy's (ADR-026 decision 8).
- **One destruction always looks the same,** and plays from where the server says it died (ADR-026 decisions 6 and 7).
- **A frame too busy for every shard drops the oldest,** and one too busy for every particle drops particles, never exhaust (ADR-026 decisions 5 and 6).

---

## Limits [§limits]

What the game does not do, at the head of `main`. A rule that a chapter already states as an absence is that chapter's, and is only pointed to here. What the game may become is the [Horizon](Horizon.md)'s, which decides nothing.

- **Play.**
  - There is no campaign and no story: no screen names a faction, and the two sides differ only in how they are drawn (Game §look.models.sets).
  - Two people play each other only in a universe; a skirmish is always against the AI (Game §session.skirmish).
  - There is one map, the one the game ships, with two starts: no other map, no map with spare starts, and no universe of more than two seats (ADR-036 decision 4; ADR-078 decision 2).
  - There is one star system: no travel between systems, and no jump from Relay to Relay (Game §map).
  - A skirmish has no save, no load and no pause (Game §session.skirmish). A universe pauses only while its server is down (Game §universe.persistence).
  - A running skirmish or matchup offers no surrender and no way back to the menu (Game §session.end); a universe is left, never surrendered (Game §session.join).
  - A universe has no season and no end, and nothing carries a save from one layout of the universe's state to another (Game §universe.persistence).
  - Nothing reaches a player outside the game: no notification and no web view of a universe.
- **Settings.**
  - There is no options screen, and the keys cannot be changed.
  - Nothing a player sets is kept between runs: the window starts full screen, the interface's scale at 100%, the windows at their first places and the difficulty at Normal (ADR-006 decision 1; ADR-070 decision 5; ADR-031 decisions 1 and 2; ADR-065 decision 4).
- **Sides and the AI.**
  - There are no races or factions with rules of their own: both sides have the same components, research and numbers, and the two model sets are looks (ADR-008 decision 3; Game §look.models.sets).
  - The AI has no personalities: its difficulties play one set of rules with different numbers (Game §ai.difficulty).
- **Economy.**
  - Ore is the only resource, and no transport or hauler carries it: a rig's income is paid straight to its player (Game §economy.income).
  - Nothing is charged over time, and nothing bounds the bank (Game §economy.bank).
- **Ships and combat.**
  - There are three hulls, Small, Medium and Large: no larger hull, no carrier, fighter or rearming, no commander unit, and no artillery or spotting (Game §ships.hulls).
  - The Sensor Array is the only module (Game §ships.modules).
  - A ship does not improve by fighting: there is no veterancy.
  - There is no damage type, accuracy, flight time or friendly fire (Game §combat.hits).
- **Interface.**
  - There is no strategic view: the widest view is the camera's, beside the minimap (Game §interface.camera).
- **Look and sound.**
  - The game plays no sound: no music, no effects and no voice.
  - No model has a texture or a vertex color: a face's color is its paint's, and its ridges are lines (Game §look.models; Game §look.lines).
- **Size.**
  - No map is larger than the fog's grid covers (Game §sight.drawn).

---

Retired keys: §world, §world.overview, §world.seats, §world.handover, §world.away, §world.persistence, §world.log, §world.loss, §world.restart.

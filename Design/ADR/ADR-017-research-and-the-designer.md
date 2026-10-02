# ADR-017 — Research is a paid queue whose upgrades apply at once, and the designer derives its stats from numbers the server sends

Status: **accepted** · 2026-10-01 · The "New picks" part of decision 7 is superseded by [ADR-023](ADR-023-queue-saves-the-design.md)

## Context

Milestone 5 gives the players research and the ship designer (design §7, §8, §9, §14). Research is milestone 5's task 5.1 and the designer task 5.2; task 5.3, the Missile Rack's splash, waits for gate G5 and is split off (owner, 2026-10-01).

Design §8 says that a player's one Research Lab researches one topic at a time, that topics cost Ore and time and some require another, and that upgrades apply at once to every existing ship and structure and change rates, never the size of a hit. It leaves the details open, and the owner settled them on 2026-10-01:

- **A lab queues up to five topics**, as a Shipyard queues ships, and each topic is paid for when it starts.
- **Hull Plating keeps a damaged ship's share of its hit points**, as in Warzone 2100.
- **A lab destroyed mid-topic loses that topic and its Ore**, as a destroyed build site does (design §5). Finished topics stay.

Design §9 puts the designer in the Shipyard panel, with live stats: damage per second after armor against each hull, per ship and per 100 Ore, cost and build time, and save, rename and queue. ADR-008 left a question for this task. The client cannot include `GameLogic`'s loader (ADR-002), so either the tuning types move to `GameProtocol` or the server sends the numbers.

## Decision

1. **Research is per player, and runs in its lab's queue.**
   - A player's finished topics are a list in the order they finished.
   - `StartResearchCommand` adds a topic to the back of the player's built Research Lab's queue, which holds up to `QUEUE_LIMIT`.
   - A topic is refused when it is researched or queued already, or when one of its prerequisites is neither researched nor ahead of it in the queue.
   - The front topic starts when the player can pay. It is paid for then, and until then it waits at the front, as a Shipyard's job does (ADR-016).
   - It takes its `researchSeconds`, counted in thousandths of a tick (ADR-014), and the next topic starts on the following tick.
   - The queue belongs to the lab. A lab destroyed takes its queue with it, and the topic under way is lost with its Ore.
2. **An upgrade is a factor on the tuning data's base number, and it applies on the tick its topic finishes.** Two topics on one rate multiply, as the Q2 model applies them.
   - **Hull Plating.** A hull's hit points are multiplied before the drive's factor. Each of the player's designs takes its new stats. Each of its warships takes the new maximum and keeps its share of hit points, rounded to the nearest hundredth and never below one. The Constructor has no hull, and structures have no upgrade (design §13), so neither changes.
   - **A fire-rate topic.** It divides the weapon's fire interval. Ships read the interval from their design, so existing ships fire faster from their next shot; a reload under way is not shortened.
   - **Improved Extraction.** It multiplies every rig's income.
   - **Automated Shipyards.** Each tick, a built Shipyard's job advances by the factor, rounded once to whole thousandths: 1,250 at 25%. The job under way speeds up too. The Command Station's Constructors do not, since it is not a Shipyard.
3. **Income is paid in full when it is not a whole number of hundredths a tick.** An upgraded home rig earns 6.25 Ore a second, which is 31.25 hundredths a tick at 20 Hz.
   - A player's income is counted per second, in hundredths.
   - Each tick adds it to a remainder in hundredths times ticks a second. The player receives the remainder divided by the tick rate, and keeps what is left over.
   - Integers keep a replay exact (ADR-009), and unupgraded income comes out as before: 25 and 40 hundredths every tick.
4. **An unlock makes a component available to its player alone.**
   - A component is available when no topic unlocks it, or when a topic that does is researched.
   - A new design must use available components.
   - The tuning loader rejects an upgrade whose stat is not its target's one rate: a Mining Rig's `income`, all hulls' `hitPoints`, a weapon's `fireRate` or the shipyards' `buildSpeed`. So the file cannot ask for an upgrade the game would ignore.
5. **The server sends the numbers, and one function derives the stats on both sides.**
   - **The snapshot carries the components as the player has them.** Every hull, drive and weapon has its numbers after the player's upgrades and whether it is available (`HullView`, `DriveView`, `WeaponView`). It also carries every research topic with its effect in words and whether the player has it, and the Shipyards' build-speed factor.
   - **One derivation, in `GameProtocol`.** `DesignStatsOf` turns three component views into `DesignStats`, together with `HitHundredths`, `DamagePerSecond`, `ShipMovement` and `DesignComponents`. These move from `GameLogic` to `GameProtocol/DesignStats.h`. The server derives a design's stats by building views from the tuning data and calling the same function, so the designer shows what a ship will fight with.
   - **The tuning types and the loader stay in `GameLogic`.** Moving them would give the client the whole file, the AI's review interval included. It would also put a second copy of the upgrade rules on the client, to apply research itself.
   - **The snapshot repeats this catalog every tick, as ADR-016 decision 10 does.** Measured with libstdc++ on x64 Linux, it adds 1,624 bytes of records and 357 bytes of text to each snapshot.
6. **A design is saved by `SaveDesignCommand`, and its components never change.**
   - **A new design.** An invalid identifier saves a new design of available components that the player has no design of yet.
   - **A rename.** The identifier of one of the player's own designs renames it, and it must name that design's components. A design of other components is a new design, so ships already built of a design stay what they are.
   - **The name.** It must pass `IsValidDesignName`: 1–32 printable ASCII characters, the ones the HUD's font holds (ADR-015), and not all spaces. The longest name design §7 writes, "Medium+Fusion+Missile Rack", fits. The client and the server use the same check.
   - **Refusals** have their own `CommandResult`s.
7. **The designer is client state, drawn by the HUD.**
   - `Outpost::Designer` in `GameApp` holds the picks, a typed name and whether the name field has the keyboard.
   - **Where it shows.** It appears at the top right when one of the player's built Shipyards is selected alone, beside that Shipyard's queue buttons.
   - **A saved design.** When the picks match one of the player's saved designs, the panel shows that design's name, and offers Rename once the name changes and Queue.
   - **New picks.** Otherwise it shows the components' names as the design calls them, and offers Save once every pick is unlocked.
   - **The stats.** It shows hit points, armor, speed, range, cost and build time, the build time divided by the Shipyards' factor. Below them is damage per second after armor against each hull, per ship and per 100 Ore.
8. **Text reaches the game as characters, and the name field takes the keyboard while it has it.**
   - **Characters.** `Neuron::Window` turns `WM_CHAR` into `InputEventKind::Character` events, which carry the UTF-16 unit.
   - **The keyboard while typing.** While the name field has the keyboard, `GameClient` gives every key and character to the designer and none to the controls or the camera: no order, control group or camera key reads them.
   - **Stopping.** Enter saves, and Escape drops what was typed. A press anywhere but the field ends typing and keeps the text, and so does the designer leaving the screen.
9. **Research shows in two places.** A line under the Ore names the topic under way and its progress, or that it waits for Ore. A selected Research Lab's panel lists its queue and each topic the player may still take, with what it does. That topic's button carries its cost, and is dim while the queue is full, the Ore short or a prerequisite neither researched nor queued.

## Consequences

- **`GameProtocol` holds functions, not only data.** They are pure, and they are the contract the designer and the simulation share. Splash damage (task 5.3) will add a field to `WeaponView` and `DesignStats` in the same place.
- **The Q2 check is unchanged.** It applies research to its own parts, multiplying a hull's hit points and dividing a weapon's interval, as decision 2 does.
- **Every snapshot costs about 2 KB more to build and copy.** In the Linux container, task 3.7's stress test ran at a median tick of 0.135–0.170 ms over four runs, against 0.142 ms before. That difference is within the noise between runs. Q4's measurement on the development machine includes it.
- **The upgrade factors round once.** With today's 15% and 25%, every hull's new maximum is a whole number of hundredths and a Shipyard's 1,250 thousandths a tick are exact. A damaged ship's share rounds to the nearest hundredth when Hull Plating lands, and an upgraded fire interval rounds to whole thousandths of a tick (ADR-014).
- **The AI (task 6.1) reads research and components from its snapshot**, as a player does.
- **Not run yet.** The designer panel, the research HUD and typing are presentation and input, and the owner's run checks them (AGENTS.md §3).

## What this forecloses

- Cancelling research, refunding it, or keeping a destroyed lab's progress, without a new decision.
- Changing a saved design's components: a changed design is a new one.
- The client reading the tuning file, or applying research itself.
- Upgrades to a range or the size of a hit (design §8), which the loader now rejects.

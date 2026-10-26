# ADR-017 — Research is a paid queue whose upgrades apply at once, and the designer derives its stats from numbers the server sends

Status: **accepted** · 2026-10-07
Owns: research-queue, research-upgrades, component-unlocks, component-catalog-in-snapshot, design-saving, ship-designer, design-name-input

## Context

A player's one Research Lab researches topics that cost Ore and time, some of which require another, and an upgrade applies at once to every existing ship and structure and changes a rate, never the size of a hit (Game §research). The owner settled three of its details (owner, 2026-10-01):

- **A lab queues up to five topics**, as a Shipyard queues ships, and each topic is paid for when it starts.
- **Hull Plating keeps a damaged ship's share of its hit points**, as in Warzone 2100.
- **A lab destroyed mid-topic loses that topic and its Ore**, as a destroyed build site does. Finished topics stay.

The designer shows live stats: damage per second after armor against each hull, per ship and per 100 Ore, cost and build time, and it saves, renames and queues (Game §ships.designer). The client cannot include `GameLogic`'s loader (ADR-002), so either the tuning types move to `GameProtocol` or the server sends the numbers.

## Decision

1. **Research is per player, and runs in its lab's queue** (owner, 2026-10-01).
   - A player's finished topics are a list in the order they finished.
   - `StartResearchCommand` adds a topic to the back of the player's built Research Lab's queue, which holds up to `QUEUE_LIMIT`.
   - A topic is refused when it is researched or queued already, or when one of its prerequisites is neither researched nor ahead of it in the queue.
   - **A topic starts once its prerequisites are researched and the player can pay**, and it is paid for then; until then it waits at the front, as a Shipyard's job does (ADR-016). From the Lab's level that gives a second slot ([ADR-064](ADR-064-structure-upgrades.md) decision 10), the topic behind the front starts the same way once the front one has started, and each takes its own time. When the first finishes, the second's progress moves to the front. The snapshot carries the second's progress (`secondJobPermille`).
   - It takes its `researchSeconds`, counted in thousandths of a tick (ADR-014), and the next topic starts on the following tick.
   - The queue belongs to the lab. A lab destroyed takes its queue with it, and the topic under way is lost with its Ore.
2. **An upgrade is a factor on the tuning data's base number, and it applies on the tick its topic finishes.** Two topics on one rate add their percentages ([ADR-033](ADR-033-research-tiers.md) decision 3).
   - **Hull Plating.** A hull's hit points are multiplied before the drive's factor. Each of the player's designs takes its new stats. Each of its warships takes the new maximum and keeps its share of hit points, rounded to the nearest hundredth and never below one. Neither the Constructor nor a structure has a hull, so neither changes; structures' hit points have a topic of their own ([ADR-033](ADR-033-research-tiers.md) decision 4).
   - **A fire-rate topic.** It divides the weapon's fire interval. Ships read the interval from their design, so existing ships fire faster from their next shot; a reload under way is not shortened.
   - **Improved Extraction.** It multiplies every rig's income.
   - **Automated Shipyards.** Each tick, a built Shipyard's job advances by the factor, rounded once to whole thousandths. The job under way speeds up too. The Command Station's Constructors do not, since it is not a Shipyard.
3. *(retired)*
4. **An unlock makes a component available to its player alone.**
   - A component is available when no topic unlocks it, or when a topic that does is researched.
   - A new design must use available components.
   - The tuning loader rejects an upgrade whose stat is not its target's one rate: a Mining Rig's `income`, all hulls' `hitPoints`, a weapon's `fireRate`, the shipyards' `buildSpeed`, or one of the targets [ADR-033](ADR-033-research-tiers.md) decision 4 adds. So the file cannot ask for an upgrade the game would ignore.
5. **The server sends the numbers, and one function derives the stats on both sides.**
   - **The snapshot carries the components as the player has them.** Every hull, drive and weapon has its numbers after the player's upgrades and whether it is available (`HullView`, `DriveView`, `WeaponView`). It also carries every research topic with its effect in words and whether the player has it, and the Shipyards' build-speed factor.
   - **One derivation, in `GameProtocol`.** `DesignStatsOf` turns three component views, and a module's when the design has one ([ADR-058](ADR-058-modules.md)), into `DesignStats`, together with `HitHundredths`, `DamagePerSecond`, `ShipMovement` and `DesignComponents`. They live in `GameProtocol/DesignStats.h`. The server derives a design's stats by building views from the tuning data and calling the same function, so the designer shows what a ship will fight with.
   - **The tuning types and the loader stay in `GameLogic`.** Moving them would give the client the whole file, the AI's review interval included. It would also put a second copy of the upgrade rules on the client, to apply research itself.
   - **The snapshot repeats this catalog every tick, as ADR-016 decision 10 does.**
6. **A design is saved by `SaveDesignCommand`, and its components never change.**
   - **A new design.** An invalid identifier saves a new design of available components that the player has no design of yet.
   - **A rename.** The identifier of one of the player's own designs renames it, and it must name that design's components. A design of other components is a new design, so ships already built of a design stay what they are.
   - **The name.** It must pass `IsValidDesignName`: 1–32 printable ASCII characters, the ones the HUD's font holds (ADR-015), and not all spaces. A components' name that a module's would take past that is written with the module's initials ([ADR-069](ADR-069-starting-design-names.md) decision 5). The client and the server use the same check.
   - **Refusals** have their own `CommandResult`s.
7. **The designer is client state, drawn by the HUD.**
   - `Outpost::Designer` in `GameApp` holds the picks, a typed name, the count and whether the name field has the keyboard.
   - **Where it shows.** It is a floating window, opened from a selected Shipyard of the player's or by its key ([ADR-031](ADR-031-floating-windows.md) decision 6). Its size and its rows are the interface ADRs' ([ADR-062](ADR-062-type-scale-and-contrast.md), [ADR-068](ADR-068-interface-polish.md)).
   - **Its target Shipyard.** Its queue goes to one of the player's finished Shipyards, which its arrows step through by number; opening it with a Shipyard selected, or selecting one while it is open, makes that one the target. Without a finished Shipyard it still designs and saves.
   - **The picks.** A hull, a drive, a weapon, and a module or none, each picked from a row of cards; a locked component's card names the topic that unlocks it. A saved design's chip loads its picks.
   - **A saved design.** When the picks match one of the player's saved designs, the panel shows that design's name, and offers Rename once the name changes, and Queue.
   - **New picks.** Otherwise it names them as [ADR-069](ADR-069-starting-design-names.md) decision 5 has it, and offers Save and Queue once every pick is unlocked ([ADR-023](ADR-023-queue-saves-the-design.md)).
   - **The figures.** Bars show hit points, armor, speed, range, the module's sight, cost and build time, the build time divided by the Shipyards' factor, each against the best that any design of the components reaches, locked ones included. Below them, a card for each hull gives damage per second after its armor, per ship and per 100 Ore, counting each ship of a formation that a splash reaches ([ADR-014](ADR-014-designs-and-combat.md) decision 12). A card is rated good from two thirds of the best any design does to that hull, fair from a third, and poor below.
   - **A preview.** Hovering a component's card shows the bars and cards of the design with it in place, and the change against the picks.
   - **A locked component's preview.** A locked card answers the pointer as an open one does, with the design it would make at the player's research as it stands, so that a player sees what a topic buys before researching it. It takes no click: its area is a preview's (`Layout::previews`), which `Layout::HoverAt` finds and `Layout::ActionAt` does not. It keeps its hatching and its research line and lights nothing, and the help line names its topic, "Preview: with Large, once Large Hull is researched", or "once it is researched" where no topic unlocks it, so that the preview is not read as a pick.
   - **The count.** It runs from one to the target Shipyard's free queue slots, and Queue adds that many jobs.
8. **Text reaches the game as characters, and the name field takes the keyboard while it has it.**
   - **Characters.** `Neuron::Window` turns `WM_CHAR` into `InputEventKind::Character` events, which carry the UTF-16 unit.
   - **The keyboard while typing.** While the name field has the keyboard, `GameClient` gives every key and character to the designer and none to the controls or the camera: no order, control group or camera key reads them.
   - **Stopping.** Enter saves, and Escape drops what was typed. A press anywhere but the field ends typing and keeps the text, and so does the designer leaving the screen.
9. **Research shows in two places.** A line of the status panel under the Ore names the topic under way and its progress, that it waits for Ore, or that the Lab is idle ([ADR-066](ADR-066-production-status-on-the-hud.md)). The research window ([ADR-031](ADR-031-floating-windows.md) decision 6) lists the Research Lab's queue and each topic the player may still take, with what it does and its cost; a topic of a tier not open is shown as [ADR-033](ADR-033-research-tiers.md) decision 5 has it.

## Consequences

- **`GameProtocol` holds functions, not only data.** They are pure, and they are the contract the designer and the simulation share. The splash radius is a field of `WeaponView` and `DesignStats` ([ADR-014](ADR-014-designs-and-combat.md) decision 12).
- **The balance check applies research to its own parts as decision 2 does**, multiplying a hull's hit points and dividing a weapon's interval.
- **The upgrade factors round once.** With the tuning data's percentages, summed as [ADR-033](ADR-033-research-tiers.md) has them, every hull's new maximum is a whole number of hundredths and a Shipyard's 1,250 or 1,500 thousandths a tick are exact. A damaged ship's share rounds to the nearest hundredth when Hull Plating lands, and an upgraded fire interval rounds to whole thousandths of a tick (ADR-014).
- **The AI reads research and components from its snapshot**, as a player does.

## What this forecloses

- Cancelling research, refunding it, or keeping a destroyed lab's progress, without a new decision.
- Changing a saved design's components: a changed design is a new one.
- The client reading the tuning file, or applying research itself.
- Upgrades to a range or the size of a hit, which the loader rejects (decision 4).

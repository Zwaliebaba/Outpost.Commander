# ADR-032 — Graphs are built off the order, a large order plans over two ticks, and a search does less

Status: **accepted** · 2026-10-03
Owns: path-graph-preparation, large-order-split, path-search-pruning

## Context

A tick has a budget, the orders included ([`Docs/Budgets.md`](../Budgets.md)). Most of an order tick goes to path graphs and searches. The pathfinder keeps a graph for each clearance, one per hull and the Constructor, over the obstacles ([ADR-010](ADR-010-movement-and-pathing.md)), and a structure placed or destroyed changes the obstacles each graph stands on. An order then searches a path for each of its ships, and a ship tries the routes of the ships before it, each try a run of line tests. An order of a whole fleet does all of that in one tick.

## Decision

1. **The graphs are built when match setup is over**, by `InProcessServer::PreparePathfinding` at the end of `SetUpServer`, which `CreateInProcessServer` and the other server factories call, after the bases and any load are placed. The first order of a match finds them built.
2. **A tick that plans no paths brings one graph up to date.** The pathfinder remembers every clearance it has built a graph for, and `Pathfinder::PrepareNext` brings the first that needs it up to date with the obstacles placed and taken away since; what that does to a graph is [ADR-054](ADR-054-obstacle-grid-and-extended-graphs.md) decisions 5 to 7's. A structure placed or destroyed in play so costs one graph's update a tick, on the quiet ticks after it, instead of all of them in the next order. The graphs are a cache: when one is brought up to date changes no path. An order that comes before they are all up to date still brings up to date what it needs.
3. **An order for more than 32 ships plans its paths over two ticks.** It lays its slots and searches its group's route in its own tick, plans every other ship's path then, and the rest at the start of the next, before that tick's orders. Alternate ships rather than the front half, because the ships behind search more often: each tick then plans about half the work. Its ships hold, with no order, until every path is planned, and the group sets off together in the second tick at one pace, as ADR-010 decision 4 has it. The routes the first tick's searches found are kept for the second, so a ship there joins them as it would have. A ship given any other order in between leaves the group; one destroyed is skipped; an attack whose target is destroyed leaves its ships holding. The orders being planned are state, so `Simulation`'s equality compares them. An order of 32 ships or fewer plans and sets off in its own tick.
4. **A search does less, for the same answer.** It does not heap every corner the start might see: those entries wait in a list whose smallest are sorted 64 at a time, and the next entry is the smaller of the list's front and the queue's, which takes them in the very order one queue would. And a ship tries a route only when the shortest any way along it could be, the line to one of its corners plus the route from there plus the line from its end, is within the detour limit; a route that is not would be rejected after its line tests anyway.

## Consequences

- **Decisions 2 and 4 change no state.** The graphs are a cache and a search finds the same route, so the same load ends in the same state, bit for bit, with them and without.
- **A large group sets off one tick after its order** (`rules.tickHz`), and its second half plans from where its ships stand then. Its first visible response comes a tick later than a small order's, and the response has a budget in [`Docs/Budgets.md`](../Budgets.md).
- **The balance check fields armies of more than 32 ships at its larger budgets**, so its battles plan over two ticks as a match's do.
- **A structure destroyed is taken out of each graph in use, and one placed extended over** ([ADR-054](ADR-054-obstacle-grid-and-extended-graphs.md) decisions 6 and 7).
- **Combat ticks** that destroy a structure leave its graphs to the quiet ticks after. An order in the few ticks after a structure falls can still bring one up to date.
- **The tests:** `PackagedServerTests.TheFirstOrderBuildsNoPathGraph` checks decision 1; `MovementTests.RebuildsDroppedGraphsOnQuietTicks` the quiet ticks' upkeep; and `MovementTests.PlansALargeOrderOverTwoTicks` and `AShipOrderedAgainLeavesItsPlanningGroup` the split.

## What this forecloses

- Planning a large order over more than two ticks, without a new decision: a third tick would put its first response two ticks behind a small order's.
- Starting a large group's front ships before its rear ships are planned: the group keeps one pace, and its pace needs every path.

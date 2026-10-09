# ADR-086 — The HUD says where the match runs, and when its server has gone quiet

Status: **accepted** · 2026-10-09

## Context

A match runs on one of two servers. A skirmish runs on the game's own, on a thread of its own in the same process, which both players reach over QUIC ([ADR-060](ADR-060-quic-in-process.md)). A world runs on a dedicated server that the player joins over the network, at the address the join file names ([ADR-078](ADR-078-dedicated-server.md)). Nothing on screen told the two apart.

A connection to a dedicated server that stops answering is only reported once MsQuic's idle timeout ends it, which is 30 s by default, since `QuicChannel` sets only the handshake's. Until then the world simply stops moving: the interpolator has nothing newer to show, and nothing says why.

The owner asked on 2026-10-09 for the client to show whether it plays standalone or on a server, and chose a tag in the top-right corner that names the server's address and warns once its snapshots stop coming.

## Decision

1. **A tag in the top-right corner says where the match runs**, "Local skirmish" for a match on the game's own server, and "Server 203.0.113.5:4433" for a world on a dedicated server. `Hud::ServerName` writes the address as the join file names it: the host and the port, with an IPv6 host in brackets, "[2001:db8::5]:4433". The shell passes it to `GameClient::StartMatch`, and nothing for a skirmish.
   - The tag is as wide as its words, its right edge 16 units from the screen's, and an address wider than 360 units is cut short. It takes no click.
   - It shows with the rest of the HUD, from the match's first snapshot. A join whose server never welcomes the client already goes back to the menu, saying why (ADR-078).
2. **The tag warns once no snapshot has come for `Hud::SILENT_SECONDS`, 2 s**: a warning chip under the address, "No word from the server · 5 s" ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1), counting whole seconds up to 999. The server sends each player a snapshot every tick, 20 a second at the tuning data's rate ([ADR-009](ADR-009-deterministic-core.md) decision 3, [ADR-013](ADR-013-client-view-and-controls.md)), so 2 s is 40 missed in a row, which no ordinary hitch on a working connection makes. While it warns, the tag is as wide as "No word from the server · 999 s", so that its edge does not move as the seconds count.
   - `GameClient` counts the seconds since `Receive` last took a snapshot, by the frames' elapsed time.
   - A skirmish warns too. Its server is on a thread of the same process, and a stall there leaves the world as still as a lost connection does.
3. **The ship designer covers it.** The designer opens in the top-right corner, and covers the tag while it is there, as a window covers any panel of the HUD.

## Consequences

- **The silence is the client's measure, not the connection's.** A server that is up but too slow to keep 20 snapshots a second warns the same as one that has gone. That is the point: either way the world on screen has stopped.
- **The warning ends with the next snapshot,** and the match goes back to the menu as before when the connection is reported lost.
- **Tests:** `HudTests.ShowsWhereTheMatchRuns` checks no tag without a connection; "Local skirmish" anchored to the top-right corner; a server's address, no warning after a second; the chip under the address at 2, 10, 999 and 5,000 s, the last written as 999, with the tag one width throughout; and an address of 80 characters cut short within the tag's bound. `WritesAServerName` checks an IPv4 address, a host name and an IPv6 address. The whole-HUD contrast and overlap tests see the tag at its widest through `LongestContent`.
- `GameClient`'s count and the shell's address need D3D12 and Windows, so they have no test; CI builds them, and the owner's run sees the tag in a skirmish and on a dedicated server, and the warning when the server is stopped.

## What this forecloses

- **A ping or round-trip time on the tag,** without reading MsQuic's statistics through `QuicChannel`, which nothing does today.
- **A tag on the menu.** The menu has no match to place.

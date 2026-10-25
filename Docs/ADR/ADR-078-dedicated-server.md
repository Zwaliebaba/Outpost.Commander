# ADR-078 — A world runs on a dedicated server, whose seats are taken with a token at any time, and whose certificate is kept

Status: **accepted** · 2026-10-08
Owns: dedicated-server, world-settings, world-seats, world-certificate, join-file, lost-connection-menu, server-listen-address

## Context

A world runs on a dedicated server that a player on another machine reaches. It runs on the owner's PC or on a Windows Server VM, and a player's game finds it by its address, its certificate's hash and a seat token (owner, 2026-10-08). A match's server runs inside the game, listens on the loopback and makes a certificate for each match ([ADR-060](ADR-060-quic-in-process.md)). A world needs more:

- an executable that runs a world from its folder, with no window, until it is told to stop;
- an address and a port that another machine can reach;
- a certificate that stays the same from one run to the next, so that a player pins it once;
- a seat that only its player can take, at any time while the world runs, and again after a dropped connection;
- a way for the player's game to learn all of that, and to say why it lost its connection.

## Decision

1. **`OutpostServer` is a console executable** (`SubSystem` Console, not packaged), with ADR-002's row: it includes and links `NeuronCore`, `NeuronServer`, `GameProtocol`, `GameLogic` and `Opponent`, whose AI empires and deputies it hands the server to host ([ADR-079](ADR-079-seat-controller-and-deputy.md)). It reads `Assets\Tuning.json`, `Assets\Map.json`, `Assets\Matchups.json` and the AI's settings files beside itself, copied there by its project, as the game reads its own (ADR-008), and finds `msquic.dll` beside itself through `NeuronCore`'s content (R14).
   - `OutpostServer --new-world <folder> [--host <name>] [--address <ip>] [--port <n>] [--seed <n>] [--ai <player>[:<difficulty>]]` writes a new world's settings and refuses a folder that holds a world already. An option it is not given leaves decision 2's default, and the seed's is the system clock's count since its epoch as the world is made. `--ai` gives a seat to an AI empire, Normal unless it names Easy or Hard (ADR-079).
   - `OutpostServer <folder>` runs the world, hosting an AI empire for each AI seat and a deputy for each player's (ADR-079). Ctrl+C, Ctrl+Break, closing the console, signing out and shutting down all ask it to stop. It then destroys the server, so that the world's folder writes its last save (ADR-077). The console handler waits up to 5 seconds for that, since Windows ends the process once the handler of a close returns. Once a second the main thread takes the server's tick timings, which is where a failure on its thread reaches it (ADR-025), and a failure ends the process with its message on standard error.
   - `--world-run` is [ADR-082](ADR-082-world-log-and-world-run.md) decision 4's, and `--matchups` [ADR-083](ADR-083-battle-matchups.md) decision 5's.
2. **A world's settings are `World.json` in its folder** (`WorldSettings`): the address the server listens on, `0.0.0.0` by default; its UDP port, `DEFAULT_WORLD_PORT` (45000) by default; the host its players reach it at, which the join files name, `localhost` by default; the seed, as a string of its decimal digits, since a JSON number cannot hold every 64-bit seed; and a seat for each of the map's starts, each a player's with a token, or an AI empire's with its difficulty (ADR-079). The owner makes them once; the server only reads them. A world whose folder holds a save comes back with the save's seed (ADR-077).
3. **A seat is taken with a token.** A `SeatToken` is 128 bits from `std::random_device`, which draws from the system's cryptographic source under MSVC and Linux, never from the simulation's PRNG (ADR-009). The hello carries it after the player. `Server::OpenSeat` takes the seat's token, and the `ServerAddress` it returns carries it; the shell gives each match's seat a new one. A hello whose token is not its seat's is refused as `SeatRefused`.
4. **A world's seat is taken at any time, and the newest connection holds it.** A match's seats are taken before it starts, and `Start` throws while one is open. A world, a server made with `ServerDesc::world`, starts with its seats open, and takes a hello with the seat's token at any time. A hello for a seat that is held replaces the connection that held it, which is closed with `CloseReason::SeatTaken`. The newest connection wins so that a player whose connection dropped without the server knowing yet takes its seat back at once, rather than waiting out MsQuic's idle timeout. The swap is made under the seat lock, and the old connection is closed after the lock is let go, since its own callbacks take it. A command from a connection the seat was taken from is dropped. A snapshot is whole, so the first after a seat is taken shows everything the player sees and remembers; the shots and destructions of the ticks it missed are lost, which is presentation.
5. *(retired)*
6. **The listener listens on the address it is given** (`QuicListener::Desc::address`, from `ServerDesc::quicAddress`), as an IPv4 or IPv6 literal, and on `127.0.0.1` when it is given none; and on `ServerDesc::quicPort`, or a port the system chooses at zero. An address that is not one is refused. `OpenSeat` names the host a player on the same machine reaches it at: `127.0.0.1` for a server given no address or listening on IPv4's every interface (`0.0.0.0`), `::1` for one listening on IPv6's (`::`), and otherwise the address it listens on.
7. **A world's certificate is kept in its folder** (`QuicListener::Desc::identity`, from `ServerDesc::world`). `ServerCertificate` made with a folder:
   - opens the certificate in `Certificate.der` and the key whose name `Certificate.key` holds, and uses them when the key is in the user's key store and the certificate is valid now, after naming the key on the certificate (`CERT_KEY_PROV_INFO_PROP_ID`), as `CertCreateSelfSignCertificate` does, so that Schannel finds it;
   - otherwise makes a new key and certificate, valid for five years, writes both files, and deletes the key of a certificate that is not valid now;
   - leaves the key in the store when it goes.

   So a player pins a world's certificate once: its hash changes only when its key is lost or after five years. A match's certificate is made and deleted as [ADR-060](ADR-060-quic-in-process.md) decision 6 has it.
8. **A join file tells a player's game where its seat is** (`JoinTicket`): a JSON object of the host, the port, the certificate's SHA-256 and the seat's token in hexadecimal, and the player. `OutpostServer` writes `Join-Player<n>.json` for each player's seat into the world's folder each time it runs, and the owner hands each to its player.
9. **The game joins a world** from `--join <file>`, or from the menu's "Join world", which shows when the player has `Outpost Commander\Join.json` in their documents (`SHGetKnownFolderPath(FOLDERID_Documents)`). The documents, not the game's local folder, because the packaged game's own application data is redirected away from where a player would put a file, and the documents are not. A joined world is the player's connection alone: no server, rival or AI in the game.
10. **A lost connection takes the player back to the menu, saying why.** `QuicChannel::PeerErrorCode` gives the code the peer closed with, and `QuicTransport` throws `DescribeClose`'s words for a `CloseReason`. The shell catches it from the player's connection, a match's or a world's, and shows the menu with the reason in the warning's color (`Hud::MenuState`).

## Consequences

- **Tests.**
  - `WorldSettingsTests`: settings and join files read back as written, every refusal, and a seed past 2⁵³ intact.
  - `QuicTransportTests`: a seat refused without its token, a world's seat taken after it starts and again by a newer connection, a world's certificate kept and used by Schannel when it runs again, a listen address, and a hello of another version.
  - `HudTests.OffersAWorldAndSaysWhyTheLastGameEnded`: the menu offers a world to join and says why the last game ended.
- **The dedicated server needs Windows 11, or Windows Server 2022 or later.** It listens over QUIC, which needs TLS 1.3 from Schannel ([ADR-004](ADR-004-quic-transport.md)).
- **A world's port is reached through the owner's router and firewall.** Windows Defender Firewall asks about `OutpostServer` the first time it listens on an interface other than the loopback. A server behind a router needs the port forwarded. Neither is the game's to do.
- **The tokens are in `World.json` and the join files as plain text.** Anyone who reads the world's folder or a player's join file can take that seat. That is the trust of a circle of friends, not of a public server.

## What this forecloses

- **Taking a seat without its token,** or a player id alone.
- **A second connection holding a seat beside the first.** The newest one holds it.
- **Validating a world's certificate against a root.** It is pinned ([ADR-060](ADR-060-quic-in-process.md) decision 6), and kept.

# ADR-098 — A state crosses as what changed since the one the client holds

Status: **accepted** · 2026-10-26
Owns: state-changes, held-states

## Context

With its catalogue and its events apart ([ADR-060](ADR-060-quic-in-process.md) decision 9, [ADR-096](ADR-096-events-and-state-cross-apart.md)), a seat's state still carries every entity the player sees, whole, every tick. Under the measurement load a state is 21.8 KB. From one tick to the next a moving ship changes its place and its heading, 12 of the 122 bytes it takes, and a structure or an asteroid most often nothing. Measured in CI's Debug|x64 run of `ImpairedLinkTests.HoldsASeatsBacklogToTwoStatesOnTheHotspot` on 2026-10-26, the hotspot profile carried 462 states of that load in a minute of 1,200 ticks, with 9,590 datagrams carried to the client and 197 lost: MsQuic's congestion control backs off at each loss, and holds a lossy link below what whole states take. The owner had the states cross as changes ahead of any measurement of what a seat's state takes (owner, 2026-10-26).

## Decision

1. **A state crosses as what changed since the newest state the client holds, where that is the shorter** (owner, 2026-10-26). `ChangeOf` writes it from the two states as they cross the wire, without their catalogue and events.
   - **A record changes by its fields:** a bit for each of its fields, in the order `WireFields.h` lists them, set where the field differs bit for bit, and then each field that differs, whole. Floats compare by their bits, so a change keeps a zero's sign.
   - **A list of records with identifiers changes by its elements,** where both states list them in increasing order of identifier, as they do the entities, the sectors and the scheduled orders: the identifiers of the elements gone, each element changed with its identifier and its own change, and each element new, whole. A list out of order crosses whole.
   - **A moving ship's change** is its identifier, five bytes of its fields' bits, its place and its heading: 21 bytes of the 122 it takes whole, by the layout.
   - **The message,** `StateChangeMessage`, names the tick of the state it is a change of and how many catalogues and events the connection had been sent (ADR-096 decision 3), and crosses where a `StateMessage` would, on a stream of its own (ADR-096 decision 1). `PutChange` puts it together with that state.
2. **The client says which state it holds.** Once it holds a state, whole or put together, the client sends its tick, `HeldMessage`, up its own stream (ADR-096 decision 1). The listener's receiver keeps the newest tick a seat's client has said in the seat's `LoopbackChannel`, under its lock (ADR-060 decision 7), and only from the connection that holds the seat ([ADR-078](ADR-078-dedicated-server.md) decision 4).
3. **The server keeps the states it sent a seat** (`SentStates`), on the thread that steps it: each one `QuicChannel::SendNewest` queued, `STATES_KEPT`, 32, at most, which is 1.6 s at 20 ticks a second, and none older than the one the client holds. It sends the next state as a change of the one the client holds while it keeps it. It sends the whole state when the client has said it holds none, when the one it holds is no longer kept, and when the change would be the longer. A seat taken again lets every kept state go, since its new connection holds none.
4. **The client holds each state until a change of a newer one comes** (`HeldStates`), `STATES_KEPT` at most. The server changes from the newest state it has heard the client holds, which is never older than the one its last change was from, so a held state older than that one is let go. A change of a state the client does not hold, or that is no change of it, is a broken stream, and `QuicTransport::Receive` throws.
5. **All of it is in the transport,** as the catalogue is (ADR-060 decision 9). `Snapshot` is unchanged, and the snapshot a client hands on is the one the server built, bit for bit. A state lost on the way breaks nothing after it, since the server changes only from a state the client has said it holds.

## Consequences

- **Each state costs the server more:** a copy of it to keep, a comparison of each of its fields with the state the client holds, and its encoding whole as well as its change, to tell which is the shorter. These run in the tick's `Snapshots` part, so the tick's timings include them. The client copies each state it holds.
- **An acknowledgement is never sent again.** The next state's says as much, and a client that says nothing is sent whole states.
- **The wire's layout changed,** so a client of an older build is refused as `WrongVersion` (ADR-060 decision 5). `PROTOCOL_VERSION` stays 1.
- **Tests.**
  - `WireFormatTests.CarriesAStateAsWhatChanged` puts together a change and the state it changes into the state built, bit for bit: an entity moved, one gone, two new, a heading whose zero turned negative, a sector taken and the player's Ore. It also counts a moved ship's change in bytes and carries a list out of order whole. `RefusesAChangeOfAnotherState` refuses a change put together with a state it is not a change of, one that names a field past the state's last, and one cut short or run on.
  - `SentStatesTests` checks when the server sends a change and when the whole state, and `HeldStatesTests` what the client holds and what it refuses.
  - `ImpairedLinkTests.SendsAStateAsAChangeOfOneTheClientHolds` checks that through the Wi-Fi profile, over 20 s of the measurement load, no more than one state in 20 crosses whole, and that the states average under a third of the largest.
  - `QuicTransportTests.HandsOnTheSnapshotsTheServerBuilt` checks two minutes of snapshots, whole or put together from changes, against the ones the server built.
  - The QUIC tests run in CI's Debug|x64 run only.

## What this forecloses

- **A change of a state the client has not said it holds.** A change of the state sent last would not wait the acknowledgement's round trip, but one state lost on the way would break every change after it until a whole one came.

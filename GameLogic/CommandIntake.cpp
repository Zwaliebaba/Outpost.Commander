#include "pch.h"

#include "CommandIntake.h"

#include <vector>

namespace Outpost
{

EntityId ResolveWireIdentity(const World& _world, WireIdentity _wireIdentity) noexcept
{
  const std::uint16_t index = IndexOf(_wireIdentity);
  if (!_world.IsSlotAlive(index))
  {
    return NO_ENTITY;
  }

  const EntityId stored = _world.EntityInSlot(index).id;

  // Eight bits is all the wire carries, so this compares eight bits. The store's generation is wider
  // and that is deliberate (R16 does not owe the wire its precision); what matters here is that a
  // sender naming a slot's previous occupant is refused.
  if (GenerationOf(_wireIdentity) != static_cast<std::uint16_t>(stored.generation & WIRE_GENERATION_MASK))
  {
    return NO_ENTITY;
  }
  return stored;
}

bool CommandIntake::IsNewer(std::uint16_t _sequence, std::uint16_t _lastApplied) noexcept
{
  // EXPLICITLY, RATHER THAN "AT OR BELOW" (Q24). A sequence is sixteen bits and wraps; at the
  // wrap, 1 is newer than 65,535 and a plain comparison says the opposite, which would make the
  // host ignore every command for the rest of the match. The signed difference is the same trick
  // the binary angle uses for headings, and it is right for the same reason.
  return static_cast<std::int16_t>(_sequence - _lastApplied) > 0;
}

CommandRejection CommandIntake::Apply(World& _world, BuildSystem& _build, PlayerId _player, const Command& _command) noexcept
{
  if ((_player == NO_PLAYER) || (_player > MAX_PLAYERS))
  {
    return CommandRejection::Empty;
  }
  if (!IsKnown(_command.type))
  {
    return CommandRejection::UnknownType;
  }

  // **THE SELECTION HAS TO MATCH THE TYPE, IN BOTH DIRECTIONS** (M1.6). A move with nothing selected
  // is malformed; a build carrying identities is a build order with 600 of them in it, which is the
  // amplification Q24 is about arriving through a door that did not exist when Q24 was written.
  if (ActsOnSelection(_command.type) != !_command.selection.empty())
  {
    return CommandRejection::Empty;
  }

  // Sequence first, because a resent command is the common case and everything below it costs
  // more. ADR-003 repeats a command in every packet until it is acknowledged, so most arrivals of
  // most commands land here and must leave without a side effect.
  if (m_hasApplied[_player] && !IsNewer(_command.sequence, m_lastApplied[_player]))
  {
    return CommandRejection::AlreadyApplied;
  }

  // THE STATION'S ORDERS ACT ON NO SELECTION, so every check below this belongs to the other three.
  // The acknowledgment still advances on a refusal, for the reason an Attack that resolves nothing
  // does: the host UNDERSTOOD the order, and a sequence that did not advance would have the client
  // repeat it forever.
  if (!ActsOnSelection(_command.type))
  {
    // **THE PLAYER'S OWN SHIPYARD SETS THE RATE, AT THE START** (M2.12): an item keeps the rate it began at, so a
    // shipyard finished or lost mid-build changes the next item and not this one.
    const std::uint32_t buildRate = BuildRateMultiplierPercent(_world, _player);
    bool ordered = false;
    switch (_command.type)
    {
    case CommandType::Build:
      ordered = _build.Start(_world, _player, static_cast<DesignId>(_command.TargetDesign()), buildRate) == BuildRejection::None;
      break;
    case CommandType::PlaceModule:
    {
      // THE SITE IS THE BUILD SYSTEM'S TO JUDGE, with `CheckModuleSite` (M2.11), from the point as the wire said
      // it -- clamped for the reason every point here is.
      const Neuron::Vec2 site =
        ClampToPlayArea(Neuron::Vec2{.x = DequantizePosition(_command.targetX), .y = DequantizePosition(_command.targetY)});
      ordered = _build.StartModule(_world, _player, static_cast<DesignId>(_command.placedDesign), site, buildRate) == BuildRejection::None;
      break;
    }
    case CommandType::UpgradeModule:
      // A stale or unknown identity resolves to nothing, which the build system refuses as not upgradeable.
      ordered = _build.StartUpgrade(_world, _player, ResolveWireIdentity(_world, _command.TargetEntity()),
                                    static_cast<DesignId>(_command.UpgradeLevel()), buildRate) == BuildRejection::None;
      break;
    case CommandType::CancelBuild:
      ordered = _build.Cancel(_player);
      break;
    case CommandType::MoveTo:
    case CommandType::Attack:
    case CommandType::Mine:
      // Unreachable: these act on a selection and were sent below by `ActsOnSelection`.
      break;
    }

    m_lastApplied[_player] = _command.sequence;
    m_hasApplied[_player] = true;
    return ordered ? CommandRejection::None : CommandRejection::BuildRefused;
  }

  // BOUNDED AT THE SENDER'S OWN ENTITY COUNT, which is the check that turns the amplification into
  // a rejected packet. A selection longer than the player has ships cannot be a real selection
  // however it was produced.
  const std::size_t owned = _world.OwnedCount(_player);
  if (_command.selection.size() > owned)
  {
    return CommandRejection::SelectionTooLong;
  }

  // EVERY IDENTITY IS RESOLVED AND CHECKED BEFORE ANY OF THEM IS ACTED ON. A command that is
  // half applied and then refused would leave the match in a state no sequence number describes,
  // and the client would never learn which half took.
  for (const WireIdentity wire : _command.selection)
  {
    const EntityId resolved = ResolveWireIdentity(_world, wire);
    if (!resolved.IsValid())
    {
      return CommandRejection::StaleGeneration;
    }
    const Entity* entity = _world.Find(resolved);
    if ((entity == nullptr) || (entity->owner != _player))
    {
      return CommandRejection::NotOwned;
    }
  }

  // Clamped, although a wire target cannot leave the play area -- see ClampToPlayArea for why it
  // is written anyway.
  const Neuron::Vec2 target =
    ClampToPlayArea(Neuron::Vec2{.x = DequantizePosition(_command.targetX), .y = DequantizePosition(_command.targetY)});

  // A ROCK THE FIELD HAS, before anything is acted on -- the same all-or-nothing rule as the identities.
  if ((_command.type == CommandType::Mine) && (_command.TargetRock() >= _world.Field().size()))
  {
    return CommandRejection::NoSuchRock;
  }

  if (_command.type == CommandType::MoveTo)
  {
    // **ONE RING SLOT EACH, RATHER THAN FIFTY SHIPS ON ONE POINT** (Q19, M1.7). The whole selection
    // goes to `RingAssignment` together, because a slot depends on who else is in the order -- which
    // is why this is one call and not a loop.
    std::vector<EntityId> resolved;
    resolved.reserve(_command.selection.size());
    for (const WireIdentity wire : _command.selection)
    {
      resolved.push_back(ResolveWireIdentity(_world, wire));
    }
    static_cast<void>(OrderFleetTo(_world, resolved, target));

    // **A MOVE ENDS A MINE ORDER AND KEEPS THE CARGO** (M2.6). The standing order is the one that does not
    // complete, so it is the one another order has to end explicitly.
    for (const EntityId id : resolved)
    {
      static_cast<void>(_world.StopMining(id));
    }
  }
  else if (_command.type == CommandType::Mine)
  {
    // **ONLY WHAT CAN MINE TAKES THE ORDER** (`GameDesign.md` section 4): a design whose derived capacity is
    // zero is skipped and keeps whatever it was doing. The command is still accepted -- M2.8's mixed
    // selection sends the rest a separate move -- and the mining system does the travelling, so nothing
    // moves here.
    for (const WireIdentity wire : _command.selection)
    {
      const EntityId id = ResolveWireIdentity(_world, wire);
      const Entity* entity = _world.Find(id);
      if ((entity != nullptr) && (Derive(entity->design).oreCapacity > 0))
      {
        static_cast<void>(_world.OrderMine(id, _command.TargetRock()));
      }
    }
  }
  else
  {
    // M0 has no weapons. ADR-004's fire resolution is M3's, and an Attack that reaches here moves
    // nothing rather than pretending -- the command is still ACCEPTED, because the host understood
    // it and the acknowledgment must advance or the client repeats it forever.
  }

  m_lastApplied[_player] = _command.sequence;
  m_hasApplied[_player] = true;
  return CommandRejection::None;
}

std::size_t CommandIntake::ApplyPacket(World& _world, BuildSystem& _build, const CommandPacket& _packet) noexcept
{
  std::size_t applied = 0;
  for (const Command& command : _packet.commands)
  {
    if (Apply(_world, _build, _packet.player, command) == CommandRejection::None)
    {
      ++applied;
    }
  }
  return applied;
}

std::uint16_t CommandIntake::LastAppliedSequence(PlayerId _player) const noexcept
{
  if ((_player == NO_PLAYER) || (_player > MAX_PLAYERS))
  {
    return 0;
  }
  return m_lastApplied[_player];
}

} // namespace Outpost

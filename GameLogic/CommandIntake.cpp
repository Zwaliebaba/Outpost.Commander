#include "pch.h"

#include "CommandIntake.h"

#include <vector>

namespace Outpost
{

EntityId ResolveWireIdentity(const World& _world, std::uint16_t _wireIdentity) noexcept
{
  const std::uint16_t index = IndexOf(_wireIdentity);
  if (!_world.IsSlotAlive(index))
  {
    return NO_ENTITY;
  }

  const EntityId stored = _world.EntityInSlot(index).id;

  // Six bits is all the wire carries, so this compares six bits. The store's generation is wider
  // and that is deliberate (R16 does not owe the wire its precision); what matters here is that a
  // sender naming a slot's previous occupant is refused.
  if (GenerationOf(_wireIdentity) != (stored.generation & WIRE_GENERATION_MASK))
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

  // THE STATION'S TWO ORDERS TOUCH NO ENTITY, so every check below this belongs to the other two.
  // The acknowledgment still advances on a refusal, for the reason an Attack that resolves nothing
  // does: the host UNDERSTOOD the order, and a sequence that did not advance would have the client
  // repeat it forever.
  if (!ActsOnSelection(_command.type))
  {
    const bool ordered = (_command.type == CommandType::Build)
                           ? (_build.Start(_world, _player, static_cast<DesignId>(_command.TargetDesign())) == BuildRejection::None)
                           : _build.Cancel(_player);

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
  for (const std::uint16_t wire : _command.selection)
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

  if (_command.type == CommandType::MoveTo)
  {
    // **ONE RING SLOT EACH, RATHER THAN FIFTY SHIPS ON ONE POINT** (Q19, M1.7). The whole selection
    // goes to `RingAssignment` together, because a slot depends on who else is in the order -- which
    // is why this is one call and not a loop.
    std::vector<EntityId> resolved;
    resolved.reserve(_command.selection.size());
    for (const std::uint16_t wire : _command.selection)
    {
      resolved.push_back(ResolveWireIdentity(_world, wire));
    }
    static_cast<void>(OrderFleetTo(_world, resolved, target));
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

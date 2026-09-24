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
    // **THE PLAYER'S OWN SHIPYARD SETS THE RATE** (M2.12): the item starts at it, and since M3.8b (Q77's first item)
    // `BuildSystem::Advance` follows it as it moves -- a shipyard finished or lost mid-build changes this item too.
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

  // **A DEAD SHIP IS SKIPPED, NOT REFUSED** (Q24 as amended after the 2026-09-23 review, M4). The client
  // learns of a death 50 to 150 ms after the host, so a retreat tapped during a raid names a ship that has
  // just died -- and refusing the whole order for it would leave the survivors unordered when the order
  // matters most. A dead ship cannot be ordered, so dropping it half-applies nothing. An identity whose
  // slot has been reused resolves to nothing too, and is skipped for the same reason: the sender meant
  // the ship that died, not whatever took its slot.
  //
  // **A FOREIGN IDENTITY STILL REFUSES THE WHOLE ORDER**, resolved before anything is acted on: that is not
  // a race a client can lose, it is a client that is wrong, and applying the rest would leave the match in a
  // state no sequence number describes.
  std::vector<EntityId> live;
  live.reserve(_command.selection.size());
  CommandRejection refusal = CommandRejection::None;
  for (const WireIdentity wire : _command.selection)
  {
    const EntityId resolved = ResolveWireIdentity(_world, wire);
    if (!resolved.IsValid())
    {
      continue;
    }
    const Entity* entity = _world.Find(resolved);
    if ((entity == nullptr) || (entity->owner != _player))
    {
      refusal = CommandRejection::NotOwned;
      break;
    }
    live.push_back(resolved);
  }

  // BOUNDED AT THE SENDER'S OWN ENTITY COUNT, which is the check that turns the amplification into a
  // refused order -- **counted over the live identities**, so a selection that shrank by the ships that
  // died since the client saw them is not mistaken for a long one. Resolving costs one lookup an identity;
  // what the bound protects is the ring assignment below.
  if ((refusal == CommandRejection::None) && (live.size() > _world.OwnedCount(_player)))
  {
    refusal = CommandRejection::SelectionTooLong;
  }

  // A ROCK THE FIELD HAS, before anything is acted on -- the same all-or-nothing rule as the identities.
  if ((refusal == CommandRejection::None) && (_command.type == CommandType::Mine) && (_command.TargetRock() >= _world.Field().size()))
  {
    refusal = CommandRejection::NoSuchRock;
  }

  // AN ENEMY TO ATTACK, before anything is acted on (M3.2, Q67): alive, somebody's, and not the sender's.
  EntityId attackTarget = NO_ENTITY;
  if ((refusal == CommandRejection::None) && (_command.type == CommandType::Attack))
  {
    attackTarget = ResolveWireIdentity(_world, _command.TargetEntity());
    const Entity* victim = _world.Find(attackTarget);
    if ((victim == nullptr) || (victim->owner == NO_PLAYER) || (victim->owner == _player))
    {
      refusal = CommandRejection::NoSuchTarget;
    }
  }

  // **EVERY REFUSAL PAST THE SEQUENCE CHECK IS ACKNOWLEDGED** (Q24 as amended, M4), as a station order's
  // already was: the host understood the order and will never apply it, so a sequence that did not advance
  // would have the client repeat it until its resend gave up -- and hold every later order behind it.
  if (refusal != CommandRejection::None)
  {
    m_lastApplied[_player] = _command.sequence;
    m_hasApplied[_player] = true;
    return refusal;
  }

  // Clamped, although a wire target cannot leave the play area -- see ClampToPlayArea for why it
  // is written anyway.
  const Neuron::Vec2 target =
    ClampToPlayArea(Neuron::Vec2{.x = DequantizePosition(_command.targetX), .y = DequantizePosition(_command.targetY)});

  if (_command.type == CommandType::MoveTo)
  {
    // **ONE RING SLOT EACH, RATHER THAN FIFTY SHIPS ON ONE POINT** (Q19, M1.7). The whole selection
    // goes to `RingAssignment` together, because a slot depends on who else is in the order -- which
    // is why this is one call and not a loop. **Only the live ships**, so a ship that died does not hold
    // a slot the survivors would otherwise take.
    static_cast<void>(OrderFleetTo(_world, live, target));

    // **A MOVE ENDS A MINE ORDER AND KEEPS THE CARGO** (M2.6). The standing order is the one that does not
    // complete, so it is the one another order has to end explicitly. **And it ends an attack order** (M3.2).
    for (const EntityId id : live)
    {
      static_cast<void>(_world.StopMining(id));
      static_cast<void>(_world.StopAttack(id));
    }
  }
  else if (_command.type == CommandType::Mine)
  {
    // **ONLY WHAT CAN MINE TAKES THE ORDER** (`GameDesign.md` section 4): a design whose derived capacity is
    // zero is skipped and keeps whatever it was doing. The command is still accepted -- M2.8's mixed
    // selection sends the rest a separate move -- and the mining system does the travelling, so nothing
    // moves here.
    for (const EntityId id : live)
    {
      const Entity* entity = _world.Find(id);
      if ((entity != nullptr) && (Derive(entity->design).oreCapacity > 0))
      {
        static_cast<void>(_world.OrderMine(id, _command.TargetRock()));
      }
    }
  }
  else if (_command.type == CommandType::Attack)
  {
    // **A STANDOFF ARC AROUND THE TARGET** (M3.2, Q67), as one order and one group. What cannot fight -- a miner
    // in the selection -- is left doing what it was doing.
    static_cast<void>(OrderAttack(_world, live, attackTarget, _world.NewOrderGroup()));
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

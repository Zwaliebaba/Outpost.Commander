#include "pch.h"

#include "ModulePlacement.h"

#include <cmath>

namespace Outpost
{

void ModuleArming::Toggle(DesignId _design) noexcept
{
  if (m_armed && (m_design == _design))
  {
    m_armed = false;
    return;
  }
  m_design = _design;
  m_armed = true;
}

std::size_t OwnDepotCount(std::span<const EntityRecord> _entities, PlayerId _player) noexcept
{
  std::size_t count = 0;
  for (const EntityRecord& record : _entities)
  {
    count +=
      ((record.owner == _player) && (record.designIdentity < Designs().size()) && IsDepot(static_cast<DesignId>(record.designIdentity)))
        ? 1
        : 0;
  }
  return count;
}

std::vector<PlacedModule> OwnModules(std::span<const EntityRecord> _entities, PlayerId _player)
{
  std::vector<PlacedModule> modules;
  for (const EntityRecord& record : _entities)
  {
    const auto design = static_cast<DesignId>(record.designIdentity);
    if ((_player != NO_PLAYER) && (OwnerOf(record) == _player) && IsModule(design))
    {
      modules.push_back(
        PlacedModule{.identity = record.identity,
                     .position = Neuron::Vec2{.x = DequantizePosition(record.positionX), .y = DequantizePosition(record.positionY)},
                     .design = design});
    }
  }
  return modules;
}

bool OwnStation(std::span<const EntityRecord> _entities, PlayerId _player, EntityRecord& _outStation) noexcept
{
  for (const EntityRecord& record : _entities)
  {
    if ((_player != NO_PLAYER) && (OwnerOf(record) == _player) && (static_cast<DesignId>(record.designIdentity) == DesignId::Station))
    {
      _outStation = record;
      return true;
    }
  }
  return false;
}

PlacementOutcome ResolvePlacementTap(const CameraPose& _pose, const HitTestRequest& _request, std::span<const EntityRecord> _entities,
                                     std::span<const RockPickPoint> _rocks, DesignId _armed)
{
  PlacementOutcome outcome;

  // WITH A SELECTION, SO EMPTY SPACE RESOLVES TO A POINT: the armed placement is what the tap is for, whatever
  // is selected.
  const std::vector<PickCandidate> candidates = CandidatesUnderTap(_pose, _request, _entities, _rocks);
  const TapOutcome tap = ResolveTap(_pose, _request.aspectRatio, _request.authoredX, _request.authoredY, _request.authoredWidth,
                                    _request.authoredHeight, candidates, true);

  if (tap.action == TapAction::Occupied)
  {
    if (tap.hit.tier == PickTier::OwnShip)
    {
      outcome.action = PlacementAction::FallThrough;
      return outcome;
    }

    // **AN OWN MODULE IS AN UPGRADE'S TARGET AND NOTHING ELSE'S** (Q57), and only for the level that upgrades it.
    if (tap.hit.tier == PickTier::OwnStructure)
    {
      for (const EntityRecord& record : _entities)
      {
        if ((record.identity == tap.hit.identity) && UpgradesTo(static_cast<DesignId>(record.designIdentity), _armed))
        {
          outcome.action = PlacementAction::Upgrade;
          outcome.module = record.identity;
          return outcome;
        }
      }
    }
    return outcome;
  }

  if ((tap.action != TapAction::MoveTo) || (!IsPlacedLevel(_armed) && !IsDepot(_armed)))
  {
    return outcome;
  }

  EntityRecord station{};
  if (!OwnStation(_entities, _request.player, station))
  {
    return outcome;
  }

  // Through the wire's grid and back, so this judges the point the host will receive.
  const auto fixedX = static_cast<Neuron::Fixed>(tap.worldX * static_cast<float>(Neuron::FIXED_ONE));
  const auto fixedY = static_cast<Neuron::Fixed>(tap.worldY * static_cast<float>(Neuron::FIXED_ONE));
  const Neuron::Vec2 site{.x = DequantizePosition(QuantizePosition(fixedX)), .y = DequantizePosition(QuantizePosition(fixedY))};
  const Neuron::Vec2 stationPosition{.x = DequantizePosition(station.positionX), .y = DequantizePosition(station.positionY)};

  // M3.9: A DEPOT IS JUDGED BY ITS OWN RULE (Q69) -- every station, the field, this player's depots -- with the same
  // function the host uses. What the client cannot see is what is queued; the host counts that, and decides.
  if (IsDepot(_armed))
  {
    std::vector<Neuron::Vec2> stations;
    std::vector<Neuron::Vec2> depots;
    for (const EntityRecord& record : _entities)
    {
      const Neuron::Vec2 position{.x = DequantizePosition(record.positionX), .y = DequantizePosition(record.positionY)};
      if (record.designIdentity == static_cast<std::uint8_t>(DesignId::Station))
      {
        stations.push_back(position);
      }
      else if ((record.designIdentity < Designs().size()) && IsDepot(static_cast<DesignId>(record.designIdentity)) &&
               (record.owner == _request.player))
      {
        depots.push_back(position);
      }
    }
    std::vector<Placement> field;
    for (const RockPickPoint& rock : _rocks)
    {
      field.push_back(
        Placement{.kind = PlacedKind::Asteroid,
                  .position = Neuron::Vec2{.x = static_cast<Neuron::Fixed>(rock.worldX * static_cast<float>(Neuron::FIXED_ONE)),
                                           .y = static_cast<Neuron::Fixed>(rock.worldY * static_cast<float>(Neuron::FIXED_ONE))}});
    }
    outcome.depotFault = CheckDepotSite(stations, field, depots, site, _armed);
    if (outcome.depotFault == DepotSiteFault::None)
    {
      outcome.action = PlacementAction::Place;
      outcome.site = site;
    }
    return outcome;
  }

  const std::vector<PlacedModule> modules = OwnModules(_entities, _request.player);
  const ModuleSiteVerdict verdict = CheckModuleSite(stationPosition, DesignId::Station, modules, site, _armed);
  outcome.fault = verdict.fault;
  if (verdict.Legal())
  {
    outcome.action = PlacementAction::Place;
    outcome.site = site;
  }
  return outcome;
}

Command BuildPlaceModuleCommand(std::uint16_t _sequence, const Neuron::Vec2& _site, DesignId _design) noexcept
{
  Command command;
  command.sequence = _sequence;
  command.type = CommandType::PlaceModule;
  command.targetX = QuantizePosition(_site.x);
  command.targetY = QuantizePosition(_site.y);
  command.placedDesign = static_cast<std::uint8_t>(_design);
  return command;
}

Command BuildUpgradeModuleCommand(std::uint16_t _sequence, WireIdentity _module, DesignId _level) noexcept
{
  Command command;
  command.sequence = _sequence;
  command.type = CommandType::UpgradeModule;
  command.AimAtUpgrade(_module, _level);
  return command;
}

std::vector<HudRect> PlacementRingSquares(const CameraPose& _pose, float _aspectRatio, float _authoredWidth, float _authoredHeight,
                                          const Neuron::Vec2& _center)
{
  constexpr float TWO_PI = 6.28318530718f;
  constexpr float STEP_PIXELS = 2.0f;
  constexpr std::int32_t HALF_STROKE = PLACEMENT_RING_STROKE_PIXELS / 2;

  const float centerX = static_cast<float>(_center.x) / static_cast<float>(Neuron::FIXED_ONE);
  const float centerY = static_cast<float>(_center.y) / static_cast<float>(Neuron::FIXED_ONE);
  const auto radius = static_cast<float>(MODULE_BUILD_RADIUS_UNITS);

  const auto toAuthored = [&](float _angle, float& _outX, float& _outY)
  {
    float screenX = 0.0f;
    float screenY = 0.0f;
    if (!PlaneToScreen(_pose, _aspectRatio, centerX + (radius * std::cos(_angle)), centerY + (radius * std::sin(_angle)), screenX, screenY))
    {
      return false;
    }
    // The y axis flips: the projection counts up and authored pixels count down.
    _outX = ((screenX + 1.0f) * 0.5f) * _authoredWidth;
    _outY = ((1.0f - screenY) * 0.5f) * _authoredHeight;
    return true;
  };

  std::vector<HudRect> squares;
  // EVERY OTHER SEGMENT, which is the handoff's 24 of 48 and what keeps the ring from reading as the selection
  // circle (`design_handoff_hud`, its second load-bearing rule).
  for (std::size_t segment = 0; segment < PLACEMENT_RING_SEGMENTS; segment += 2)
  {
    const float from = (TWO_PI * static_cast<float>(segment)) / static_cast<float>(PLACEMENT_RING_SEGMENTS);
    const float to = (TWO_PI * static_cast<float>(segment + 1)) / static_cast<float>(PLACEMENT_RING_SEGMENTS);
    float fromX = 0.0f;
    float fromY = 0.0f;
    float toX = 0.0f;
    float toY = 0.0f;
    if (!toAuthored(from, fromX, fromY) || !toAuthored(to, toX, toY))
    {
      continue;
    }

    const float length = std::sqrt(((toX - fromX) * (toX - fromX)) + ((toY - fromY) * (toY - fromY)));
    // Bounded, so a camera pressed against the ring cannot turn one dash into thousands of squares.
    const auto steps = static_cast<std::int32_t>(std::fmin(std::ceil(length / STEP_PIXELS), 256.0f));
    for (std::int32_t step = 0; step <= steps; ++step)
    {
      const float along = (steps == 0) ? 0.0f : (static_cast<float>(step) / static_cast<float>(steps));
      const float x = fromX + ((toX - fromX) * along);
      const float y = fromY + ((toY - fromY) * along);
      squares.push_back(HudRect{static_cast<std::int32_t>(std::lround(x)) - HALF_STROKE,
                                static_cast<std::int32_t>(std::lround(y)) - HALF_STROKE, PLACEMENT_RING_STROKE_PIXELS,
                                PLACEMENT_RING_STROKE_PIXELS});
    }
  }
  return squares;
}

} // namespace Outpost

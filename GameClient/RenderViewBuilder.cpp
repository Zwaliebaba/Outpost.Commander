#include "pch.h"

#include "RenderViewBuilder.h"

#include "Placement.h"

#include "FixedPoint.h"

#include <algorithm>
#include <cmath>

namespace Outpost
{

namespace
{

/// A wire position as world units. GameShared/Records.h quantises to a quarter of a world unit.
[[nodiscard]] float WorldFromWire(std::int32_t _wireUnits) noexcept
{
  return Neuron::WorldUnitsOfSubunits(_wireUnits * SUBUNITS_PER_WIRE_UNIT);
}

/// A wire height as WHOLE world units, for the terrain flatten of m1-vertical-slice/K6.
///
/// The host writes mean * SUBUNITS_PER_WORLD_UNIT for a mean in whole world units, and the wire
/// holds a quarter of one, so the wire value is exactly four times the mean and this recovers it
/// without loss. SubunitsFromWire would answer the same, because the half-bucket it adds is an
/// eighth of a world unit and the divide floors it away - measured, not assumed, and a mutation to
/// it is caught by no test. The exact divide is kept because it says what is meant: a flatten is a
/// whole number of world units, not a position to be un-quantized.
///
/// THE MULTIPLY IS 64-BIT AND THE CAST IS NOT DECORATION. Both operands are int32, so the product
/// would be computed in int and only then widened to FloorDiv's parameter - which is what
/// bugprone-implicit-widening-of-multiplication-result refuses, and rightly: this height is small,
/// and the identical shape over a position in subunits would not be.
[[nodiscard]] std::int32_t WorldUnitsOfWireHeight(std::int32_t _wireUnits) noexcept
{
  return static_cast<std::int32_t>(
    Neuron::FloorDiv(static_cast<std::int64_t>(_wireUnits) * SUBUNITS_PER_WIRE_UNIT, Neuron::SUBUNITS_PER_WORLD_UNIT));
}

/// Where a thing placed on the grid stands: the CENTRE of its footprint, because a model is
/// authored about its own origin and a structure whose origin sat on a corner would be drawn a
/// footprint's worth away from where the simulation says it is.
[[nodiscard]] float WorldFromCells(std::uint32_t _cell, std::uint32_t _footprintCells) noexcept
{
  return static_cast<float>(_cell * Neuron::WORLD_UNITS_PER_CELL) +
         static_cast<float>(_footprintCells * Neuron::WORLD_UNITS_PER_CELL) * 0.5f;
}

[[nodiscard]] bool Contains(std::span<const std::uint32_t> _ids, std::uint32_t _id) noexcept
{
  return std::find(_ids.begin(), _ids.end(), _id) != _ids.end();
}

} // namespace

RenderViewBuilder::RenderViewBuilder(const ContentTree& _content, const ModelComposer& _composer, const RenderViewSettings& _settings)
  : m_content(&_content),
    m_composer(&_composer),
    m_settings(_settings)
{
}

void RenderViewBuilder::Settings(const RenderViewSettings& _settings)
{
  m_settings = _settings;
  ForgetTerrain();
}

void RenderViewBuilder::ForgetTerrain()
{
  m_flattened.clear();
}

std::uint32_t LevelUnderFlattens(Landscape& _landscape, std::span<const Neuron::TerrainFlatten> _flattens)
{
  std::uint32_t applied = 0;
  if (!_landscape.Created())
  {
    return applied;
  }
  for (const Neuron::TerrainFlatten& flatten : _flattens)
  {
    const Footprint footprint{flatten.cellX, flatten.cellY, flatten.cellsX, flatten.cellsY};
    HeightDelta delta = FlattenDelta(_landscape, footprint);
    if (delta.heights.empty())
    {
      continue; // Off the landscape: nothing to level.
    }
    // FlattenDelta chose the rectangle and filled it with THIS landscape's mean; the header says
    // why the height is replaced by the one the host levelled to rather than kept.
    std::fill(delta.heights.begin(), delta.heights.end(), static_cast<std::int16_t>(flatten.heightWorldUnits));
    applied += _landscape.ApplyDelta(delta) ? 1 : 0;
  }
  return applied;
}

void ChunksOfFootprint(std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _footprintCellsX, std::uint32_t _footprintCellsY,
                       std::uint32_t _cellsPerSide, std::uint32_t _chunkCells, std::vector<std::uint32_t>& _outChunks)
{
  // THE FIRST TWO ARE LOAD-BEARING and the last two are not, which is worth stating because
  // mutation testing removed the off-landscape test and failed nothing: a cell past the edge gives
  // a first chunk beyond the clamped last one, so the loops below simply do not run. It stays
  // because "a cell that is not on the landscape names no chunk" is the rule, and leaving it to
  // fall out of two clamps and a loop bound is how a rule gets lost the next time one of them
  // moves. The zero tests ARE load-bearing: a chunk size of zero divides by zero, and a landscape
  // of zero cells underflows `_cellsPerSide - 1` into a very large clamp.
  if (_chunkCells == 0 || _cellsPerSide == 0 || _cellX >= _cellsPerSide || _cellY >= _cellsPerSide)
  {
    return;
  }
  const std::uint32_t chunksPerSide = (_cellsPerSide + _chunkCells - 1) / _chunkCells;
  // The footprint's cells INCLUSIVE OF THE LAST. A footprint of zero cells is treated as one,
  // because a structure standing on no ground is a content fault and drawing nothing for it here
  // would hide it behind a chunk list that was merely short.
  const std::uint32_t firstX = _cellX / _chunkCells;
  const std::uint32_t firstY = _cellY / _chunkCells;
  const std::uint32_t lastX = std::min(_cellX + std::max(_footprintCellsX, 1u) - 1, _cellsPerSide - 1) / _chunkCells;
  const std::uint32_t lastY = std::min(_cellY + std::max(_footprintCellsY, 1u) - 1, _cellsPerSide - 1) / _chunkCells;
  for (std::uint32_t chunkY = firstY; chunkY <= lastY && chunkY < chunksPerSide; ++chunkY)
  {
    for (std::uint32_t chunkX = firstX; chunkX <= lastX && chunkX < chunksPerSide; ++chunkX)
    {
      _outChunks.push_back(chunkY * chunksPerSide + chunkX);
    }
  }
}

void RenderViewBuilder::MarkChunks(const Flattened& _footprint, std::vector<std::uint32_t>& _outChunks) const
{
  ChunksOfFootprint(_footprint.cellX, _footprint.cellY, _footprint.footprintCellsX, _footprint.footprintCellsY, m_settings.cellsPerSide,
                    m_settings.chunkCells, _outChunks);
}

/// Turns this frame's Shot events into shots to draw, one each, and forgets the ones whose weapon
/// row says nothing about what a shot looks like.
///
/// A SHOT WHOSE SHOOTER IS NOT DRAWN IS NOT DRAWN EITHER. The muzzle comes from the composition of
/// the shooter, so a commander who cannot see the firing device sees no tracer leave it - which is
/// the §5.2 rule holding without a second test for it: the host does not send him the event at all
/// (GameLogic/Host.cpp filters events by what the seat can see), and if one arrived anyway there is
/// nowhere for it to start.
void RenderViewBuilder::TakeShots(const Replica& _replica, const std::map<std::uint32_t, ComposedMuzzle>& _muzzles)
{
  // ONCE A FRAME AND NOT ONCE A BUILD. Replica::Events() holds the NEWEST frame's and is replaced
  // by every Apply - it is not emptied when no frame arrives - and a client draws many frames
  // inside one publish interval, so a build that took them again would put a second shot in the
  // air for every drawn frame between one frame and the next. The replica's own sequence says
  // whether these are the same events as last time; it is what the client acknowledged, so it
  // moves exactly when Events() is replaced.
  if (_replica.NewestSequence() == m_shotsThroughSequence)
  {
    return;
  }
  m_shotsThroughSequence = _replica.NewestSequence();
  for (const Event& event : _replica.Events())
  {
    if (event.kind != EventKind::Shot)
    {
      continue;
    }
    const auto muzzle = _muzzles.find(event.source);
    if (muzzle == _muzzles.end())
    {
      continue; // Fired by something this commander is not drawing.
    }
    if (muzzle->second.moduleRow >= m_content->components.modules.size())
    {
      continue;
    }
    const ModuleDesc& weapon = m_content->components.modules[muzzle->second.moduleRow];
    if (weapon.projectileModel.empty() || weapon.projectileLifetimeTicks == 0)
    {
      continue; // A weapon that shows nothing in flight, which is a legal row (GameShared/ComponentDesc.h).
    }
    const std::uint32_t model = m_composer->ProjectileModel(muzzle->second.moduleRow);
    if (model == ModelComposer::NO_MODEL)
    {
      ++m_lastUnresolved; // The row names a projectile the tree does not hold; C1's validator says so too.
      continue;
    }
    FlyingShot shot{};
    // FROM THE EVENT'S OWN TICK, which is the acceptance line's "not drawn a frame early or late":
    // a frame covers an interval and the event says where inside it the trigger was pulled.
    shot.firedAt = RenderTimeOfTick(event.tick);
    shot.expiresAt = shot.firedAt + RenderTimeOfTick(weapon.projectileLifetimeTicks);
    shot.modelIndex = model;
    shot.scale = m_composer->ProjectileScale(muzzle->second.moduleRow);
    shot.fromX = muzzle->second.x;
    shot.fromY = muzzle->second.y;
    shot.fromZ = muzzle->second.z;
    shot.toX = WorldFromWire(event.x);
    shot.toY = WorldFromWire(event.y);
    shot.toZ = WorldFromWire(event.z);
    m_shots.push_back(shot);
  }
}

/// Draws every shot still in the air at this render time and drops the rest.
///
/// THE HEADING IS THE DIRECTION OF TRAVEL, so a shell points where it is going rather than where
/// its model happens to face. A shot that goes straight up or is fired at its own muzzle has no
/// direction, and atan2 of two zeroes is zero rather than a fault, so that case needs no branch.
void RenderViewBuilder::DrawShots(std::int64_t _renderTime, Neuron::RenderView& _outView)
{
  std::erase_if(m_shots, [_renderTime](const FlyingShot& _shot) { return _renderTime >= _shot.expiresAt; });
  for (const FlyingShot& shot : m_shots)
  {
    if (_renderTime < shot.firedAt)
    {
      continue; // Fired later in the interval than this frame is drawing; it is on its way.
    }
    const std::int64_t span = shot.expiresAt - shot.firedAt;
    const float travelled = span <= 0 ? 1.0f : static_cast<float>(_renderTime - shot.firedAt) / static_cast<float>(span);
    ObjectAppearance appearance{};
    const Pose pose{shot.fromX + (shot.toX - shot.fromX) * travelled, shot.fromY + (shot.toY - shot.fromY) * travelled,
                    shot.fromZ + (shot.toZ - shot.fromZ) * travelled, std::atan2(shot.toX - shot.fromX, shot.toZ - shot.fromZ)};
    m_composer->ComposeSingle(shot.modelIndex, shot.scale, pose, appearance, Neuron::RenderInstanceKind::Projectile, _outView.instances);
  }
}

void RenderViewBuilder::Build(const Replica& _replica, std::int64_t _renderTime, std::span<const std::uint32_t> _selectedIds,
                              Neuron::RenderView& _outView, PickSet& _outPicks)
{
  _outView.instances.clear();
  _outView.changedChunks.clear();
  _outView.flattens.clear();
  _outPicks.candidates.clear();
  m_lastUnresolved = 0;

  // Where each visible device's first weapon points from, for the shots below. Scratch: a shot is
  // drawn from where its shooter is NOW, and a shooter that has gone out of sight draws none.
  std::map<std::uint32_t, ComposedMuzzle> firedFrom;
  std::vector<ComposedMuzzle> muzzles;

  // ── Devices ────────────────────────────────────────────────────────────────────────────────
  for (const auto& [id, device] : _replica.Devices())
  {
    const DesignState* design = _replica.Designs().Find(device.state.seat, device.state.design);
    if (design == nullptr)
    {
      // The host sends a design with the first device of it a commander sees, so this is a frame
      // arriving before the one that carried it - rare, and a device drawn as nothing for a frame
      // is better than one drawn as whatever row zero happens to be.
      ++m_lastUnresolved;
      continue;
    }
    const Pose pose = Evaluate(device.motion, _renderTime);
    ObjectAppearance appearance{};
    appearance.colorIndex = device.state.seat;
    appearance.rankBadge = device.state.rank;
    appearance.selected = Contains(_selectedIds, id);
    // THE MUZZLES ARE TAKEN HERE OR NOWHERE (m1-vertical-slice/C8). A Shot event names the shooter
    // and nothing else about where the shot leaves from, and only the composition knows: the
    // module's MarkerMuzzle carried through the module's transform and then the device's. Composing
    // the device twice to get it would be the whole model tree walked twice a frame.
    muzzles.clear();
    m_composer->ComposeDevice(*design, pose, appearance, _outView.instances, &muzzles);
    if (!muzzles.empty())
    {
      // THE FIRST MOUNTED WEAPON, AND THIS IS A SIMPLIFICATION WITH A NAME. GameShared/Records.h's Event
      // carries the shooter, the target and a point; it does not carry WHICH module fired, and
      // adding a field is a wire change this task does not own. So a device with two weapons of
      // different rows draws both their shots as the first one's projectile. Every design the M1
      // tables can build carries one weapon, so the case does not arise yet - and when it does, the
      // fix is a field on the event rather than a guess here.
      firedFrom.insert_or_assign(id, muzzles.front());
    }

    PickCandidate candidate{};
    candidate.id = id;
    candidate.x = pose.x;
    candidate.y = pose.y;
    candidate.z = pose.z;
    candidate.radius = m_composer->ChassisRadius(design->chassis);
    candidate.seat = device.state.seat;
    candidate.kind = ObjectKind::Device;
    _outPicks.candidates.push_back(candidate);
  }

  // ── Structures, and the terrain they flattened ─────────────────────────────────────────────
  std::map<std::uint32_t, Flattened> standing;
  for (const auto& [id, structure] : _replica.Structures())
  {
    const StructureState& state = structure.state;
    const StructureDesc* row =
      state.design < m_content->structures.structures.size() ? &m_content->structures.structures[state.design] : nullptr;
    const std::uint32_t footprintX = row != nullptr ? row->footprintCellsX : 1;
    const std::uint32_t footprintY = row != nullptr ? row->footprintCellsY : 1;
    const Pose pose{WorldFromCells(state.cellX, footprintX), WorldFromWire(state.y), WorldFromCells(state.cellY, footprintY), 0.0f};

    ObjectAppearance appearance{};
    appearance.colorIndex = state.seat;
    appearance.selected = Contains(_selectedIds, id);
    m_composer->ComposeStructure(state, pose, appearance, _outView.instances);
    if (row == nullptr)
    {
      ++m_lastUnresolved;
    }

    // A GHOST IS NOT PICKED. It is what the commander remembers of a building he cannot see now,
    // so a click on one would select something that may not be there - and §5's inspection is for
    // what is visible. It is still drawn, and the fog pass darkens it like everything else.
    if (!structure.ghost)
    {
      PickCandidate candidate{};
      candidate.id = id;
      candidate.x = pose.x;
      candidate.y = pose.y;
      candidate.z = pose.z;
      candidate.radius = m_composer->StructureRadius(state.design);
      candidate.seat = state.seat;
      candidate.kind = ObjectKind::Structure;
      _outPicks.candidates.push_back(candidate);
    }

    standing.emplace(id, Flattened{state.cellX, state.cellY, footprintX, footprintY, state.y});
  }

  // What changed since the last frame: a structure that arrived, one whose flattened height moved,
  // and one that is gone - a razed building leaves its ground as it left it, and the chunk has to
  // be rebuilt either way.
  for (const auto& [id, flattened] : standing)
  {
    const auto was = m_flattened.find(id);
    if (was == m_flattened.end() || was->second != flattened)
    {
      MarkChunks(flattened, _outView.changedChunks);
      // And the flatten itself, for whoever holds a landscape (m1-vertical-slice/K6).
      _outView.flattens.push_back(
        {flattened.cellX, flattened.cellY, flattened.footprintCellsX, flattened.footprintCellsY, WorldUnitsOfWireHeight(flattened.y)});
    }
  }
  for (const auto& [id, flattened] : m_flattened)
  {
    if (!standing.contains(id))
    {
      MarkChunks(flattened, _outView.changedChunks);
    }
  }
  m_flattened = std::move(standing);
  // Ascending and without repeats, which is what a consumer of a change list expects and what two
  // structures on one chunk would otherwise break.
  std::sort(_outView.changedChunks.begin(), _outView.changedChunks.end());
  _outView.changedChunks.erase(std::unique(_outView.changedChunks.begin(), _outView.changedChunks.end()), _outView.changedChunks.end());

  // ── Wrecks ─────────────────────────────────────────────────────────────────────────────────
  for (const auto& [id, wreck] : _replica.Wrecks())
  {
    const WreckState& state = wreck.state;
    std::uint32_t model = ModelComposer::NO_MODEL;
    float scale = 1.0f;
    if (static_cast<ObjectKind>(state.origin) == ObjectKind::Structure)
    {
      model = m_composer->StructureModel(state.design);
      scale = m_composer->StructureScale(state.design);
    }
    else if (const DesignState* design = _replica.Designs().Find(state.seat, state.design); design != nullptr)
    {
      // A DEVICE'S WRECK IS ITS CHASSIS AND NOT ITS WHOLE TREE. What is left after §7's explosion
      // is the hull; the drives and the modules came apart with everything else.
      model = m_composer->ChassisModel(design->chassis);
      scale = m_composer->ChassisScale(design->chassis);
    }
    if (model == ModelComposer::NO_MODEL)
    {
      ++m_lastUnresolved;
      continue;
    }
    ObjectAppearance appearance{};
    appearance.colorIndex = state.seat;
    const Pose pose{WorldFromWire(state.x), WorldFromWire(state.y), WorldFromWire(state.z), RadiansOfWireHeading(state.heading)};
    m_composer->ComposeSingle(model, scale, pose, appearance, Neuron::RenderInstanceKind::Wreck, _outView.instances);
  }

  // ── Shots in flight ────────────────────────────────────────────────────────────────────────
  // AFTER the devices, because a shot leaves a muzzle and the muzzles were worked out up there.
  TakeShots(_replica, firedFrom);
  DrawShots(_renderTime, _outView);

  // ── The fog, straight through ──────────────────────────────────────────────────────────────
  // The replica's own bytes without a table: NeuronCore/RenderView.h's FogShade and GameShared/FogGrid.h's
  // FogState carry the same three values in the same order, and that header says so.
  const std::span<const FogState> fog = _replica.Fog();
  _outView.fog.cells.resize(fog.size());
  for (std::size_t index = 0; index < fog.size(); ++index)
  {
    _outView.fog.cells[index] = static_cast<std::uint8_t>(fog[index]);
  }
  const std::span<const std::uint32_t> changedRows = _replica.ChangedFogRows();
  _outView.fog.changedRows.assign(changedRows.begin(), changedRows.end());
  _outView.fog.cellsPerSide = _replica.FogCellsPerSide();
}

} // namespace Outpost

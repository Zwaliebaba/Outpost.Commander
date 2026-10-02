#include "pch.h"
#include "GameClient.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
constexpr auto MODELS_FILE = L"Models.json";
constexpr auto CAMERA_FILE = L"Camera.json";
// The texture of every particle: DeepSpaceOutpost's, a flat square with a brighter rim (ADR-026).
constexpr auto PARTICLE_SPRITE_FILE = L"Textures\\Particle.dds";
// The HUD's font: installed with Windows, so nothing ships (ADR-015).
constexpr std::wstring_view HUD_FONT = L"Segoe UI";

// One light from above and behind the default view's top-left, and how much of an object's color the unlit side keeps.
// Presentation, not tuning: the design asks only that the scene reads clearly (design §11).
constexpr DirectX::XMFLOAT3 TOWARD_LIGHT{-0.4f, 0.8f, 0.45f};
constexpr float AMBIENT = 0.3f;

// The grid covers the 2,000 m map (design §4) with a line every 100 m, each a pixel wide at any zoom, in the line art
// of the asteroids' ridges (ADR-028). It is a dim blue-gray, barely there, so the sky shows through (ADR-022) and the
// ridges stand out above it, and it is what shows the ground moving when the view pans.
constexpr float GRID_HALF_EXTENT_METERS = 1000.0f;
constexpr float GRID_SPACING_METERS = 100.0f;
constexpr DirectX::XMFLOAT4 GRID_COLOR{0.012f, 0.013f, 0.019f, 1.0f};

// Asteroids are drawn with three rock meshes, each 2 m long, so a rock's scale is its radius (ADR-011). They are
// low-poly on purpose, big facets with clear ridges for the edge lines (owner 2026-10-02, ADR-027). A
// rock takes the small mesh up to SMALL_ROCK_MAX_METERS of radius and the large one from LARGE_ROCK_MIN_METERS, so its
// facets come out about the same size on the screen whatever its size: an ore asteroid, 45 m, is medium, and a field
// mixes all three.
constexpr std::string_view ASTEROID_SET = "Asteroids";
constexpr std::array<std::string_view, 3> ROCK_MODELS{"Small", "Medium", "Large"};
constexpr float SMALL_ROCK_MAX_METERS = 35.0f;
constexpr float LARGE_ROCK_MIN_METERS = 55.0f;
// A field is a loose cluster of rocks inside its circle: one in the middle and a ring around it, each given as a
// distance from the center and a size, both in shares of the field's radius, turned by a fixed angle per rock. The
// server blocks the whole circle; this only has to read as a field (design §4).
constexpr float FIELD_CENTER_ROCK_SHARE = 0.45f;
constexpr int FIELD_RING_ROCKS = 6;
constexpr float FIELD_RING_DISTANCE_SHARE = 0.62f;
constexpr float FIELD_RING_ROCK_SHARE = 0.3f;
constexpr float FIELD_RING_START_RADIANS = 0.26f;
// A field is darker than an ore asteroid, so the two read apart.
constexpr float FIELD_SHADE = 0.7f;
// Every model, rock, ship and structure, also shows its edges as thin lines, for a vector look from the eighties (owner,
// 2026-10-02, ADR-027). Only the edges where its surface bends by more than CREASE_DEGREES are drawn: on low-poly
// models that is nearly every edge between two facets, and two triangles that lie almost flat read as one facet. The
// lines are lit as the model is, in its color made EDGE_BRIGHTNESS brighter, and the faces are drawn FILL_SHADE darker,
// so the lines stand out from the faces beside them on the lit side and the dark side alike. They are lifted
// EDGE_LIFT_SHARE of the mesh's size off the surface so that it does not hide them. The rocks are terrain, so their lines
// are only ROCK_EDGE_BRIGHTNESS brighter: near white at EDGE_BRIGHTNESS, they outshone both fleets (owner, 2026-10-02,
// ADR-028).
constexpr float CREASE_DEGREES = 10.0f;
constexpr float EDGE_BRIGHTNESS = 2.0f;
constexpr float ROCK_EDGE_BRIGHTNESS = 1.35f;
constexpr float FILL_SHADE = 0.65f;
constexpr float EDGE_LIFT_SHARE = 0.005f;

// A Mining Rig's feet are its lowest points out toward its rim, past this share of its half length from its middle, and
// within this share of its height of the lowest of them: its legs' tips, and not a drill hanging under its middle. The
// rig is lowered until every foot reaches the rock under it, so that it stands on its asteroid rather than floating over
// the rock's slopes (owner, 2026-10-02, ADR-027).
constexpr float RIG_FOOT_RADIUS_SHARE = 0.5f;
constexpr float RIG_FOOT_HEIGHT_SHARE = 0.05f;

// A beam is its shooter's side's color taken this share of the way to white, so that the player sees whose fire it is
// and it still reads as light (ADR-028).
constexpr float BEAM_WHITE_SHARE = 0.35f;

// The selection is a ring on the ground around each selected ship, green, or amber while attack-move waits for its
// click; a drag box is its outline on the ground. Both sit just above the grid so that they do not flicker with it.
constexpr float RING_INNER_RADIUS = 1.0f;
constexpr float RING_OUTER_RADIUS = 1.15f;
constexpr int RING_SEGMENTS = 48;
constexpr float RING_SIZE_PER_FOOTPRINT = 1.3f;
constexpr float OVERLAY_LIFT_METERS = 0.3f;
constexpr float DRAG_LINE_WIDTH_METERS = 1.5f;
constexpr DirectX::XMFLOAT4 SELECTION_COLOR{0.25f, 0.95f, 0.35f, 1.0f};
constexpr DirectX::XMFLOAT4 ATTACK_MOVE_COLOR{1.0f, 0.65f, 0.1f, 1.0f};
constexpr DirectX::XMFLOAT4 DRAG_BOX_COLOR{0.25f, 0.95f, 0.35f, 1.0f};

// A damaged ship's or structure's bar floats above it, as long as its footprint is wide and just off it toward -z: green
// above half its hit points, then amber, then red, over a full-length bar in its side's color darkened to
// HEALTH_BACK_SHADE, so that a bar says whose it is as well as how hurt (owner, 2026-10-02, ADR-028); dark gray for a
// side the data does not name. The gap is small, so that the bar reads as the ship's.
constexpr float HEALTH_BAR_HEIGHT_METERS = 10.0f;
constexpr float HEALTH_BAR_WIDTH_METERS = 2.5f;
constexpr float HEALTH_BAR_GAP_METERS = 1.5f;
constexpr DirectX::XMFLOAT4 HEALTH_BACK_COLOR{0.08f, 0.08f, 0.08f, 1.0f};
constexpr float HEALTH_BACK_SHADE = 0.3f;
constexpr DirectX::XMFLOAT4 HEALTH_GOOD_COLOR{0.2f, 0.85f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_HURT_COLOR{1.0f, 0.7f, 0.1f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_LOW_COLOR{0.95f, 0.2f, 0.15f, 1.0f};
constexpr float HEALTH_HURT_SHARE = 0.5f;
constexpr float HEALTH_LOW_SHARE = 0.25f;
// A structure under construction: its color darkens toward this share at the start, and a blue bar shows the share built,
// just beyond the health bar.
constexpr float UNBUILT_SHADE = 0.35f;
constexpr DirectX::XMFLOAT4 BUILD_BAR_COLOR{0.25f, 0.65f, 1.0f, 1.0f};
// The ghost: a flat disc of the footprint, a little above the ground.
constexpr DirectX::XMFLOAT4 GHOST_VALID_COLOR{0.2f, 0.75f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 GHOST_INVALID_COLOR{0.85f, 0.2f, 0.15f, 1.0f};
constexpr int DISC_SEGMENTS = 24;
// An exhaust is at its longest and brightest at this speed or faster (ADR-019): an Ion Medium's cruise, so that a fast
// design reads as fast and a Fusion Large, at 20 m/s, burns well short of it.
constexpr float EXHAUST_FULL_SPEED_METERS_PER_SECOND = 50.0f;
// A structure breaks into three sets of shards at once, as a DeepSpaceOutpost building does, and a ship into one
// (ADR-026).
constexpr int STRUCTURE_SHARD_COPIES = 3;

Neuron::ByteBuffer ReadAsset(const std::wstring& _fileName)
{
  Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(_fileName);
  if (bytes.empty())
    throw Neuron::Exception(std::format("The game asset Assets\\{} is missing or cannot be read.", winrt::to_string(_fileName)));
  return bytes;
}

std::string_view AsText(const Neuron::ByteBuffer& _bytes) noexcept
{
  return {reinterpret_cast<const char*>(_bytes.data()), _bytes.size()};
}

// A data file's loader names the member at fault; this adds which file it is in.
template <typename Fn> auto LoadDataFile(const wchar_t* _fileName, Fn _load)
{
  const Neuron::ByteBuffer bytes = ReadAsset(_fileName);
  try
  {
    return _load(AsText(bytes));
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("Assets\\{}: {}", winrt::to_string(_fileName), error.what()));
  }
}

// Lines along x and along z on the ground, every GRID_SPACING_METERS across the grid, as a line list for
// MeshPipeline::DrawLines, lit as the ground facing up.
Neuron::MeshData BuildGrid()
{
  Neuron::MeshData grid;
  const auto addLine = [&grid](float _x0, float _z0, float _x1, float _z1)
  {
    constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
    grid.indices.push_back(static_cast<std::uint32_t>(grid.vertices.size()));
    grid.vertices.push_back({{_x0, 0.0f, _z0}, UP});
    grid.indices.push_back(static_cast<std::uint32_t>(grid.vertices.size()));
    grid.vertices.push_back({{_x1, 0.0f, _z1}, UP});
  };

  const auto lineCount = static_cast<int>(std::lround(2.0f * GRID_HALF_EXTENT_METERS / GRID_SPACING_METERS));
  for (int line = 0; line <= lineCount; ++line)
  {
    const float at = -GRID_HALF_EXTENT_METERS + (static_cast<float>(line) * GRID_SPACING_METERS);
    addLine(at, -GRID_HALF_EXTENT_METERS, at, GRID_HALF_EXTENT_METERS);
    addLine(-GRID_HALF_EXTENT_METERS, at, GRID_HALF_EXTENT_METERS, at);
  }
  grid.boundsMin = {-GRID_HALF_EXTENT_METERS, 0.0f, -GRID_HALF_EXTENT_METERS};
  grid.boundsMax = {GRID_HALF_EXTENT_METERS, 0.0f, GRID_HALF_EXTENT_METERS};
  return grid;
}

// A flat ring of radius 1 on the ground, facing up.
Neuron::MeshData BuildRing()
{
  Neuron::MeshData ring;
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  for (int i = 0; i < RING_SEGMENTS; ++i)
  {
    const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / RING_SEGMENTS;
    ring.vertices.push_back({{RING_INNER_RADIUS * std::cos(angle), 0.0f, RING_INNER_RADIUS * std::sin(angle)}, UP});
    ring.vertices.push_back({{RING_OUTER_RADIUS * std::cos(angle), 0.0f, RING_OUTER_RADIUS * std::sin(angle)}, UP});
  }
  for (int i = 0; i < RING_SEGMENTS; ++i)
  {
    const auto inner = static_cast<std::uint32_t>(2 * i);
    const auto next = static_cast<std::uint32_t>(2 * ((i + 1) % RING_SEGMENTS));
    // The angle turns counterclockwise seen from above, so outer-then-inner of the next spoke is clockwise.
    for (const std::uint32_t corner : {inner, next + 1, inner + 1, inner, next, next + 1})
      ring.indices.push_back(corner);
  }
  ring.boundsMin = {-RING_OUTER_RADIUS, 0.0f, -RING_OUTER_RADIUS};
  ring.boundsMax = {RING_OUTER_RADIUS, 0.0f, RING_OUTER_RADIUS};
  return ring;
}

// A flat disc of radius 1 on the ground, facing up: a fan of triangles round its center.
Neuron::MeshData BuildDisc()
{
  Neuron::MeshData disc;
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  disc.vertices.push_back({{0.0f, 0.0f, 0.0f}, UP});
  for (int i = 0; i < DISC_SEGMENTS; ++i)
  {
    const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / DISC_SEGMENTS;
    disc.vertices.push_back({{std::cos(angle), 0.0f, std::sin(angle)}, UP});
  }
  for (int i = 0; i < DISC_SEGMENTS; ++i)
  {
    const auto rim = static_cast<std::uint32_t>(1 + i);
    const auto next = static_cast<std::uint32_t>(1 + ((i + 1) % DISC_SEGMENTS));
    // The angle turns counterclockwise seen from above, so center, next, this is clockwise and faces up.
    for (const std::uint32_t corner : {0u, next, rim})
      disc.indices.push_back(corner);
  }
  disc.boundsMin = {-1.0f, 0.0f, -1.0f};
  disc.boundsMax = {1.0f, 0.0f, 1.0f};
  return disc;
}

// A flat strip on the ground from x = 0 to 1 and z = -0.5 to 0.5, facing up.
Neuron::MeshData BuildStrip()
{
  Neuron::MeshData strip;
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  strip.vertices = {{{0.0f, 0.0f, 0.5f}, UP}, {{1.0f, 0.0f, 0.5f}, UP}, {{1.0f, 0.0f, -0.5f}, UP}, {{0.0f, 0.0f, -0.5f}, UP}};
  strip.indices = {0, 1, 2, 0, 2, 3};
  strip.boundsMin = {0.0f, 0.0f, -0.5f};
  strip.boundsMax = {1.0f, 0.0f, 0.5f};
  return strip;
}

// Scaled uniformly, turned to a heading counterclockwise from +x seen from above, and moved to a point.
DirectX::XMFLOAT4X4 WorldMatrix(const DirectX::XMFLOAT3& _position, float _headingRadians, float _scale) noexcept
{
  // XMMatrixRotationY turns +x toward -z for a positive angle, which is clockwise seen from above.
  const DirectX::XMMATRIX world = DirectX::XMMatrixScaling(_scale, _scale, _scale) * DirectX::XMMatrixRotationY(-_headingRadians) *
                                  DirectX::XMMatrixTranslation(_position.x, _position.y, _position.z);
  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, world);
  return result;
}

std::string MeshKey(std::string_view _set, std::string_view _model)
{
  return std::format("{}/{}", _set, _model);
}

// Which of ROCK_MODELS a rock of _radiusMeters is drawn with.
size_t RockIndex(float _radiusMeters) noexcept
{
  if (_radiusMeters <= SMALL_ROCK_MAX_METERS)
    return 0;
  return _radiusMeters < LARGE_ROCK_MIN_METERS ? 1 : 2;
}

// The rock mesh for a rock of _radiusMeters.
std::string_view RockModel(float _radiusMeters) noexcept
{
  return ROCK_MODELS[RockIndex(_radiusMeters)];
}

// The feet of a Mining Rig's mesh, fitted, as RIG_FOOT_RADIUS_SHARE and RIG_FOOT_HEIGHT_SHARE pick them.
std::vector<DirectX::XMFLOAT3> RigFeet(const Neuron::MeshData& _rig)
{
  const float rim = RIG_FOOT_RADIUS_SHARE * _rig.Extents().x / 2.0f;
  const auto outward = [rim](const Neuron::MeshVertex& _vertex) { return std::hypot(_vertex.position.x, _vertex.position.z) > rim; };
  float lowest = std::numeric_limits<float>::max();
  for (const Neuron::MeshVertex& vertex : _rig.vertices)
  {
    if (outward(vertex))
      lowest = std::min(lowest, vertex.position.y);
  }
  const float reach = lowest + (RIG_FOOT_HEIGHT_SHARE * _rig.Extents().y);
  std::vector<DirectX::XMFLOAT3> feet;
  for (const Neuron::MeshVertex& vertex : _rig.vertices)
  {
    if (outward(vertex) && vertex.position.y <= reach)
      feet.push_back(vertex.position);
  }
  return feet;
}

// A color times _scale, kept at most 1.
DirectX::XMFLOAT4 Shaded(const DirectX::XMFLOAT4& _color, float _scale) noexcept
{
  return {std::min(1.0f, _color.x * _scale), std::min(1.0f, _color.y * _scale), std::min(1.0f, _color.z * _scale), _color.w};
}

// The color a model is drawn in: its set's, times its tint, and darker while it is built (task 4.2). A ship has no tint
// and is always built, so it is its set's color.
DirectX::XMFLOAT4 ModelColor(const DirectX::XMFLOAT4& _setColor, float _tint, std::int32_t _builtPermille) noexcept
{
  const float built = static_cast<float>(_builtPermille) / static_cast<float>(Outpost::PERMILLE);
  const float shade = _tint * (UNBUILT_SHADE + ((1.0f - UNBUILT_SHADE) * built));
  return {std::min(1.0f, _setColor.x * shade), std::min(1.0f, _setColor.y * shade), std::min(1.0f, _setColor.z * shade), _setColor.w};
}
} // namespace

Outpost::GameClient::GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(_ticksPerSecond),
    m_catalog(LoadDataFile(MODELS_FILE, LoadModelCatalog)),
    m_camera(LoadDataFile(CAMERA_FILE, LoadCameraSettings)),
    m_pipeline(_renderer),
    m_glows(_renderer),
    m_groundMask(_renderer),
    m_ui(_renderer, HUD_FONT, Hud::FONT_UNITS * Hud::Scale(_renderer.WidthPixels(), _renderer.HeightPixels())),
    m_view(_ticksPerSecond),
    m_effects(_ticksPerSecond),
    m_particles(_ticksPerSecond),
    m_explosions(_ticksPerSecond)
{
  for (const ModelSet& set : m_catalog.sets)
  {
    for (const ModelEntry& model : set.models)
    {
      const std::wstring fileName = ModelFileName(set, model);
      const Neuron::ByteBuffer bytes = ReadAsset(fileName);
      Neuron::MeshData data = BuildModelMesh(bytes, model, std::format("Assets\\{}", winrt::to_string(fileName)));
      m_modelMeshes.emplace(MeshKey(set.name, model.name), std::make_unique<Neuron::Mesh>(_renderer, data));
      m_modelHardpoints.emplace(MeshKey(set.name, model.name), std::move(data.hardpoints));
      // Its creases, drawn over it as lines (ADR-027).
      const Neuron::MeshData edges = Neuron::BuildCreaseLines(data, CREASE_DEGREES * std::numbers::pi_v<float> / 180.0f, EDGE_LIFT_SHARE);
      if (!edges.vertices.empty())
        m_modelEdges.emplace(MeshKey(set.name, model.name), std::make_unique<Neuron::Mesh>(_renderer, edges));
      // Kept on the CPU too, for an explosion to break into its triangles (ADR-026).
      data.hardpoints.clear();
      m_modelShapes.emplace(MeshKey(set.name, model.name), std::move(data));
    }
  }
  m_grid = std::make_unique<Neuron::Mesh>(_renderer, BuildGrid());
  m_ring = std::make_unique<Neuron::Mesh>(_renderer, BuildRing());
  m_disc = std::make_unique<Neuron::Mesh>(_renderer, BuildDisc());
  m_strip = std::make_unique<Neuron::Mesh>(_renderer, BuildStrip());
  // How high each rock reaches over its center, for a Mining Rig to stand there; the top of its bounds if the line down
  // its center misses it.
  for (size_t index = 0; index < ROCK_MODELS.size(); ++index)
  {
    const Neuron::MeshData& rock = ModelShape(ASTEROID_SET, ROCK_MODELS[index]);
    m_rockTops[index] = Neuron::SurfaceHeightAt(rock, 0.0f, 0.0f).value_or(rock.boundsMax.y);
  }
  if (const StructureModel* rig = m_catalog.ModelForStructure(StructureKind::MiningRig))
  {
    for (const PlayerModels& player : m_catalog.players)
      m_rigFeet.emplace(MeshKey(player.set, rig->model), RigFeet(ModelShape(player.set, rig->model)));
  }

  const Starfield sky = BuildStarfield();
  m_sky = std::make_unique<Neuron::StarPipeline>(_renderer, sky.points);
  m_bursts = std::make_unique<Neuron::StarPipeline>(_renderer, sky.bursts, Neuron::StarPipeline::Shape::Cross);

  Neuron::TextureData particleSprite =
    Neuron::ParseDds(ReadAsset(PARTICLE_SPRITE_FILE), std::format("Assets\\{}", winrt::to_string(PARTICLE_SPRITE_FILE)));
  Neuron::BuildMipLevels(particleSprite);
  m_particleSprites = std::make_unique<Neuron::GlowPipeline>(_renderer, &particleSprite);
}

void Outpost::GameClient::StartMatch()
{
  ClearMatch();
  m_screen = Screen::Match;
}

void Outpost::GameClient::ShowMenu()
{
  ClearMatch();
  m_screen = Screen::Menu;
}

void Outpost::GameClient::ClearMatch()
{
  m_view = SnapshotInterpolator(m_ticksPerSecond);
  m_effects = CombatEffects(m_ticksPerSecond);
  m_particles = ParticleSystem(m_ticksPerSecond);
  m_explosions = ExplosionManager(m_ticksPerSecond);
  m_particleGlows.clear();
  m_shardVertices.clear();
  m_shardBatches.clear();
  m_controls = PlayerControls();
  m_designer = Designer();
  m_fog = FogOfWar();
  m_banking.Clear();
  m_effectDraws.clear();
  m_entities.clear();
  m_previousEntities.clear();
  m_hudLayout = {};
  m_cameraPlaced = false;
  m_minimapDragging = false;
  m_cursorGround.reset();
  m_probe.reset();
  m_responseShown.reset();
}

void Outpost::GameClient::Receive(std::vector<Snapshot> _snapshots)
{
  // On the menu there is no match to show; anything still arriving from the last one is dropped.
  if (m_screen == Screen::Menu)
    return;
  for (Snapshot& snapshot : _snapshots)
  {
    m_effects.Receive(snapshot);
    Explode(snapshot);
    m_view.Receive(std::move(snapshot));
  }

  // The player starts looking at its own fleet, wherever the map put its start.
  if (!m_cameraPlaced && !m_view.IsEmpty())
  {
    const Snapshot& newest = m_view.Newest();
    float sumX = 0.0f;
    float sumZ = 0.0f;
    int count = 0;
    for (const EntityView& entity : newest.entities)
    {
      if (entity.kind == EntityKind::Ship && entity.owner == newest.player)
      {
        sumX += entity.position.xMeters;
        sumZ += entity.position.zMeters;
        ++count;
      }
    }
    if (count > 0)
      m_camera.SetFocus(sumX / static_cast<float>(count), sumZ / static_cast<float>(count));
    m_cameraPlaced = true;
  }
}

void Outpost::GameClient::Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
                                 std::uint32_t _viewportHeightPixels)
{
  m_viewport = {.widthPixels = _viewportWidthPixels, .heightPixels = _viewportHeightPixels};
  if (m_screen == Screen::Menu)
  {
    // The menu is all there is: a press on its buttons, and nothing for the camera or the controls.
    m_hudLayout = Hud::LayMenu(_viewportWidthPixels, _viewportHeightPixels);
    if (!_input.active)
      return;
    for (const Neuron::InputEvent& event : _input.events)
    {
      if (event.kind != Neuron::InputEventKind::ButtonDown || event.key != VK_LBUTTON)
        continue;
      if (const std::optional<Hud::Action> action =
            m_hudLayout.ActionAt(static_cast<float>(event.xPixels), static_cast<float>(event.yPixels)))
        HandleHudAction(*action);
    }
    return;
  }

  m_view.Advance(_elapsedSeconds);
  m_previousEntities = std::move(m_entities);
  m_entities = m_view.Entities();
  if (!m_view.IsEmpty() && m_view.Newest().fogOfWar)
  {
    if (m_fog.CellsPerSide() == 0)
      m_fog.Reset(m_view.Newest().mapSizeMeters);
    m_fog.Update(m_entities, m_view.Newest().player);
  }
  UpdateBanking(_elapsedSeconds);
  m_frameSeconds = _elapsedSeconds;
  m_effectDraws = m_effects.At(
    m_view.ViewTick(), [this](EntityId _shooter, PlanePosition _target) { return MuzzleOf(_shooter, _target); },
    [this](EntityId _shooter) { return BeamColor(_shooter); });
  m_particleGlows.clear();
  m_particles.At(m_view.ViewTick(), m_particleGlows);
  m_shardVertices.clear();
  m_shardBatches.clear();
  m_explosions.At(m_view.ViewTick(), m_shardVertices, m_shardBatches);
  Neuron::InputState input = _input;
  if (!m_view.IsEmpty())
  {
    m_designer.Update(m_view.Newest());
    for (const QueueShipCommand& queue : m_designer.TakeQueueCommands(m_view.Newest()))
      m_controls.Queue(queue.producer, queue.design);
  }
  HandleTyping(input);
  m_camera.Update(input, _elapsedSeconds, _viewportWidthPixels, _viewportHeightPixels);

  m_cursorGround =
    m_camera.GroundPointAtPixel(static_cast<float>(input.cursorXPixels), static_cast<float>(input.cursorYPixels), m_viewport);
  HandleHudInput(input);

  // A press on the HUD is the HUD's, not an order or a selection in the world; releases still reach the controls, so
  // that a drag begun in the world ends wherever it is let go.
  std::erase_if(input.events,
                [this](const Neuron::InputEvent& _event)
                {
                  return _event.kind == Neuron::InputEventKind::ButtonDown &&
                         m_hudLayout.Covers(static_cast<float>(_event.xPixels), static_cast<float>(_event.yPixels));
                });
  if (!m_view.IsEmpty())
  {
    m_controls.Update(input, m_entities, m_view.Newest().player, m_camera, m_viewport);
    const std::vector<PlanePosition> view = ViewOnGround();
    Hud::Content content = Hud::Describe(m_view.Newest(), m_entities, m_controls.Selected(), m_controls.Placing(), &m_designer);
    content.fogShades.assign(m_fog.Shades().begin(), m_fog.Shades().end());
    content.fogCellsPerSide = m_fog.CellsPerSide();
    content.outcome = Hud::DescribeOutcome(m_view.Newest(), m_ticksPerSecond);
    // The name takes no more typing once the designer is not shown: its Shipyard was deselected or destroyed.
    if (!content.designer.has_value())
      m_designer.EndEditing();
    m_hudLayout = Hud::Lay(content, _viewportWidthPixels, _viewportHeightPixels, view);
  }
  WatchForResponse();
}

void Outpost::GameClient::HandleTyping(Neuron::InputState& _input)
{
  if (!m_designer.IsEditing() || m_view.IsEmpty())
    return;
  for (const Neuron::InputEvent& event : _input.events)
  {
    if (std::optional<SaveDesignCommand> save = m_designer.Edit(event, m_view.Newest()))
      m_controls.SaveDesign(std::move(*save));
  }
  std::erase_if(_input.events, [](const Neuron::InputEvent& _event)
                { return _event.kind == Neuron::InputEventKind::KeyDown || _event.kind == Neuron::InputEventKind::Character; });
  // The mouse buttons stay held; every key reads as up.
  std::bitset<256> buttons;
  for (const std::uint8_t button : std::array<std::uint8_t, 3>{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON})
    buttons.set(button, _input.keysDown.test(button));
  _input.keysDown = buttons;
}

void Outpost::GameClient::HandleHudAction(const Hud::Action& _action)
{
  switch (_action.kind)
  {
  case Hud::ActionKind::Build:
    m_controls.ArmPlacement(_action.structure, m_entities);
    break;
  case Hud::ActionKind::Queue:
    m_controls.Queue(_action.producer, _action.design);
    break;
  case Hud::ActionKind::Research:
    m_controls.Research(_action.producer, _action.topic);
    break;
  case Hud::ActionKind::PickHull:
    m_designer.PickHull(_action.hull);
    break;
  case Hud::ActionKind::PickDrive:
    m_designer.PickDrive(_action.drive);
    break;
  case Hud::ActionKind::PickWeapon:
    m_designer.PickWeapon(_action.weapon);
    break;
  case Hud::ActionKind::EditName:
    m_designer.BeginEditing(m_view.Newest());
    break;
  case Hud::ActionKind::SaveDesign:
    if (std::optional<SaveDesignCommand> save = m_designer.SaveCommand(m_view.Newest()))
    {
      m_controls.SaveDesign(std::move(*save));
      m_designer.ForgetTypedName();
    }
    break;
  case Hud::ActionKind::SaveAndQueue:
    if (std::optional<SaveDesignCommand> save = m_designer.SaveAndQueue(_action.producer, m_view.Newest()))
    {
      m_controls.SaveDesign(std::move(*save));
      m_designer.ForgetTypedName();
    }
    break;
  case Hud::ActionKind::StartSkirmish:
    m_request = Request::StartSkirmish;
    break;
  case Hud::ActionKind::Quit:
    m_request = Request::Quit;
    break;
  case Hud::ActionKind::BackToMenu:
    m_request = Request::BackToMenu;
    break;
  }
}

void Outpost::GameClient::HandleHudInput(const Neuron::InputState& _input)
{
  if (!_input.active)
  {
    m_minimapDragging = false;
    return;
  }
  for (const Neuron::InputEvent& event : _input.events)
  {
    const auto x = static_cast<float>(event.xPixels);
    const auto y = static_cast<float>(event.yPixels);
    if (event.kind == Neuron::InputEventKind::ButtonUp && event.key == VK_LBUTTON)
      m_minimapDragging = false;
    if (event.kind != Neuron::InputEventKind::ButtonDown)
      continue;
    if (event.key == VK_LBUTTON)
    {
      const std::optional<Hud::Action> action = m_hudLayout.ActionAt(x, y);
      // A press anywhere but the name field stops typing; what was typed stays.
      if (!action.has_value() || action->kind != Hud::ActionKind::EditName)
        m_designer.EndEditing();
      if (action.has_value())
      {
        if (!m_view.IsEmpty())
          HandleHudAction(*action);
      }
      else if (const std::optional<PlanePosition> point = m_hudLayout.MapPointAt(x, y))
      {
        m_camera.SetFocus(point->xMeters, point->zMeters);
        m_minimapDragging = true;
      }
    }
    else if (event.key == VK_RBUTTON)
    {
      // A right-click on the minimap sends the selected ships there.
      if (const std::optional<PlanePosition> point = m_hudLayout.MapPointAt(x, y))
        m_controls.MoveTo(*point, m_entities);
    }
  }
  if (m_minimapDragging && _input.IsDown(VK_LBUTTON))
  {
    if (const std::optional<PlanePosition> point =
          m_hudLayout.MapPointAt(static_cast<float>(_input.cursorXPixels), static_cast<float>(_input.cursorYPixels)))
      m_camera.SetFocus(point->xMeters, point->zMeters);
  }
}

std::vector<Outpost::PlanePosition> Outpost::GameClient::ViewOnGround() const
{
  const auto width = static_cast<float>(m_viewport.widthPixels);
  const auto height = static_cast<float>(m_viewport.heightPixels);
  std::vector<PlanePosition> corners;
  corners.reserve(4);
  for (const auto& [x, y] : std::array<std::pair<float, float>, 4>{{{0.0f, 0.0f}, {width, 0.0f}, {width, height}, {0.0f, height}}})
  {
    const std::optional<PlanePosition> point = m_camera.GroundPointAtPixel(x, y, m_viewport);
    if (!point.has_value())
      return {};
    corners.push_back(*point);
  }
  return corners;
}

void Outpost::GameClient::WatchForResponse()
{
  const auto findIn = [](const std::vector<EntityView>& _entities, EntityId _id) -> const EntityView*
  {
    const auto found = std::ranges::find(_entities, _id, &EntityView::id);
    return found == _entities.end() ? nullptr : &*found;
  };
  // Any motion at all, far below a pixel and far above float noise: a ship doing this is already moving.
  const auto hasChanged = [](const EntityView& _before, const EntityView& _now)
  {
    constexpr float MOVED_METERS = 0.01f;
    constexpr float TURNED_RADIANS = 0.001f;
    return std::hypot(_now.position.xMeters - _before.position.xMeters, _now.position.zMeters - _before.position.zMeters) > MOVED_METERS ||
           std::abs(InterpolateHeading(_before.headingRadians, _now.headingRadians, 1.0f) - _before.headingRadians) > TURNED_RADIANS;
  };
  // Q5's "visibly responding": the ship's center or its nose, a footprint's radius ahead of the center, has moved at
  // least a pixel on screen.
  const auto hasVisiblyChanged = [this](const EntityView& _before, const EntityView& _now)
  {
    constexpr float VISIBLE_PIXELS = 1.0f;
    const auto nose = [](const EntityView& _ship)
    {
      return PlanePosition{.xMeters = _ship.position.xMeters + (_ship.radiusMeters * std::cos(_ship.headingRadians)),
                           .zMeters = _ship.position.zMeters + (_ship.radiusMeters * std::sin(_ship.headingRadians))};
    };
    const auto moved = [this](PlanePosition _from, PlanePosition _to)
    {
      const std::optional<DirectX::XMFLOAT2> a = m_camera.PixelOf(_from, m_viewport);
      const std::optional<DirectX::XMFLOAT2> b = m_camera.PixelOf(_to, m_viewport);
      return a.has_value() && b.has_value() && std::hypot(b->x - a->x, b->y - a->y) >= VISIBLE_PIXELS;
    };
    return moved(_before.position, _now.position) || moved(nose(_before), nose(_now));
  };

  if (std::optional<PlayerControls::MoveOrder> order = m_controls.TakeLastMove())
  {
    // The view has not yet shown this order, which reaches the server only after this frame. A ship already moving
    // between the last frame and this one would hide the response, so such an order is not watched.
    ResponseProbe probe{.ships = {}, .inputRead = order->inputRead};
    bool alreadyMoving = false;
    for (const EntityId id : order->ships)
    {
      const EntityView* now = findIn(m_entities, id);
      const EntityView* before = findIn(m_previousEntities, id);
      if (now == nullptr)
        continue;
      alreadyMoving = alreadyMoving || (before != nullptr && hasChanged(*before, *now));
      probe.ships.push_back(*now);
    }
    m_probe.reset();
    if (!alreadyMoving && !probe.ships.empty())
      m_probe = std::move(probe);
    return;
  }

  if (!m_probe.has_value())
    return;
  for (const EntityView& before : m_probe->ships)
  {
    const EntityView* now = findIn(m_entities, before.id);
    if (now != nullptr && hasVisiblyChanged(before, *now))
    {
      m_responseShown = m_probe->inputRead;
      m_probe.reset();
      return;
    }
  }
}

void Outpost::GameClient::Render(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList)
{
  const float aspectRatio = static_cast<float>(_renderer.WidthPixels()) / static_cast<float>(_renderer.HeightPixels());
  Neuron::MeshPipeline::FrameConstants constants{};
  constants.viewProjection = m_camera.ViewProjection(aspectRatio);
  DirectX::XMStoreFloat3(&constants.directionToLight, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&TOWARD_LIGHT)));
  constants.ambient = AMBIENT;

  // The sky first, under everything. Its stars are sized on the reference frame, so they keep their look at any
  // resolution (ADR-006, ADR-022).
  const float skyScale = Hud::Scale(_renderer.WidthPixels(), _renderer.HeightPixels());
  const Neuron::StarPipeline::FrameConstants sky{
    .viewProjection = constants.viewProjection,
    .clipPerPixelX = 2.0f * skyScale / static_cast<float>(_renderer.WidthPixels()),
    .clipPerPixelY = 2.0f * skyScale / static_cast<float>(_renderer.HeightPixels()),
    .unused0 = 0.0f,
    .unused1 = 0.0f,
  };
  m_sky->Draw(_commandList, sky);
  m_bursts->Draw(_commandList, sky);

  m_pipeline.BeginDrawing(_commandList, _renderer.FrameIndex(), constants);

  const DirectX::XMFLOAT4X4 identity = WorldMatrix({}, 0.0f, 1.0f);
  m_pipeline.DrawLines(_commandList, *m_grid, identity, GRID_COLOR);
  // The menu shows over the empty grid and the sky.
  if (m_screen == Screen::Menu)
  {
    DrawHud(_commandList, _renderer.FrameIndex());
    return;
  }
  m_lineDraws.clear();
  for (const EntityView& entity : m_entities)
    DrawEntity(_commandList, entity);
  DrawShards(_commandList);
  // Every model's lines at once, after every face they may lie behind.
  m_pipeline.DrawLines(_commandList, m_lineDraws);
  DrawSelection(_commandList);
  DrawGhost(_commandList);
  DrawHealthBars(_commandList);
  DrawEffects(_commandList);
  DrawGlows(_renderer, _commandList);
  DrawFog(_renderer, _commandList);
  DrawHud(_commandList, _renderer.FrameIndex());
}

void Outpost::GameClient::DrawFog(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList)
{
  if (m_fog.CellsPerSide() == 0)
    return;
  const float aspectRatio = static_cast<float>(_renderer.WidthPixels()) / static_cast<float>(_renderer.HeightPixels());
  const PlanePosition origin = m_fog.Origin();
  const Neuron::GroundMaskPipeline::FrameConstants constants{.viewProjection = m_camera.ViewProjection(aspectRatio),
                                                             .originXMeters = origin.xMeters,
                                                             .originZMeters = origin.zMeters,
                                                             .cellMeters = FogOfWar::CELL_METERS,
                                                             .cellsPerSide = m_fog.CellsPerSide()};
  m_groundMask.Draw(_commandList, _renderer.FrameIndex(), constants, m_fog.Shades());
}

void Outpost::GameClient::DrawHud(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex)
{
  // Nothing to show before the first snapshot has been laid out.
  if (m_hudLayout.fontPixels <= 0.0f)
    return;
  m_ui.Begin(m_viewport.widthPixels, m_viewport.heightPixels, m_hudLayout.fontPixels);
  for (const Hud::Rect& panel : m_hudLayout.panels)
    m_ui.FillRect(panel.left, panel.top, panel.width, panel.height, panel.color);
  for (const Hud::Text& text : m_hudLayout.texts)
    m_ui.DrawText(text.text, text.left, text.top, text.color);
  m_ui.End(_commandList, _frameIndex);
}

void Outpost::GameClient::DrawHealthBars(ID3D12GraphicsCommandList* _commandList)
{
  for (const EntityView& entity : m_entities)
  {
    const ModelSet* side = m_catalog.SetForPlayer(entity.owner);
    const DirectX::XMFLOAT4 back = side != nullptr ? Shaded(side->color, HEALTH_BACK_SHADE) : HEALTH_BACK_COLOR;
    if (entity.builtPermille < PERMILLE)
    {
      const float left = entity.position.xMeters - entity.radiusMeters;
      const float z = entity.position.zMeters - entity.radiusMeters - (2.0f * HEALTH_BAR_GAP_METERS) - HEALTH_BAR_WIDTH_METERS;
      const float built = static_cast<float>(entity.builtPermille) / static_cast<float>(PERMILLE);
      DrawBand(_commandList, {.xMeters = left, .zMeters = z}, {.xMeters = left + (2.0f * entity.radiusMeters), .zMeters = z},
               HEALTH_BAR_WIDTH_METERS, HEALTH_BAR_HEIGHT_METERS, back);
      DrawBand(_commandList, {.xMeters = left, .zMeters = z}, {.xMeters = left + (2.0f * entity.radiusMeters * built), .zMeters = z},
               HEALTH_BAR_WIDTH_METERS, HEALTH_BAR_HEIGHT_METERS + OVERLAY_LIFT_METERS, BUILD_BAR_COLOR);
    }
    if (entity.maxHitPointsHundredths <= 0 || entity.hitPointsHundredths >= entity.maxHitPointsHundredths)
      continue;
    const float share =
      std::clamp(static_cast<float>(entity.hitPointsHundredths) / static_cast<float>(entity.maxHitPointsHundredths), 0.0f, 1.0f);
    const float left = entity.position.xMeters - entity.radiusMeters;
    const float z = entity.position.zMeters - entity.radiusMeters - HEALTH_BAR_GAP_METERS;
    const PlanePosition start{.xMeters = left, .zMeters = z};
    DrawBand(_commandList, start, {.xMeters = left + (2.0f * entity.radiusMeters), .zMeters = z}, HEALTH_BAR_WIDTH_METERS,
             HEALTH_BAR_HEIGHT_METERS, back);
    const DirectX::XMFLOAT4& color = share > HEALTH_HURT_SHARE  ? HEALTH_GOOD_COLOR
                                     : share > HEALTH_LOW_SHARE ? HEALTH_HURT_COLOR
                                                                : HEALTH_LOW_COLOR;
    // A little higher than the dark bar, so the two do not fight over the same depth.
    DrawBand(_commandList, start, {.xMeters = left + (2.0f * entity.radiusMeters * share), .zMeters = z}, HEALTH_BAR_WIDTH_METERS,
             HEALTH_BAR_HEIGHT_METERS + OVERLAY_LIFT_METERS, color);
  }
}

void Outpost::GameClient::DrawEffects(ID3D12GraphicsCommandList* _commandList)
{
  for (const CombatEffects::Draw& draw : m_effectDraws)
  {
    const DirectX::XMFLOAT3 at{draw.from.xMeters, draw.heightMeters, draw.from.zMeters};
    switch (draw.shape)
    {
    case CombatEffects::Shape::Disc:
      m_pipeline.Draw(_commandList, *m_disc, WorldMatrix(at, 0.0f, draw.radiusMeters), draw.color);
      break;
    case CombatEffects::Shape::Ring:
      m_pipeline.Draw(_commandList, *m_ring, WorldMatrix(at, 0.0f, draw.radiusMeters), draw.color);
      break;
    case CombatEffects::Shape::Band:
    default:
      DrawBand(_commandList, draw.from, draw.to, draw.widthMeters, draw.heightMeters, draw.color);
      break;
    }
  }
}

void Outpost::GameClient::DrawSelection(ID3D12GraphicsCommandList* _commandList)
{
  const DirectX::XMFLOAT4& ringColor = m_controls.IsAttackMoveArmed() ? ATTACK_MOVE_COLOR : SELECTION_COLOR;
  for (const EntityId id : m_controls.Selected())
  {
    const auto ship = std::ranges::find(m_entities, id, &EntityView::id);
    if (ship == m_entities.end())
      continue;
    const DirectX::XMFLOAT3 at{ship->position.xMeters, OVERLAY_LIFT_METERS, ship->position.zMeters};
    m_pipeline.Draw(_commandList, *m_ring, WorldMatrix(at, 0.0f, ship->radiusMeters * RING_SIZE_PER_FOOTPRINT), ringColor);
  }

  // The box's corners on the ground, so the outline covers exactly the ground the screen rectangle does.
  const std::optional<ScreenRect> box = m_controls.DragBox();
  if (!box.has_value())
    return;
  const std::array<std::pair<float, float>, 4> corners{
    {{box->left, box->top}, {box->right, box->top}, {box->right, box->bottom}, {box->left, box->bottom}}};
  std::array<PlanePosition, 4> ground{};
  for (size_t i = 0; i < corners.size(); ++i)
  {
    const std::optional<PlanePosition> point = m_camera.GroundPointAtPixel(corners[i].first, corners[i].second, m_viewport);
    if (!point.has_value())
      return;
    ground[i] = *point;
  }
  for (size_t i = 0; i < ground.size(); ++i)
    DrawBand(_commandList, ground[i], ground[(i + 1) % ground.size()], DRAG_LINE_WIDTH_METERS, OVERLAY_LIFT_METERS, DRAG_BOX_COLOR);
}

void Outpost::GameClient::DrawBand(ID3D12GraphicsCommandList* _commandList, PlanePosition _from, PlanePosition _to, float _widthMeters,
                                   float _heightMeters, const DirectX::XMFLOAT4& _color)
{
  const float dx = _to.xMeters - _from.xMeters;
  const float dz = _to.zMeters - _from.zMeters;
  const float length = std::hypot(dx, dz);
  if (length <= 0.0f)
    return;
  // The strip is stretched along x and widened along z, which keeps its upward normal, then turned onto the line.
  const DirectX::XMMATRIX world = DirectX::XMMatrixScaling(length, 1.0f, _widthMeters) * DirectX::XMMatrixRotationY(-std::atan2(dz, dx)) *
                                  DirectX::XMMatrixTranslation(_from.xMeters, _heightMeters, _from.zMeters);
  DirectX::XMFLOAT4X4 matrix;
  DirectX::XMStoreFloat4x4(&matrix, world);
  m_pipeline.Draw(_commandList, *m_strip, matrix, _color);
}

void Outpost::GameClient::DrawEntity(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity)
{
  const DirectX::XMFLOAT3 position{_entity.position.xMeters, 0.0f, _entity.position.zMeters};
  switch (_entity.kind)
  {
  case EntityKind::Ship:
  {
    // The data maps every player and hull the server can send, and the Constructor (ModelCatalog); anything else is not
    // drawn.
    if (const std::optional<PlacedModel> placed = PlaceModel(_entity))
      DrawModel(_commandList, placed->set->name, *placed->model, PoseMatrix(placed->pose), placed->set->color);
    break;
  }
  case EntityKind::Asteroid:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    DrawModel(_commandList, ASTEROID_SET, RockModel(_entity.radiusMeters),
              WorldMatrix(position, _entity.headingRadians, _entity.radiusMeters), set.color);
    break;
  }
  case EntityKind::AsteroidField:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    const DirectX::XMFLOAT4 color{set.color.x * FIELD_SHADE, set.color.y * FIELD_SHADE, set.color.z * FIELD_SHADE, set.color.w};
    const float radius = _entity.radiusMeters;
    const float centerRock = radius * FIELD_CENTER_ROCK_SHARE;
    DrawModel(_commandList, ASTEROID_SET, RockModel(centerRock), WorldMatrix(position, 0.0f, centerRock), color);
    const float ringRock = radius * FIELD_RING_ROCK_SHARE;
    for (int i = 0; i < FIELD_RING_ROCKS; ++i)
    {
      const float angle = FIELD_RING_START_RADIANS + (static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / FIELD_RING_ROCKS);
      const float distance = radius * FIELD_RING_DISTANCE_SHARE;
      const DirectX::XMFLOAT3 at{position.x + (distance * std::cos(angle)), 0.0f, position.z + (distance * std::sin(angle))};
      DrawModel(_commandList, ASTEROID_SET, RockModel(ringRock), WorldMatrix(at, angle * 2.0f, ringRock), color);
    }
    break;
  }
  case EntityKind::Structure:
  default:
    DrawStructure(_commandList, _entity);
    break;
  }
}

void Outpost::GameClient::DrawModel(ID3D12GraphicsCommandList* _commandList, std::string_view _set, std::string_view _model,
                                    const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4& _color)
{
  m_pipeline.Draw(_commandList, ModelMesh(_set, _model), _world, Shaded(_color, FILL_SHADE));
  if (const auto edges = m_modelEdges.find(MeshKey(_set, _model)); edges != m_modelEdges.end())
  {
    const float brightness = _set == ASTEROID_SET ? ROCK_EDGE_BRIGHTNESS : EDGE_BRIGHTNESS;
    m_lineDraws.push_back({.lines = edges->second.get(), .world = _world, .color = Shaded(_color, brightness)});
  }
}

void Outpost::GameClient::DrawStructure(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity)
{
  const std::optional<PlacedModel> placed = PlaceModel(_entity);
  if (!placed.has_value())
    return;
  DrawModel(_commandList, placed->set->name, *placed->model, PoseMatrix(placed->pose),
            ModelColor(placed->set->color, placed->tint, _entity.builtPermille));
}

void Outpost::GameClient::Explode(const Snapshot& _snapshot)
{
  // As a shot does, a destruction plays from one tick before its snapshot, where the view shows it (ADR-013).
  const double start = static_cast<double>(_snapshot.tick) - 1.0;
  for (const DestroyedView& destroyed : _snapshot.destroyed)
  {
    // What blew up as the view last held it, which says what a structure is and how it is drawn; it is not in this
    // snapshot. It breaks where the server says it died. Without it, a ship falls back on its hull.
    EntityView entity{.id = destroyed.id, .kind = destroyed.kind, .owner = destroyed.owner, .hull = destroyed.hull};
    if (!m_view.IsEmpty())
    {
      const std::vector<EntityView>& last = m_view.Newest().entities;
      if (const auto found = std::ranges::find(last, destroyed.id, &EntityView::id); found != last.end())
        entity = *found;
    }
    entity.position = destroyed.position;
    entity.headingRadians = destroyed.headingRadians;
    entity.radiusMeters = destroyed.radiusMeters;

    // The same destruction always looks the same.
    const std::uint64_t seed = (std::uint64_t{destroyed.id.value} << 32U) ^ _snapshot.tick;
    const std::optional<PlacedModel> placed = PlaceModel(entity);
    const float liftMeters = placed.has_value() ? placed->pose.liftMeters : 0.0f;
    m_particles.AddBlast({destroyed.position.xMeters, liftMeters, destroyed.position.zMeters}, destroyed.radiusMeters, destroyed.kind,
                         start, seed);
    if (placed.has_value())
    {
      // The shards are the faces' color, which is the model's darkened (ADR-027).
      m_explosions.Add(ModelShape(placed->set->name, *placed->model), PoseMatrix(placed->pose),
                       Shaded(ModelColor(placed->set->color, placed->tint, entity.builtPermille), FILL_SHADE), start, seed,
                       destroyed.kind == EntityKind::Structure ? STRUCTURE_SHARD_COPIES : 1);
    }
  }
}

void Outpost::GameClient::DrawShards(ID3D12GraphicsCommandList* _commandList)
{
  const std::span<const Neuron::MeshVertex> vertices(m_shardVertices);
  // The batches come newest first, so a frame with more shards than the pipeline takes drops the oldest, darkest ones.
  for (const ExplosionManager::Batch& batch : m_shardBatches)
    m_pipeline.DrawTriangles(_commandList, vertices.subspan(batch.firstVertex, batch.vertexCount), batch.color);
}

std::optional<Outpost::GameClient::PlacedModel> Outpost::GameClient::PlaceModel(const EntityView& _entity) const
{
  const ModelSet* set = m_catalog.SetForPlayer(_entity.owner);
  if (set == nullptr)
    return std::nullopt;
  if (_entity.kind == EntityKind::Ship)
  {
    const std::string* model = _entity.role == ShipRole::Constructor ? &m_catalog.constructor : m_catalog.ModelForHull(_entity.hull);
    if (model == nullptr)
      return std::nullopt;
    return PlacedModel{
      .set = set,
      .model = model,
      .pose = {.position = _entity.position, .headingRadians = _entity.headingRadians, .bankRadians = m_banking.BankRadians(_entity.id)}};
  }
  if (_entity.kind != EntityKind::Structure || m_view.IsEmpty())
    return std::nullopt;
  const StructureModel* model = m_catalog.ModelForStructure(_entity.structure);
  if (model == nullptr)
    return std::nullopt;
  // Drawn across its kind's footprint: a Mining Rig's entity covers its asteroid, but the rig is the size of its kind,
  // standing on the rock.
  const Snapshot& newest = m_view.Newest();
  const auto type = std::ranges::find(newest.structureTypes, _entity.structure, &StructureTypeView::structure);
  const float radius = type != newest.structureTypes.end() ? type->radiusMeters : _entity.radiusMeters;
  const ModelEntry& entry = set->Model(model->model);
  const float scale = 2.0f * radius / entry.lengthMeters;
  const float lift = _entity.structure == StructureKind::MiningRig && radius < _entity.radiusMeters
                       ? RigLift(set->name, model->model, _entity, scale)
                       : 0.0f;
  return PlacedModel{.set = set,
                     .model = &model->model,
                     .pose = {.position = _entity.position, .liftMeters = lift, .headingRadians = _entity.headingRadians, .scale = scale},
                     .tint = model->tint};
}

void Outpost::GameClient::UpdateBanking(float _elapsedSeconds)
{
  const std::vector<EntityMotion> motions = m_view.Motions();
  m_bankTargets.clear();
  for (const EntityView& ship : m_entities)
  {
    const BankLimits* limits = m_catalog.BankFor(ship);
    if (limits == nullptr)
      continue;
    const auto motion = std::ranges::lower_bound(motions, ship.id, {}, &EntityMotion::id);
    const float bank = motion != motions.end() && motion->id == ship.id ? TargetBankRadians(*motion, *limits) : 0.0f;
    m_bankTargets.push_back({.id = ship.id, .bankRadians = bank, .settleSeconds = limits->settleSeconds});
  }
  m_banking.Update(m_bankTargets, _elapsedSeconds);
}

float Outpost::GameClient::RigLift(std::string_view _set, std::string_view _model, const EntityView& _rig, float _scale) const
{
  // The rig stands at its asteroid's center (design §6), so its entity's radius is the rock's.
  const float rockRadius = _rig.radiusMeters;
  const Neuron::MeshData& rock = ModelShape(ASTEROID_SET, RockModel(rockRadius));
  // Without feet over the rock, the rig's lowest point stands on the rock's top.
  const float onTop = (m_rockTops[RockIndex(rockRadius)] * rockRadius) - (ModelShape(_set, _model).boundsMin.y * _scale);
  const auto feet = m_rigFeet.find(MeshKey(_set, _model));
  if (feet == m_rigFeet.end())
    return onTop;

  // The rock is turned by its asteroid's heading and the rig by its own, both as WorldMatrix turns them, so a foot lies
  // over the rock's mesh where the rig's turn less the rock's puts it.
  const auto asteroid = std::ranges::find_if(m_entities, [&_rig](const EntityView& _entity)
                                             { return _entity.kind == EntityKind::Asteroid && _entity.position == _rig.position; });
  const float rockHeading = asteroid != m_entities.end() ? asteroid->headingRadians : 0.0f;
  const DirectX::XMMATRIX toRock = DirectX::XMMatrixRotationY(rockHeading - _rig.headingRadians);
  // The lift that puts each foot on the rock under it; the least of them puts every foot on it or in it.
  std::optional<float> deepest;
  for (const DirectX::XMFLOAT3& foot : feet->second)
  {
    const DirectX::XMVECTOR over =
      DirectX::XMVector3Transform(DirectX::XMVectorSet(foot.x * _scale / rockRadius, 0.0f, foot.z * _scale / rockRadius, 0.0f), toRock);
    const std::optional<float> surface = Neuron::SurfaceHeightAt(rock, DirectX::XMVectorGetX(over), DirectX::XMVectorGetZ(over));
    if (!surface.has_value())
      continue;
    const float footLift = (*surface * rockRadius) - (foot.y * _scale);
    if (!deepest.has_value() || footLift < *deepest)
      deepest = footLift;
  }
  return deepest.value_or(onTop);
}

std::optional<DirectX::XMFLOAT4> Outpost::GameClient::BeamColor(EntityId _shooter) const
{
  const auto shooter = std::ranges::find(m_entities, _shooter, &EntityView::id);
  if (shooter == m_entities.end())
    return std::nullopt;
  const ModelSet* set = m_catalog.SetForPlayer(shooter->owner);
  if (set == nullptr)
    return std::nullopt;
  const auto toWhite = [](float _channel) { return std::lerp(_channel, 1.0f, BEAM_WHITE_SHARE); };
  return DirectX::XMFLOAT4{toWhite(set->color.x), toWhite(set->color.y), toWhite(set->color.z), 1.0f};
}

std::optional<Outpost::PlanePosition> Outpost::GameClient::MuzzleOf(EntityId _shooter, PlanePosition _target) const
{
  const auto shooter = std::ranges::find(m_entities, _shooter, &EntityView::id);
  if (shooter == m_entities.end())
    return std::nullopt;
  const std::optional<PlacedModel> placed = PlaceModel(*shooter);
  if (!placed.has_value())
    return std::nullopt;
  return NearestMuzzle(ModelHardpoints(placed->set->name, *placed->model), placed->pose, _target);
}

void Outpost::GameClient::DrawGlows(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList)
{
  m_frameGlows.clear();
  for (const EntityView& ship : m_entities)
  {
    const DirectX::XMFLOAT4* color = m_catalog.ExhaustColor(ship);
    const std::optional<PlacedModel> placed = color != nullptr ? PlaceModel(ship) : std::nullopt;
    if (!placed.has_value())
      continue;
    // How fast the view shows the ship going, from where the last frame drew it.
    float speedShare = 0.0f;
    const auto before = std::ranges::find(m_previousEntities, ship.id, &EntityView::id);
    if (before != m_previousEntities.end() && m_frameSeconds > 0.0f)
    {
      const float meters = std::hypot(ship.position.xMeters - before->position.xMeters, ship.position.zMeters - before->position.zMeters);
      speedShare = meters / m_frameSeconds / EXHAUST_FULL_SPEED_METERS_PER_SECOND;
    }
    AddExhaustGlows(ModelHardpoints(placed->set->name, *placed->model), placed->pose, *color, speedShare, m_frameGlows);
  }
  const float aspectRatio = static_cast<float>(_renderer.WidthPixels()) / static_cast<float>(_renderer.HeightPixels());
  const auto [right, up] = m_camera.ScreenAxes(aspectRatio);
  const DirectX::XMFLOAT4X4 viewProjection = m_camera.ViewProjection(aspectRatio);
  const Neuron::GlowPipeline::FrameConstants constants{
    .viewProjection = viewProjection, .screenRight = right, .unused0 = 0.0f, .screenUp = up, .unused1 = 0.0f};
  m_glows.Draw(_commandList, _renderer.FrameIndex(), constants, m_frameGlows);

  // A particle is a diamond, as DeepSpaceOutpost draws it: the square turned an eighth of a turn, its tips a radius out
  // along the screen's right and up. Its quad's axes are the sum and the difference of those, halved.
  const DirectX::XMFLOAT3 diamondRight{(right.x + up.x) * 0.5f, (right.y + up.y) * 0.5f, (right.z + up.z) * 0.5f};
  const DirectX::XMFLOAT3 diamondUp{(up.x - right.x) * 0.5f, (up.y - right.y) * 0.5f, (up.z - right.z) * 0.5f};
  const Neuron::GlowPipeline::FrameConstants particleConstants{
    .viewProjection = viewProjection, .screenRight = diamondRight, .unused0 = 0.0f, .screenUp = diamondUp, .unused1 = 0.0f};
  m_particleSprites->Draw(_commandList, _renderer.FrameIndex(), particleConstants, m_particleGlows);
}

void Outpost::GameClient::DrawGhost(ID3D12GraphicsCommandList* _commandList)
{
  const std::optional<StructureKind> placing = m_controls.Placing();
  if (!placing.has_value() || !m_cursorGround.has_value() || m_view.IsEmpty())
    return;
  const Snapshot& newest = m_view.Newest();
  const auto type = std::ranges::find(newest.structureTypes, *placing, &StructureTypeView::structure);
  if (type == newest.structureTypes.end())
    return;
  const GhostPlacement ghost = PlaceGhost(*type, *m_cursorGround, m_entities, newest.mapSizeMeters);
  const DirectX::XMFLOAT3 at{ghost.position.xMeters, OVERLAY_LIFT_METERS, ghost.position.zMeters};
  m_pipeline.Draw(_commandList, *m_ring, WorldMatrix(at, 0.0f, ghost.radiusMeters), ghost.valid ? GHOST_VALID_COLOR : GHOST_INVALID_COLOR);
  // The structure itself, shown where it would stand.
  if (const ModelSet* set = m_catalog.SetForPlayer(newest.player))
  {
    if (const StructureModel* model = m_catalog.ModelForStructure(*placing))
    {
      const ModelEntry& entry = set->Model(model->model);
      const DirectX::XMFLOAT3 standing{ghost.position.xMeters, 0.0f, ghost.position.zMeters};
      m_pipeline.Draw(_commandList, ModelMesh(set->name, model->model),
                      WorldMatrix(standing, 0.0f, 2.0f * type->radiusMeters / entry.lengthMeters),
                      ghost.valid ? GHOST_VALID_COLOR : GHOST_INVALID_COLOR);
    }
  }
}

const std::vector<Neuron::MeshHardpoint>& Outpost::GameClient::ModelHardpoints(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelHardpoints.find(MeshKey(_set, _model));
  if (found == m_modelHardpoints.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return found->second;
}

const Neuron::MeshData& Outpost::GameClient::ModelShape(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelShapes.find(MeshKey(_set, _model));
  if (found == m_modelShapes.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return found->second;
}

const Neuron::Mesh& Outpost::GameClient::ModelMesh(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelMeshes.find(MeshKey(_set, _model));
  if (found == m_modelMeshes.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return *found->second;
}

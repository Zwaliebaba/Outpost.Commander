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

// The keys are KeyBindings.h's, which the HUD's buttons and the Controls window show (tasks 15.2 and 16.4).

// One light from above and behind the default view's top-left, and how much of an object's color the unlit side keeps.
// Presentation, not tuning: the design asks only that the scene reads clearly (design §11).
constexpr DirectX::XMFLOAT3 TOWARD_LIGHT{-0.4f, 0.8f, 0.45f};
constexpr float AMBIENT = 0.3f;

// The grid covers the 10,000 m map (Phase 4 design §6) with a line every 100 m, each a pixel wide at any zoom, in the line art
// of the asteroids' ridges (ADR-028). It is a dim blue-gray, barely there, so the sky shows through (ADR-022) and the
// ridges stand out above it, and it is what shows the ground moving when the view pans.
constexpr float GRID_HALF_EXTENT_METERS = 5000.0f;
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
// A field is a cluster of rocks inside its circle, as FieldLayout lays it out from its identifier, its ring closed to gaps
// no wider than the smallest hull (interface plan 2, task UI5.2). The server blocks the whole circle (MVP design §4). A field
// is darker than an ore asteroid, so the two read apart.
constexpr float FIELD_SHADE = 0.7f;
// Every model, rock, ship and structure, also shows its edges as thin lines, for a vector look from the eighties (owner,
// 2026-10-02, ADR-027). Only the edges where its surface bends by more than CREASE_DEGREES are drawn: on low-poly
// models that is nearly every edge between two facets, and two triangles that lie almost flat read as one facet. The
// faces are dark, FILL_SHADE of the model's color, and the lines, lit as the model is, carry its shape (ADR-040). A ship's
// or structure's lines are its color made up to EDGE_BRIGHTNESS brighter, but no further than its brightest channel
// reaching 1, so that its hue holds rather than clipping toward cyan or orange, and then EDGE_WHITE_SHARE of the way to
// white: about 4:1 above the faces beside them on the lit side and nearly 3:1 on the dark side. The rocks are terrain,
// so their lines are only ROCK_EDGE_BRIGHTNESS brighter and keep their gray: near white, they outshone both fleets
// (owner, 2026-10-02, ADR-028).
constexpr float CREASE_DEGREES = 10.0f;
constexpr float EDGE_BRIGHTNESS = 2.0f;
constexpr float EDGE_WHITE_SHARE = 0.35f;
constexpr float ROCK_EDGE_BRIGHTNESS = 1.35f;
constexpr float FILL_SHADE = 0.3f;
// A rock's faces are darker still, half a ship's, so that at the default zoom the terrain stands back from the fleet; its
// lines keep ROCK_EDGE_BRIGHTNESS and carry its shape (interface plan 2, task UI5.1).
constexpr float ROCK_FILL_SHADE = 0.15f;
// A model's lines are pulled this share of their distance toward the eye: about two and a half pixels of depth at the
// camera's 45 degree field of view on a 1080-pixel screen. They show over the faces they lie on and move nowhere on the
// screen, so they no longer stand off a silhouette as a lift along the normal did (ADR-040).
constexpr float LINE_LIFT_SHARE = 0.002f;
// A structure stands back from the ships, which are what take orders: its color STRUCTURE_GRAY_SHARE of the way to a gray
// as light as it, and its faces STRUCTURE_FILL_SHADE of that rather than FILL_SHADE (ADR-040).
constexpr float STRUCTURE_GRAY_SHARE = 0.35f;
constexpr float STRUCTURE_FILL_SHADE = 0.22f;
// Every structure stands on a faint ring the size of its selection ring, which puts it on the ground and says its size: a
// line one pixel wide at any zoom, RING_LINE_SEGMENTS long, in its side's color at FOOTPRINT_RING_SHADE. Under the
// pointer, and under every structure while one is placed, it takes the color of the structure's own lines. Selected, the
// selection's green ring takes its place (ADR-042). At rest it is there for the far view, where a structure is small, and
// fades as the camera comes in: at full strength while its radius is at most RING_FULL_VIEW_SHARE of the view's width,
// and gone from RING_GONE_VIEW_SHARE. A Mining Rig's ring, its own footprint's laid over its rock, shows only under the
// pointer and while a structure is placed (owner, 2026-10-03, ADR-046).
constexpr float FOOTPRINT_RING_SHADE = 0.35f;
constexpr int RING_LINE_SEGMENTS = 96;
constexpr float RING_FULL_VIEW_SHARE = 0.03f;
constexpr float RING_GONE_VIEW_SHARE = 0.065f;

// A Mining Rig's feet are its lowest points out toward its rim, past this share of its half length from its middle, and
// within this share of its height of the lowest of them: its legs' tips, and not a drill hanging under its middle. The
// rig is tilted and lifted until its legs stand on the rock under them, none in it, and its drill goes into the rock
// (owner, 2026-10-03, ADR-044).
constexpr float RIG_FOOT_RADIUS_SHARE = 0.5f;
constexpr float RIG_FOOT_HEIGHT_SHARE = 0.05f;

// A beam is its shooter's side's color taken this share of the way to white, so that the player sees whose fire it is
// and it still reads as light (ADR-028).
constexpr float BEAM_WHITE_SHARE = 0.35f;

// The selection is a ring on the ground around each selected ship or structure, green, or amber while attack-move waits for
// its click; a drag box is its outline on the ground. Both sit just above the grid so that they do not flicker with it. The
// ring is SELECTION_RING_UNITS of the HUD's reference units wide at any zoom, about 3 pixels at 1080p: as many one-pixel
// lines as that is pixels, a pixel apart, centered on the ring's radius (ADR-067). The placement ghost and the effects
// keep the band of RING_INNER_RADIUS to RING_OUTER_RADIUS.
constexpr float SELECTION_RING_UNITS = 3.0f;
constexpr float RING_INNER_RADIUS = 1.0f;
constexpr float RING_OUTER_RADIUS = 1.15f;
constexpr int RING_SEGMENTS = 48;
constexpr float RING_SIZE_PER_FOOTPRINT = 1.3f;
constexpr float OVERLAY_LIFT_METERS = 0.3f;
constexpr float DRAG_LINE_WIDTH_METERS = 1.5f;
constexpr DirectX::XMFLOAT4 SELECTION_COLOR{0.25f, 0.95f, 0.35f, 1.0f};
constexpr DirectX::XMFLOAT4 ATTACK_MOVE_COLOR{1.0f, 0.65f, 0.1f, 1.0f};
constexpr DirectX::XMFLOAT4 DRAG_BOX_COLOR{0.25f, 0.95f, 0.35f, 1.0f};

// The back of the bars over a ship or a structure, which the HUD draws on the screen (interface plan 2, task UI4.1): its
// side's color darkened to HEALTH_BACK_SHADE, so that a bar says whose it is as well as how hurt (owner, 2026-10-02,
// ADR-028); dark gray for a side the data does not name. Held, KEY_EVERY_HEALTH_BAR shows a bar over every ship and
// structure, whole or not (ADR-047).
constexpr DirectX::XMFLOAT4 HEALTH_BACK_COLOR{0.08f, 0.08f, 0.08f, 1.0f};
constexpr float HEALTH_BACK_SHADE = 0.3f;
// A structure under construction: its color darkens toward this share at the start, and a bar under its health bar shows
// the share built (ADR-085 decision 2).
constexpr float UNBUILT_SHADE = 0.35f;
// The territory, drawn over the fog, since every player sees it (interface plan 2, task UI1.1): the lattice of sector
// borders one pixel wide in TERRITORY_LATTICE_COLOR, brighter than the grid and never wider (ADR-028); an outline inside each
// held or guarded sector in its holder's color at TERRITORY_OUTLINE_SHADE, dashed while suppressed and dotted while cut
// off; and, while the player places a Relay, two rings on each node it could claim now, the outer the size of a Relay's
// footprint ring and the inner CLAIMABLE_INNER_SHARE of it, in the player's own lines' color (ADR-081 decision 3). A side is
// drawn with one mesh 1 long, scaled to it, so its dashes and dots are shares of its length: on a 2,000 m side, 40 dashes of
// 25 m and 100 dots of 4 m.
constexpr DirectX::XMFLOAT4 TERRITORY_LATTICE_COLOR{0.04f, 0.043f, 0.063f, 1.0f};
constexpr float TERRITORY_OUTLINE_SHADE = 0.5f;
constexpr float CLAIMABLE_INNER_SHARE = 0.75f;
constexpr int TERRITORY_DASHES = 40;
constexpr float TERRITORY_DASH_SHARE = 0.5f;
constexpr int TERRITORY_DOTS = 100;
constexpr float TERRITORY_DOT_SHARE = 0.2f;
// A structure or derelict the player only remembers (ADR-024) is drawn over the fog as its lines alone: no faces, no bar
// and no ring, in its side's color MEMORY_GRAY_SHARE of the way to a gray as light as it, at MEMORY_SHADE, however far it
// was built. What the player sees now has faces; a memory does not (interface plan 2, task UI1.2).
constexpr float MEMORY_GRAY_SHARE = 0.5f;
constexpr float MEMORY_SHADE = 0.6f;
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

// A ring of radius 1 on the ground as a line list for MeshPipeline::DrawLines, lit as the ground facing up.
Neuron::MeshData BuildRingLine()
{
  Neuron::MeshData ring;
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  for (int i = 0; i < RING_LINE_SEGMENTS; ++i)
  {
    const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / RING_LINE_SEGMENTS;
    ring.vertices.push_back({{std::cos(angle), 0.0f, std::sin(angle)}, UP});
    ring.indices.push_back(static_cast<std::uint32_t>(i));
    ring.indices.push_back(static_cast<std::uint32_t>((i + 1) % RING_LINE_SEGMENTS));
  }
  ring.boundsMin = {-1.0f, 0.0f, -1.0f};
  ring.boundsMax = {1.0f, 0.0f, 1.0f};
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

// A line from x = 0 to 1 on the ground, broken into _pieces of which each draws its first _drawnShare, as a line list for
// MeshPipeline::DrawLines, lit as the ground facing up: whole for one piece drawn whole, dashed or dotted for more.
Neuron::MeshData BuildBrokenLine(int _pieces, float _drawnShare)
{
  Neuron::MeshData line;
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  for (int piece = 0; piece < _pieces; ++piece)
  {
    const float start = static_cast<float>(piece) / static_cast<float>(_pieces);
    const float end = (static_cast<float>(piece) + _drawnShare) / static_cast<float>(_pieces);
    line.indices.push_back(static_cast<std::uint32_t>(line.vertices.size()));
    line.vertices.push_back({{start, 0.0f, 0.0f}, UP});
    line.indices.push_back(static_cast<std::uint32_t>(line.vertices.size()));
    line.vertices.push_back({{end, 0.0f, 0.0f}, UP});
  }
  line.boundsMin = {0.0f, 0.0f, 0.0f};
  line.boundsMax = {1.0f, 0.0f, 0.0f};
  return line;
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

// One copy of a mesh as the mesh pipeline draws it (ADR-053).
Neuron::MeshPipeline::Instance MeshInstance(const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4& _color, float _liftShare) noexcept
{
  return {.world = _world, .color = _color, .liftShare = _liftShare, .unused0 = 0.0f, .unused1 = 0.0f, .unused2 = 0.0f};
}

// The entity of _entities with this identifier, or nullptr. Every snapshot lists its entities in identifier order, and the
// view keeps that order (SnapshotInterpolator::Entities), so this is a binary search.
const Outpost::EntityView* FindById(std::span<const Outpost::EntityView> _entities, Outpost::EntityId _id) noexcept
{
  const auto found = std::ranges::lower_bound(_entities, _id, {}, &Outpost::EntityView::id);
  return found != _entities.end() && found->id == _id ? &*found : nullptr;
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

// A model's lines in its _color: made up to _brightness brighter, but no further than its brightest channel reaching 1,
// so that the hue holds, then _whiteShare of the way to a white as bright as that channel. A darker color, such as a
// structure's while it is built, gives darker lines.
DirectX::XMFLOAT4 EdgeColor(const DirectX::XMFLOAT4& _color, float _brightness, float _whiteShare) noexcept
{
  const float brightest = std::max({_color.x, _color.y, _color.z});
  if (brightest <= 0.0f)
    return _color;
  const float gain = std::min(_brightness, 1.0f / brightest);
  const float peak = brightest * gain;
  const auto channel = [gain, peak, _whiteShare](float _channel) { return std::lerp(_channel * gain, peak, _whiteShare); };
  return {channel(_color.x), channel(_color.y), channel(_color.z), _color.w};
}

// _color _share of the way to a gray as light as it is, by the luminance weights of linear sRGB.
DirectX::XMFLOAT4 TowardGray(const DirectX::XMFLOAT4& _color, float _share) noexcept
{
  const float lightness = (0.2126f * _color.x) + (0.7152f * _color.y) + (0.0722f * _color.z);
  return {std::lerp(_color.x, lightness, _share), std::lerp(_color.y, lightness, _share), std::lerp(_color.z, lightness, _share), _color.w};
}

// The color a model is drawn in: its set's, times its tint, and darker while it is built (task 4.2). A ship has no tint
// and is always built, so it is its set's color.
DirectX::XMFLOAT4 ModelColor(const DirectX::XMFLOAT4& _setColor, float _tint, std::int32_t _builtPermille) noexcept
{
  const float built = static_cast<float>(_builtPermille) / static_cast<float>(Outpost::PERMILLE);
  const float shade = _tint * (UNBUILT_SHADE + ((1.0f - UNBUILT_SHADE) * built));
  return {std::min(1.0f, _setColor.x * shade), std::min(1.0f, _setColor.y * shade), std::min(1.0f, _setColor.z * shade), _setColor.w};
}

// The wall clock, to the second, which a scheduled order's time of day is counted from (Phase 5 design §7).
std::chrono::sys_seconds WallClockNow() noexcept
{
  return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
}
} // namespace

Outpost::ClientAssets Outpost::LoadClientAssets()
{
  ClientAssets assets{.catalog = LoadDataFile(MODELS_FILE, LoadModelCatalog), .camera = LoadDataFile(CAMERA_FILE, LoadCameraSettings)};
  for (const ModelSet& set : assets.catalog.sets)
  {
    for (const ModelEntry& model : set.models)
    {
      // Every level of a model that grows, each after the first standing on the first's ground (ADR-064).
      std::optional<float> firstLevelWidest;
      for (int level = FIRST_MODEL_LEVEL; level <= ModelLevels(model); ++level)
      {
        const std::wstring fileName = ModelFileName(set, model, level);
        const Neuron::ByteBuffer bytes = ReadAsset(fileName);
        ClientAssets::Model& loaded = assets.models.emplace_back();
        loaded.shape = BuildModelMesh(bytes, model, std::format("Assets\\{}", winrt::to_string(fileName)), firstLevelWidest);
        if (!firstLevelWidest.has_value())
          firstLevelWidest = std::max(loaded.shape.Extents().x, loaded.shape.Extents().z);
        // The faces that stand still and each spinning part's (ADR-045), each with its creases, drawn over it as lines
        // (ADR-027).
        const auto addPiece = [&loaded](Neuron::MeshData _piece, const std::optional<Neuron::MeshPart>& _spin)
        {
          Neuron::MeshData edges = Neuron::BuildCreaseLines(_piece, CREASE_DEGREES * std::numbers::pi_v<float> / 180.0f);
          loaded.pieces.push_back({.faces = std::move(_piece), .edges = std::move(edges), .spin = _spin});
        };
        const Neuron::MeshData& data = loaded.shape;
        if (data.parts.empty())
          addPiece(data, std::nullopt);
        else
        {
          addPiece(Neuron::MeshPiece(data, 0, data.FixedIndexCount()), std::nullopt);
          for (const Neuron::MeshPart& part : data.parts)
            addPiece(Neuron::MeshPiece(data, part.firstIndex, part.indexCount), part);
        }
      }
    }
  }
  assets.sky = BuildStarfield();
  assets.particleSprite =
    Neuron::ParseDds(ReadAsset(PARTICLE_SPRITE_FILE), std::format("Assets\\{}", winrt::to_string(PARTICLE_SPRITE_FILE)));
  Neuron::BuildMipLevels(assets.particleSprite);
  return assets;
}

Neuron::UiAtlas Outpost::RasterizeInterface(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  return Neuron::RasterizeUiAtlas(Hud::Typefaces(), Hud::Sprites(), Hud::Scale(_widthPixels, _heightPixels));
}

Outpost::GameClient::GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond, ClientAssets _assets,
                                Neuron::UiAtlas _interface)
  : m_ticksPerSecond(_ticksPerSecond),
    m_catalog(std::move(_assets.catalog)),
    m_camera(_assets.camera),
    m_pipeline(_renderer),
    m_glows(_renderer),
    m_groundMask(_renderer),
    m_ui(_renderer, Hud::Typefaces(), Hud::Sprites(), std::move(_interface)),
    m_view(_ticksPerSecond),
    m_effects(_ticksPerSecond, m_catalog.shots),
    m_particles(_ticksPerSecond),
    m_explosions(_ticksPerSecond)
{
  // Gate H7: which face the figures found, Cascadia Mono or Consolas in its place (Phase 1 design §11).
  Neuron::DebugTrace(L"The interface's figures are set in {}.\n", m_ui.FamilyOf(static_cast<std::size_t>(Hud::Typeface::Figure)));
  // The models are kept in the catalog's order, which ModelIndex counts on, as LoadClientAssets made them.
  m_models.reserve(_assets.models.size());
  for (ClientAssets::Model& model : _assets.models)
  {
    LoadedModel& loaded = m_models.emplace_back();
    for (const ClientAssets::Piece& piece : model.pieces)
    {
      loaded.pieces.push_back(
        {.faces = std::make_unique<Neuron::Mesh>(_renderer, piece.faces),
         .edges = piece.edges.vertices.empty() ? std::unique_ptr<Neuron::Mesh>{} : std::make_unique<Neuron::Mesh>(_renderer, piece.edges),
         .spin = piece.spin});
    }
    loaded.hardpoints = std::move(model.shape.hardpoints);
    // Kept on the CPU too, for an explosion to break into its triangles (ADR-026).
    model.shape.hardpoints.clear();
    loaded.shape = std::move(model.shape);
  }
  m_grid = std::make_unique<Neuron::Mesh>(_renderer, BuildGrid());
  m_ring = std::make_unique<Neuron::Mesh>(_renderer, BuildRing());
  m_ringLine = std::make_unique<Neuron::Mesh>(_renderer, BuildRingLine());
  m_disc = std::make_unique<Neuron::Mesh>(_renderer, BuildDisc());
  m_strip = std::make_unique<Neuron::Mesh>(_renderer, BuildStrip());
  m_solidLine = std::make_unique<Neuron::Mesh>(_renderer, BuildBrokenLine(1, 1.0f));
  m_dashedLine = std::make_unique<Neuron::Mesh>(_renderer, BuildBrokenLine(TERRITORY_DASHES, TERRITORY_DASH_SHARE));
  m_dottedLine = std::make_unique<Neuron::Mesh>(_renderer, BuildBrokenLine(TERRITORY_DOTS, TERRITORY_DOT_SHARE));
  // How high each rock reaches over its center, for a Mining Rig to stand there; the top of its bounds if the line down
  // its center misses it.
  for (size_t index = 0; index < ROCK_MODELS.size(); ++index)
  {
    const Neuron::MeshData& rock = ModelShape(ASTEROID_SET, ROCK_MODELS[index]);
    m_rockTops[index] = Neuron::SurfaceHeightAt(rock, 0.0f, 0.0f).value_or(rock.boundsMax.y);
    m_rockReachShare = std::min(m_rockReachShare, FieldLayout::NarrowestReach(rock.vertices));
  }
  if (const StructureModel* rig = m_catalog.ModelForStructure(StructureKind::MiningRig))
  {
    for (const PlayerModels& player : m_catalog.players)
    {
      LoadedModel& loaded = m_models[ModelIndex(player.set, rig->model)];
      loaded.feet = RigFeet(loaded.shape);
    }
  }

  m_sky = std::make_unique<Neuron::StarPipeline>(_renderer, _assets.sky.points);
  m_bursts = std::make_unique<Neuron::StarPipeline>(_renderer, _assets.sky.bursts, Neuron::StarPipeline::Shape::Cross);
  m_particleSprites = std::make_unique<Neuron::GlowPipeline>(_renderer, &_assets.particleSprite);
  // The minimap's fog is the ground's own (ADR-052).
  m_ui.SetImage(m_groundMask.ShadesView());
}

void Outpost::GameClient::StartMatch(std::string _server)
{
  ClearMatch();
  m_server = std::move(_server);
  m_menuNotice.clear();
  m_screen = Screen::Match;
}

void Outpost::GameClient::ShowMenu(std::string _notice)
{
  ClearMatch();
  m_menuNotice = std::move(_notice);
  m_screen = Screen::Menu;
}

void Outpost::GameClient::ClearMatch()
{
  m_view = SnapshotInterpolator(m_ticksPerSecond);
  m_server.clear();
  m_silentSeconds = 0.0f;
  m_effects = CombatEffects(m_ticksPerSecond, m_catalog.shots);
  m_particles = ParticleSystem(m_ticksPerSecond);
  m_explosions = ExplosionManager(m_ticksPerSecond);
  m_particleGlows.clear();
  m_shardVertices.clear();
  m_shardBatches.clear();
  m_controls = PlayerControls();
  m_hovered.reset();
  m_designer = Designer();
  m_fog = FogOfWar();
  m_territory = {};
  m_territoryTick.reset();
  m_alerts.Reset();
  m_fogTick.reset();
  m_fogRevisionShown.reset();
  m_banking.Clear();
  m_windows.CloseAll();
  m_soleSelected = EntityId{};
  m_production = ProductionTarget();
  m_firstTopic = 0;
  m_orderForm = OrderForm();
  m_away.reset();
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
  if (!_snapshots.empty())
    m_silentSeconds = 0.0f;
  for (Snapshot& snapshot : _snapshots)
  {
    m_effects.Receive(snapshot);
    Explode(snapshot);
    m_alerts.Observe(snapshot, m_ticksPerSecond);
    // What happened while the player was away comes once, with its first snapshot after it takes its seat again (Phase 5
    // design §11).
    if (snapshot.away.has_value())
    {
      m_away = snapshot.away;
      m_awayTick = snapshot.tick;
      m_windows.Open(WindowKind::Away);
    }
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
  UpdateInterfaceFactor(_input);
  // The text is laid out with the fonts the frame draws it in, at this size and the interface's own scale (ADR-061,
  // ADR-070).
  const float scale = Hud::Scale(_viewportWidthPixels, _viewportHeightPixels, m_interfaceFactor);
  m_ui.UseScale(scale);
  const Hud::TextMetrics metrics(m_ui.Fonts(), scale);
  if (m_screen == Screen::Menu)
  {
    // The menu is all there is: a press on its buttons, and nothing for the camera or the controls.
    m_hudLayout = Hud::LayMenu(metrics, _viewportWidthPixels, _viewportHeightPixels, m_interfaceFactor,
                               {.joinWorld = m_offerWorld, .notice = m_menuNotice});
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

  m_everyHealthBar = _input.active && _input.IsDown(KEY_EVERY_HEALTH_BAR);
  m_view.Advance(_elapsedSeconds);
  m_silentSeconds += _elapsedSeconds;
  std::swap(m_previousEntities, m_entities);
  m_view.Entities(m_entities);
  if (!m_view.IsEmpty() && m_view.Newest().fogOfWar)
  {
    if (m_fog.CellsPerSide() == 0)
      m_fog.Reset(m_view.Newest().mapSizeMeters);
    // Once a snapshot: what the player sees changes no faster than the server ticks, and a cell is 20 m (ADR-052).
    if (m_fogTick != m_view.Newest().tick)
    {
      m_fog.Update(m_entities, m_view.Newest().player, m_view.Newest().sectors);
      m_fogTick = m_view.Newest().tick;
    }
  }
  // What the player may order on: the view, less the asteroids in space it has never seen, which it cannot claim with a
  // Mining Rig (ADR-046).
  m_knownEntities.clear();
  for (const EntityView& entity : m_entities)
  {
    if (entity.kind != EntityKind::Asteroid || m_fog.HasSeen(entity.position, entity.radiusMeters))
      m_knownEntities.push_back(entity);
  }
  UpdateBanking(_elapsedSeconds);
  m_frameSeconds = _elapsedSeconds;
  m_effectDraws = m_effects.At(
    m_view.ViewTick(), [this](EntityId _shooter, PlanePosition _target, std::uint8_t _gun) { return MuzzleOf(_shooter, _target, _gun); },
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
    m_production.Update(m_view.Newest());
    m_orderForm.Update(m_view.Newest(), m_knownEntities);
    for (const QueueShipCommand& queue : m_designer.TakeQueueCommands(m_view.Newest()))
      m_controls.Queue(queue.producer, queue.design);
  }
  HandleTyping(input);
  // Esc closes the front window, and then is the window's, not the controls' (Phase 1 design §12). D, P and R open or close
  // the designer, the production window and the research window, F1 the Controls window (task 16.4), and O the orders
  // window (Phase 5 design §11). Space moves the camera to the newest alert (ADR-059).
  for (auto event = input.events.begin(); event != input.events.end();)
  {
    const bool keyDown = event->kind == Neuron::InputEventKind::KeyDown;
    std::optional<WindowKind> toggled;
    if (keyDown && event->key == KEY_DESIGNER)
      toggled = WindowKind::Designer;
    else if (keyDown && event->key == KEY_PRODUCTION)
      toggled = WindowKind::Production;
    else if (keyDown && event->key == KEY_RESEARCH)
      toggled = WindowKind::Research;
    else if (keyDown && event->key == KEY_CONTROLS)
      toggled = WindowKind::Controls;
    else if (keyDown && event->key == KEY_ORDERS)
      toggled = WindowKind::Orders;
    if (keyDown && event->key == KEY_CANCEL && m_windows.CloseFront())
      event = input.events.erase(event);
    else if (keyDown && event->key == KEY_LATEST_ALERT)
    {
      if (const Alerts::Alert* alert = m_alerts.Newest())
        m_camera.SetFocus(alert->position.xMeters, alert->position.zMeters);
      event = input.events.erase(event);
    }
    else if (toggled.has_value() && !m_view.IsEmpty())
    {
      ToggleWindow(*toggled);
      event = input.events.erase(event);
    }
    else
      ++event;
  }
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
    m_controls.Update(input, m_knownEntities, m_view.Newest().player, m_camera, m_viewport);
    const DirectX::XMFLOAT2 cursor{static_cast<float>(input.cursorXPixels), static_cast<float>(input.cursorYPixels)};
    m_hovered = m_hudLayout.Covers(cursor.x, cursor.y)
                  ? std::nullopt
                  : PickEntity(m_entities, m_camera, m_viewport, cursor, [](const EntityView& _entity)
                               { return _entity.kind == EntityKind::Structure || _entity.kind == EntityKind::Derelict; });
    const std::vector<PlanePosition> view = ViewOnGround();
    // Selecting a Shipyard while the designer is open aims it there, and a producer while the production window is open
    // shows it (Phase 1 design §11, §12).
    const std::vector<EntityId>& selected = m_controls.Selected();
    const EntityId sole = selected.size() == 1 ? selected.front() : EntityId{};
    if (sole != m_soleSelected && m_windows.IsOpen(WindowKind::Designer))
      m_designer.SetTarget(sole, m_view.Newest());
    if (sole != m_soleSelected && m_windows.IsOpen(WindowKind::Production))
      m_production.Set(sole, m_view.Newest());
    m_soleSelected = sole;
    // The name takes no more typing once the designer is closed.
    const bool designerOpen = m_windows.IsOpen(WindowKind::Designer);
    if (!designerOpen)
      m_designer.EndEditing();
    const std::optional<Hud::Action> hovered =
      m_hudLayout.ActionAt(static_cast<float>(input.cursorXPixels), static_cast<float>(input.cursorYPixels));
    // The order the next left-click gives, whose button the HUD lights (interface plan 2, task UI4.2).
    std::optional<Hud::ActionKind> armed;
    if (m_controls.IsAttackMoveArmed())
      armed = Hud::ActionKind::AttackMove;
    else if (const std::optional<StandingOrder> standing = m_controls.ArmedStanding(); standing.has_value())
      armed = *standing == StandingOrder::HoldSector ? Hud::ActionKind::HoldSector : Hud::ActionKind::Patrol;
    Hud::Content content = Hud::Describe(m_view.Newest(), m_entities, selected, m_controls.Placing(), designerOpen ? &m_designer : nullptr,
                                         hovered, m_ticksPerSecond, armed);
    content.fog = m_fog.CellsPerSide() > 0;
    // The bars over the entities that show one, each placed by the camera above its model on the screen, for the HUD to lay
    // out (interface plan 2, task UI4.1).
    for (const EntityView& entity : m_entities)
    {
      const ModelSet* side = m_catalog.SetForPlayer(entity.owner);
      const DirectX::XMFLOAT4 back = side != nullptr ? Shaded(side->color, HEALTH_BACK_SHADE) : HEALTH_BACK_COLOR;
      const std::optional<Hud::EntityBar> shown = Hud::BarOver(entity, {}, back, m_everyHealthBar);
      if (!shown.has_value())
        continue;
      const std::array<DirectX::XMFLOAT3, 8> corners = BoundsCorners(entity);
      const std::optional<DirectX::XMFLOAT2> foot = m_camera.PixelAbove(entity.position, corners, m_viewport);
      if (!foot.has_value())
        continue;
      Hud::EntityBar bar = shown.value_or(Hud::EntityBar{});
      bar.footPixels = foot.value_or(DirectX::XMFLOAT2{});
      content.entityBars.push_back(bar);
    }
    content.connection = Hud::Connection{.server = m_server, .silentSeconds = static_cast<std::int32_t>(m_silentSeconds)};
    // What a derelict under the pointer holds (Phase 4 design §13), and how long ago a structure the player only remembers
    // was seen (interface plan 2, task UI1.2), unless a placement's hint says more.
    if (const auto pointed = std::ranges::find(m_entities, m_hovered.value_or(EntityId{}), &EntityView::id);
        content.hint.empty() && pointed != m_entities.end())
    {
      if (pointed->kind == EntityKind::Derelict)
        content.hint = Hud::DescribeDerelict(m_view.Newest(), *pointed, m_ticksPerSecond);
      else if (pointed->remembered)
        content.hint = Hud::DescribeMemory(m_view.Newest(), *pointed, m_ticksPerSecond);
    }
    for (const Alerts::Alert& alert : m_alerts.Shown(m_view.Newest().tick, m_ticksPerSecond))
      content.alerts.emplace_back(alert.text, alert.position);
    content.outcome = Hud::DescribeOutcome(m_view.Newest(), m_ticksPerSecond);
    if (m_windows.IsOpen(WindowKind::Production))
      content.production = Hud::DescribeProduction(m_view.Newest(), m_production.Target(m_view.Newest()));
    if (m_windows.IsOpen(WindowKind::Research))
    {
      content.laboratory = Hud::DescribeResearch(m_view.Newest(), m_entities, m_firstTopic);
      m_firstTopic = content.laboratory->firstTopic;
    }
    content.controls = m_windows.IsOpen(WindowKind::Controls);
    if (m_windows.IsOpen(WindowKind::Orders))
      content.orders = Hud::DescribeOrders(m_view.Newest(), m_knownEntities, selected, m_orderForm, m_clock, WallClockNow());
    // The report goes once its window is closed.
    if (m_away.has_value() && !m_windows.IsOpen(WindowKind::Away))
      m_away.reset();
    if (m_away.has_value())
      content.away = Hud::DescribeAway(m_view.Newest(), *m_away, m_awayTick, m_ticksPerSecond);
    // The scheduled order the selected ships wait on, on their panel (design §11).
    if (std::optional<std::string> pending = PendingOrderLine(selected, m_entities, m_view.Newest(), m_clock);
        pending.has_value() && !content.selection.empty())
      content.selection.push_back(std::move(*pending));
    m_hudLayout = Hud::Lay(content, metrics, _viewportWidthPixels, _viewportHeightPixels, view, &m_windows, m_interfaceFactor, hovered);
    for (const Hud::Window& window : m_hudLayout.windows)
      m_windows.Settle(window.kind, window.corner);
  }
  WatchForResponse();
}

void Outpost::GameClient::UpdateInterfaceFactor(const Neuron::InputState& _input)
{
  // Ctrl+= and Ctrl+- step the interface's own scale. It is checked again whenever the screen's size, or whether there is a
  // match to size the designer by, changes, so that its windows still fit; it lasts until the game closes (ADR-070).
  int step = 0;
  if (_input.active)
  {
    for (const Neuron::InputEvent& event : _input.events)
    {
      if (event.kind == Neuron::InputEventKind::KeyDown && event.control)
        step += event.key == KEY_LARGER_INTERFACE ? 1 : event.key == KEY_SMALLER_INTERFACE ? -1 : 0;
    }
  }
  const Snapshot* newest = m_screen == Screen::Match && !m_view.IsEmpty() ? &m_view.Newest() : nullptr;
  const InterfaceFit fit{.widthPixels = m_viewport.widthPixels, .heightPixels = m_viewport.heightPixels, .match = newest != nullptr};
  if (step == 0 && fit == m_interfaceFit)
    return;
  m_interfaceFit = fit;
  m_interfaceFactor =
    Hud::StepInterface(m_interfaceFactor, std::clamp(step, -1, 1), newest, m_viewport.widthPixels, m_viewport.heightPixels);
}

void Outpost::GameClient::ToggleWindow(WindowKind _window)
{
  if (m_windows.IsOpen(_window))
  {
    m_windows.Close(_window);
    return;
  }
  m_windows.Open(_window);
  // A window that shows a structure opens on the one selected, if it can show it.
  if (m_controls.Selected().size() != 1)
    return;
  const EntityId selected = m_controls.Selected().front();
  if (_window == WindowKind::Designer)
    m_designer.SetTarget(selected, m_view.Newest());
  else if (_window == WindowKind::Production)
    m_production.Set(selected, m_view.Newest());
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
    for (std::uint32_t ship = 0; ship < _action.count; ++ship)
      m_controls.Queue(_action.producer, _action.design);
    break;
  case Hud::ActionKind::Research:
    m_controls.Research(_action.producer, _action.topic);
    break;
  case Hud::ActionKind::Upgrade:
    m_controls.Upgrade(_action.producer);
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
  case Hud::ActionKind::PickModule:
    m_designer.PickModule(_action.module);
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
    if (std::optional<SaveDesignCommand> save = m_designer.SaveAndQueue(_action.producer, m_view.Newest(), _action.count))
    {
      m_controls.SaveDesign(std::move(*save));
      m_designer.ForgetTypedName();
    }
    break;
  case Hud::ActionKind::StartSkirmish:
    m_request = Request::StartSkirmish;
    m_difficulty = _action.difficulty;
    break;
  case Hud::ActionKind::Quit:
    m_request = Request::Quit;
    break;
  case Hud::ActionKind::BackToMenu:
    m_request = Request::BackToMenu;
    break;
  case Hud::ActionKind::JoinWorld:
    m_request = Request::JoinWorld;
    break;
  case Hud::ActionKind::OpenDesigner:
    m_designer.SetTarget(_action.producer, m_view.Newest());
    m_windows.Open(WindowKind::Designer);
    break;
  case Hud::ActionKind::PreviousShipyard:
  case Hud::ActionKind::NextShipyard:
    m_designer.StepTarget(_action.kind == Hud::ActionKind::NextShipyard ? 1 : -1, m_view.Newest());
    break;
  case Hud::ActionKind::FewerShips:
  case Hud::ActionKind::MoreShips:
    m_designer.StepCount(_action.kind == Hud::ActionKind::MoreShips ? 1 : -1, m_view.Newest());
    break;
  case Hud::ActionKind::LoadDesign:
    if (const auto design = std::ranges::find(m_view.Newest().designs, _action.design, &DesignView::id);
        design != m_view.Newest().designs.end())
      m_designer.Load(*design);
    break;
  case Hud::ActionKind::PreviousDesigns:
  case Hud::ActionKind::NextDesigns:
    m_designer.StepChips(_action.kind == Hud::ActionKind::NextDesigns ? 1 : -1, m_view.Newest().designs.size());
    break;
  case Hud::ActionKind::OpenProduction:
    m_production.Set(_action.producer, m_view.Newest());
    m_windows.Open(WindowKind::Production);
    break;
  case Hud::ActionKind::OpenResearch:
    m_windows.Open(WindowKind::Research);
    break;
  case Hud::ActionKind::PreviousProducer:
  case Hud::ActionKind::NextProducer:
    m_production.Step(_action.kind == Hud::ActionKind::NextProducer ? 1 : -1, m_view.Newest());
    break;
  case Hud::ActionKind::PreviousTopics:
  case Hud::ActionKind::NextTopics:
    m_firstTopic = Hud::StepTopics(m_firstTopic, _action.kind == Hud::ActionKind::NextTopics ? 1 : -1,
                                   Hud::DescribeResearch(m_view.Newest(), m_entities, m_firstTopic).topics.size());
    break;
  case Hud::ActionKind::DesignRetreat:
    m_designer.SetRetreat(_action.retreat);
    break;
  case Hud::ActionKind::SetRetreat:
    m_controls.SetRetreat(_action.retreat, m_entities);
    break;
  case Hud::ActionKind::AttackMove:
    m_controls.ArmAttackMove(m_entities);
    break;
  case Hud::ActionKind::HoldSector:
    m_controls.ArmStanding(StandingOrder::HoldSector, m_entities);
    break;
  case Hud::ActionKind::Patrol:
    m_controls.ArmStanding(StandingOrder::Patrol, m_entities);
    break;
  case Hud::ActionKind::Stop:
    m_controls.Stop(m_entities);
    break;
  case Hud::ActionKind::StepOrder:
    m_orderForm.Step(_action.field, _action.step, m_view.Newest(), m_knownEntities);
    break;
  case Hud::ActionKind::GiveOrder:
    m_controls.Schedule(m_orderForm, WallClockNow(), m_clock, m_view.Newest(), m_knownEntities);
    break;
  case Hud::ActionKind::Select:
    if (const auto selected = std::ranges::find(m_entities, _action.entity, &EntityView::id); selected != m_entities.end())
    {
      m_controls.SelectAlone(selected->id);
      m_camera.SetFocus(selected->position.xMeters, selected->position.zMeters);
    }
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
    {
      m_minimapDragging = false;
      m_windows.Release();
    }
    if (event.kind != Neuron::InputEventKind::ButtonDown)
      continue;
    if (const Hud::Window* window = m_hudLayout.WindowAt(x, y); window != nullptr && event.key == VK_LBUTTON)
    {
      if (window->closeBox.Contains(x, y))
      {
        m_windows.Close(window->kind);
        continue;
      }
      // A button in the title bar is pressed, not grabbed.
      if (window->titleBar.Contains(x, y) && !m_hudLayout.ActionAt(x, y).has_value())
      {
        m_windows.Grab(window->kind, ToUnits(x, y), window->corner);
        continue;
      }
      m_windows.Open(window->kind);
    }
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
  if (m_windows.IsDragging() && _input.IsDown(VK_LBUTTON))
    m_windows.Drag(ToUnits(static_cast<float>(_input.cursorXPixels), static_cast<float>(_input.cursorYPixels)));
  if (m_minimapDragging && _input.IsDown(VK_LBUTTON))
  {
    if (const std::optional<PlanePosition> point =
          m_hudLayout.MapPointAt(static_cast<float>(_input.cursorXPixels), static_cast<float>(_input.cursorYPixels)))
      m_camera.SetFocus(point->xMeters, point->zMeters);
  }
}

Outpost::WindowManager::Point Outpost::GameClient::ToUnits(float _xPixels, float _yPixels) const noexcept
{
  const float scale = Hud::Scale(m_viewport.widthPixels, m_viewport.heightPixels, m_interfaceFactor);
  return {.xUnits = _xPixels / scale, .yUnits = _yPixels / scale};
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
  constants.eyePosition = m_camera.EyePosition(aspectRatio);

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
  // The grid is not pulled toward the eye: nothing lies on it, and pulled, it would show through the hulls it cuts.
  m_pipeline.DrawLines(_commandList, *m_grid, identity, GRID_COLOR, 0.0f);
  // The menu shows over the empty grid and the sky.
  if (m_screen == Screen::Menu)
    return;
  m_faceDraws.clear();
  m_lineDraws.clear();
  // What the player only remembers is drawn over the fog, after the rest (DrawMemories).
  for (const EntityView& entity : m_entities)
  {
    if (!entity.remembered)
      QueueEntity(entity);
  }
  // Every model's faces, each mesh's copies in one draw (ADR-053).
  m_pipeline.DrawMeshes(_commandList, m_faceDraws);
  DrawShards(_commandList);
  // Every model's lines at once, after every face they may lie behind.
  m_pipeline.DrawLines(_commandList, m_lineDraws);
  DrawFootprints(_commandList);
  DrawSelection(_commandList);
  DrawGhost(_commandList);
  DrawEffects(_commandList);
  DrawGlows(_renderer, _commandList);
  DrawFog(_renderer, _commandList);
  // Over the fog, what every player sees whatever the fog, the territory, and what the player only remembers, which the fog
  // would otherwise dim as it dims the ground round it (interface plan 2, tasks UI1.1 and UI1.2).
  m_pipeline.Resume(_commandList);
  DrawTerritory(_commandList);
  DrawMemories(_commandList);
}

void Outpost::GameClient::DrawMemories(ID3D12GraphicsCommandList* _commandList)
{
  m_memoryDraws.clear();
  for (const EntityView& entity : m_entities)
  {
    if (!entity.remembered)
      continue;
    const std::optional<PlacedModel> placed = PlaceModel(entity);
    if (!placed.has_value())
      continue;
    const DirectX::XMFLOAT4 color = Shaded(TowardGray(placed->set->color, MEMORY_GRAY_SHARE), MEMORY_SHADE);
    const DirectX::XMFLOAT4X4 world = placed->World();
    for (const ModelPiece& piece : ModelPieces(placed->set->name, *placed->model, placed->level))
    {
      if (piece.edges != nullptr)
        m_memoryDraws.push_back({.mesh = piece.edges.get(), .instance = MeshInstance(PieceWorld(piece, world), color, LINE_LIFT_SHARE)});
    }
  }
  m_pipeline.DrawLines(_commandList, m_memoryDraws);
}

void Outpost::GameClient::DrawTerritory(ID3D12GraphicsCommandList* _commandList)
{
  if (m_view.IsEmpty())
    return;
  const Snapshot& newest = m_view.Newest();
  // Once a snapshot: who holds what, and which nodes the player could claim, change no faster than the server ticks.
  if (m_territoryTick != newest.tick)
  {
    m_territory = MarkTerritory(newest, m_entities);
    m_territoryTick = newest.tick;
  }
  m_territoryDraws.clear();
  for (const TerritoryMarks::Line& line : m_territory.lines)
  {
    DirectX::XMFLOAT4 color = TERRITORY_LATTICE_COLOR;
    if (line.look != TerritoryMarks::Look::Neutral)
    {
      const ModelSet* side = m_catalog.SetForPlayer(line.holder);
      if (side == nullptr)
        continue;
      color = Shaded(side->color, TERRITORY_OUTLINE_SHADE);
    }
    const float dx = line.to.xMeters - line.from.xMeters;
    const float dz = line.to.zMeters - line.from.zMeters;
    const float length = std::hypot(dx, dz);
    if (length <= 0.0f)
      continue;
    const Neuron::Mesh* mesh = line.pattern == TerritoryMarks::Pattern::Dashed   ? m_dashedLine.get()
                               : line.pattern == TerritoryMarks::Pattern::Dotted ? m_dottedLine.get()
                                                                                 : m_solidLine.get();
    const DirectX::XMFLOAT3 from{line.from.xMeters, OVERLAY_LIFT_METERS, line.from.zMeters};
    m_territoryDraws.push_back({.mesh = mesh, .instance = MeshInstance(WorldMatrix(from, std::atan2(dz, dx), length), color, 0.0f)});
  }
  // While the player places a Relay, two rings on each node it could claim now, the outer a Relay's footprint ring, so that
  // the Relay's ghost stands in it.
  const auto relay = std::ranges::find(newest.structureTypes, StructureKind::Relay, &StructureTypeView::structure);
  const float radius = (relay != newest.structureTypes.end() ? relay->radiusMeters : 0.0f) * RING_SIZE_PER_FOOTPRINT;
  const ModelSet* own = m_catalog.SetForPlayer(newest.player);
  if (m_controls.Placing() == StructureKind::Relay && radius > 0.0f && own != nullptr)
  {
    const DirectX::XMFLOAT4 color = EdgeColor(own->color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE);
    for (const PlanePosition node : m_territory.claimable)
    {
      const DirectX::XMFLOAT3 at{node.xMeters, OVERLAY_LIFT_METERS, node.zMeters};
      m_territoryDraws.push_back({.mesh = m_ringLine.get(), .instance = MeshInstance(WorldMatrix(at, 0.0f, radius), color, 0.0f)});
      m_territoryDraws.push_back(
        {.mesh = m_ringLine.get(), .instance = MeshInstance(WorldMatrix(at, 0.0f, radius * CLAIMABLE_INNER_SHARE), color, 0.0f)});
    }
  }
  m_pipeline.DrawLines(_commandList, m_territoryDraws);
}

void Outpost::GameClient::RenderInterface(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex)
{
  DrawHud(_commandList, _frameIndex);
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
                                                             .cellsPerSide = m_fog.CellsPerSide(),
                                                             // What was seen before, darker on the ground than on the minimap
                                                             // (interface plan 2, task UI1.3).
                                                             .kneeShade = FogOfWar::SEEN_BEFORE_SHADE,
                                                             .kneeOpacity = FogOfWar::GROUND_SEEN_BEFORE_SHADE};
  // The shades go to the GPU only when they change, and only the rows that did; the minimap samples the same texture
  // (ADR-052).
  if (m_fogRevisionShown != m_fog.Revision())
  {
    m_groundMask.SetShades(_commandList, _renderer.FrameIndex(), m_fog.Shades(), m_fog.CellsPerSide(), m_fog.ChangedRows());
    m_fog.ClearChangedRows();
    m_fogRevisionShown = m_fog.Revision();
  }
  m_groundMask.Draw(_commandList, constants);
}

void Outpost::GameClient::DrawHud(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex)
{
  // Nothing to show before the first snapshot has been laid out.
  if (m_hudLayout.fontPixels <= 0.0f)
    return;
  // The layout's own scale, which its font size was set by.
  const float scale = m_hudLayout.fontPixels / Hud::FONT_UNITS;
  m_ui.Begin(m_viewport.widthPixels, m_viewport.heightPixels, scale);
  // The bars over the entities, under every panel (interface plan 2, task UI4.1).
  for (const Hud::Rect& bar : m_hudLayout.bars)
    m_ui.FillRect(bar.left, bar.top, bar.width, bar.height, bar.color);
  // The HUD, then each window back to front: each layer's panels, then its lines, then its sprites, then its texts (ADR-031).
  for (std::size_t layer = 0; layer < m_hudLayout.LayerCount(); ++layer)
  {
    const Hud::Span panels = m_hudLayout.PanelsOf(layer);
    for (std::size_t i = panels.first; i < panels.end; ++i)
    {
      const Hud::Rect& panel = m_hudLayout.panels[i];
      if (panel.fill == Hud::Fill::Hatched)
        m_ui.FillHatched(panel.left, panel.top, panel.width, panel.height, panel.color, Hud::HATCH_PERIOD_UNITS * scale,
                         Hud::HATCH_STRIPE_UNITS * scale);
      else if (panel.fill == Hud::Fill::Fog)
      {
        // The fog's texture holds the grid in its corner, cell (x, z) at texel (x, z), and the minimap's top is the map's
        // far edge in z (ADR-052).
        const float extent = static_cast<float>(m_fog.CellsPerSide()) / static_cast<float>(Neuron::GroundMaskPipeline::TEXTURE_SIDE);
        m_ui.DrawImage(panel.left, panel.top, panel.width, panel.height, panel.color, 0.0f, extent, extent, 0.0f);
      }
      else
        m_ui.FillRect(panel.left, panel.top, panel.width, panel.height, panel.color);
    }
    const Hud::Span lines = m_hudLayout.LinesOf(layer);
    for (std::size_t i = lines.first; i < lines.end; ++i)
      m_ui.DrawSegment(m_hudLayout.lines[i].segment, m_hudLayout.lines[i].color);
    const Hud::Span sprites = m_hudLayout.SpritesOf(layer);
    for (std::size_t i = sprites.first; i < sprites.end; ++i)
    {
      const Hud::SpriteMark& mark = m_hudLayout.sprites[i];
      m_ui.DrawSprite(static_cast<std::size_t>(mark.sprite), mark.area.left, mark.area.top, mark.area.width, mark.area.height,
                      mark.area.color, mark.mirrorX, mark.mirrorY);
    }
    const Hud::Span texts = m_hudLayout.TextsOf(layer);
    for (std::size_t i = texts.first; i < texts.end; ++i)
    {
      const Hud::Text& text = m_hudLayout.texts[i];
      m_ui.DrawText(static_cast<std::size_t>(text.typeface), text.text, text.left, text.top, text.color, text.trackingPixels);
    }
  }
  m_ui.End(_commandList, _frameIndex);
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

void Outpost::GameClient::DrawFootprints(ID3D12GraphicsCommandList* _commandList)
{
  const std::vector<EntityId>& selected = m_controls.Selected();
  // Placing a structure, the player is choosing where it stands among the others, so every ring is at full strength.
  const bool placing = m_controls.Placing().has_value();
  m_ringDraws.clear();
  for (const EntityView& structure : m_entities)
  {
    // A memory stands on no ring: it may not be there (interface plan 2, task UI1.2).
    if (structure.kind != EntityKind::Structure || structure.remembered || std::ranges::find(selected, structure.id) != selected.end())
      continue;
    const ModelSet* side = m_catalog.SetForPlayer(structure.owner);
    if (side == nullptr)
      continue;
    const bool lit = placing || m_hovered == structure.id;
    const DirectX::XMFLOAT4 litColor = EdgeColor(side->color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE);
    // The rig's ring lies over the rock, so it is pulled toward the eye as a model's lines are, to show over the faces it
    // lies on. In a frame whose shards have taken every vertex DrawLineList can use, the rig goes without it.
    if (structure.structure == StructureKind::MiningRig)
    {
      if (lit && DrapeRigRing(structure))
        m_pipeline.DrawLineList(_commandList, m_drapedRing, litColor, LINE_LIFT_SHARE);
      continue;
    }
    const float radius = structure.radiusMeters * RING_SIZE_PER_FOOTPRINT;
    const float viewShare = radius / m_camera.ViewWidthMeters();
    const float strength =
      lit ? 1.0f : std::clamp((RING_GONE_VIEW_SHARE - viewShare) / (RING_GONE_VIEW_SHARE - RING_FULL_VIEW_SHARE), 0.0f, 1.0f);
    if (strength <= 0.0f)
      continue;
    const DirectX::XMFLOAT3 at{structure.position.xMeters, OVERLAY_LIFT_METERS, structure.position.zMeters};
    m_ringDraws.push_back({.mesh = m_ringLine.get(),
                           .instance = MeshInstance(WorldMatrix(at, 0.0f, radius),
                                                    lit ? litColor : Shaded(side->color, FOOTPRINT_RING_SHADE * strength), 0.0f)});
  }
  m_pipeline.DrawLines(_commandList, m_ringDraws);
}

bool Outpost::GameClient::DrapeRigRing(const EntityView& _rig)
{
  if (m_view.IsEmpty())
    return false;
  const Snapshot& newest = m_view.Newest();
  const auto type = std::ranges::find(newest.structureTypes, StructureKind::MiningRig, &StructureTypeView::structure);
  if (type == newest.structureTypes.end())
    return false;
  // The rig stands at its asteroid's center, and its entity's radius is the rock's (design §6). Its ring is its own
  // footprint's, as every structure's is.
  const float radius = type->radiusMeters * RING_SIZE_PER_FOOTPRINT;
  const float rockRadius = _rig.radiusMeters;
  const Neuron::MeshData& rock = ModelShape(ASTEROID_SET, RockModel(rockRadius));
  const auto asteroid = std::ranges::find_if(m_entities, [&_rig](const EntityView& _entity)
                                             { return _entity.kind == EntityKind::Asteroid && _entity.position == _rig.position; });
  // A point of the ring lies over the rock's mesh where its asteroid's turn, as WorldMatrix turns it, takes it back to.
  const DirectX::XMMATRIX toRock = DirectX::XMMatrixRotationY(asteroid != m_entities.end() ? asteroid->headingRadians : 0.0f);
  constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
  std::array<DirectX::XMFLOAT3, static_cast<size_t>(RING_LINE_SEGMENTS)> points{};
  for (int i = 0; i < RING_LINE_SEGMENTS; ++i)
  {
    const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / RING_LINE_SEGMENTS;
    const float x = radius * std::cos(angle);
    const float z = radius * std::sin(angle);
    const DirectX::XMVECTOR over = DirectX::XMVector3Transform(DirectX::XMVectorSet(x / rockRadius, 0.0f, z / rockRadius, 0.0f), toRock);
    // On the rock's surface where it is over the rock, and on the ground past its edge.
    const std::optional<float> surface = Neuron::SurfaceHeightAt(rock, DirectX::XMVectorGetX(over), DirectX::XMVectorGetZ(over));
    const float height = std::max(surface.value_or(0.0f) * rockRadius, 0.0f) + OVERLAY_LIFT_METERS;
    points[static_cast<size_t>(i)] = {_rig.position.xMeters + x, height, _rig.position.zMeters + z};
  }
  m_drapedRing.clear();
  for (size_t i = 0; i < points.size(); ++i)
  {
    m_drapedRing.push_back({points[i], UP});
    m_drapedRing.push_back({points[(i + 1) % points.size()], UP});
  }
  return true;
}

void Outpost::GameClient::DrawSelection(ID3D12GraphicsCommandList* _commandList)
{
  const DirectX::XMFLOAT4& ringColor =
    m_controls.IsAttackMoveArmed() || m_controls.ArmedStanding().has_value() ? ATTACK_MOVE_COLOR : SELECTION_COLOR;
  const float hudScale = Hud::Scale(m_viewport.widthPixels, m_viewport.heightPixels);
  const auto lines = static_cast<int>(std::max(1L, std::lround(SELECTION_RING_UNITS * hudScale)));
  m_selectionDraws.clear();
  for (const EntityId id : m_controls.Selected())
  {
    const EntityView* selected = FindById(m_entities, id);
    if (selected == nullptr)
      continue;
    // Nothing for a point behind the camera.
    const float metersPerPixel = m_camera.MetersPerPixelAt(selected->position, m_viewport).value_or(0.0f);
    if (metersPerPixel <= 0.0f)
      continue;
    const DirectX::XMFLOAT3 at{selected->position.xMeters, OVERLAY_LIFT_METERS, selected->position.zMeters};
    const float radius = selected->radiusMeters * RING_SIZE_PER_FOOTPRINT;
    for (int line = 0; line < lines; ++line)
    {
      const float offset = (static_cast<float>(line) - (static_cast<float>(lines - 1) / 2.0f)) * metersPerPixel;
      m_selectionDraws.push_back(
        {.mesh = m_ringLine.get(), .instance = MeshInstance(WorldMatrix(at, 0.0f, std::max(radius + offset, 0.0f)), ringColor, 0.0f)});
    }
  }
  m_pipeline.DrawLines(_commandList, m_selectionDraws);

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

void Outpost::GameClient::QueueEntity(const EntityView& _entity)
{
  const DirectX::XMFLOAT3 position{_entity.position.xMeters, 0.0f, _entity.position.zMeters};
  switch (_entity.kind)
  {
  case EntityKind::Ship:
  {
    // The data maps every player and hull the server can send, and the Constructor (ModelCatalog); anything else is not
    // drawn.
    if (const std::optional<PlacedModel> placed = PlaceModel(_entity))
      QueueModel(placed->set->name, *placed->model, PoseMatrix(placed->pose), placed->set->color, FILL_SHADE, placed->level);
    break;
  }
  case EntityKind::Asteroid:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    QueueModel(ASTEROID_SET, RockModel(_entity.radiusMeters), WorldMatrix(position, _entity.headingRadians, _entity.radiusMeters),
               set.color, ROCK_FILL_SHADE);
    break;
  }
  case EntityKind::AsteroidField:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    const DirectX::XMFLOAT4 color{set.color.x * FIELD_SHADE, set.color.y * FIELD_SHADE, set.color.z * FIELD_SHADE, set.color.w};
    // The ring is closed to the smallest hull's footprint, a Small hull's; with no hulls known, it is left as drawn.
    float gap = std::numeric_limits<float>::max();
    if (!m_view.IsEmpty())
    {
      for (const HullView& hull : m_view.Newest().hulls)
        gap = std::min(gap, 2.0f * static_cast<float>(hull.footprintRadiusMeters));
    }
    const FieldLayout layout = FieldLayout::Of(_entity.id, _entity.radiusMeters, m_rockReachShare, gap);
    const auto queueRock = [&](const FieldRock& _rock)
    {
      const DirectX::XMFLOAT3 at{position.x + _rock.xMeters, 0.0f, position.z + _rock.zMeters};
      QueueModel(ASTEROID_SET, RockModel(_rock.radiusMeters), WorldMatrix(at, _rock.turnRadians, _rock.radiusMeters), color,
                 ROCK_FILL_SHADE);
    };
    queueRock(layout.center);
    for (const FieldRock& rock : layout.ring)
      queueRock(rock);
    break;
  }
  case EntityKind::Derelict:
  {
    // A wreck is drawn with its hull's model across its radius, in the derelicts' gray (ADR-074).
    if (const std::optional<PlacedModel> placed = PlaceModel(_entity))
      QueueModel(placed->set->name, *placed->model, PoseMatrix(placed->pose), placed->set->color, FILL_SHADE, placed->level);
    break;
  }
  case EntityKind::Structure:
  default:
    QueueStructure(_entity);
    break;
  }
}

void Outpost::GameClient::QueueModel(std::string_view _set, std::string_view _model, const DirectX::XMFLOAT4X4& _world,
                                     const DirectX::XMFLOAT4& _color, float _fillShade, int _level)
{
  const bool rock = _set == ASTEROID_SET;
  const DirectX::XMFLOAT4 edgeColor =
    rock ? EdgeColor(_color, ROCK_EDGE_BRIGHTNESS, 0.0f) : EdgeColor(_color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE);
  for (const ModelPiece& piece : ModelPieces(_set, _model, _level))
  {
    const DirectX::XMFLOAT4X4 world = PieceWorld(piece, _world);
    m_faceDraws.push_back({.mesh = piece.faces.get(), .instance = MeshInstance(world, Shaded(_color, _fillShade), 0.0f)});
    if (piece.edges != nullptr)
      m_lineDraws.push_back({.mesh = piece.edges.get(), .instance = MeshInstance(world, edgeColor, LINE_LIFT_SHARE)});
  }
}

void Outpost::GameClient::QueueStructure(const EntityView& _entity)
{
  const std::optional<PlacedModel> placed = PlaceModel(_entity);
  if (!placed.has_value())
    return;
  QueueModel(placed->set->name, *placed->model, placed->World(),
             TowardGray(ModelColor(placed->set->color, placed->tint, _entity.builtPermille), STRUCTURE_GRAY_SHARE), STRUCTURE_FILL_SHADE,
             placed->level);
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
      if (const EntityView* found = FindById(m_view.Newest().entities, destroyed.id))
        entity = *found;
    }
    entity.position = destroyed.position;
    entity.headingRadians = destroyed.headingRadians;
    entity.radiusMeters = destroyed.radiusMeters;

    // The same destruction always looks the same.
    const std::uint64_t seed = (std::uint64_t{destroyed.id.value} << 32U) ^ _snapshot.tick;
    const std::optional<PlacedModel> placed = PlaceModel(entity);
    const float liftMeters = placed.has_value() ? placed->OriginLiftMeters() : 0.0f;
    m_particles.AddBlast({destroyed.position.xMeters, liftMeters, destroyed.position.zMeters}, destroyed.radiusMeters, destroyed.kind,
                         start, seed);
    if (placed.has_value())
    {
      // The shards are the faces' color, which is the model's darkened (ADR-027), and a structure's grayer (ADR-040).
      const bool structure = destroyed.kind == EntityKind::Structure;
      const DirectX::XMFLOAT4 color = ModelColor(placed->set->color, placed->tint, entity.builtPermille);
      const DirectX::XMFLOAT4 faces =
        structure ? Shaded(TowardGray(color, STRUCTURE_GRAY_SHARE), STRUCTURE_FILL_SHADE) : Shaded(color, FILL_SHADE);
      m_explosions.Add(ModelShape(placed->set->name, *placed->model, placed->level), placed->World(), faces, start, seed,
                       structure ? STRUCTURE_SHARD_COPIES : 1);
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
  if (_entity.kind == EntityKind::Derelict)
  {
    const ModelSet* wrecks = m_catalog.SetForDerelicts();
    const std::string* model = wrecks != nullptr ? m_catalog.ModelForHull(_entity.hull) : nullptr;
    if (model == nullptr)
      return std::nullopt;
    return PlacedModel{.set = wrecks,
                       .model = model,
                       .pose = {.position = _entity.position,
                                .headingRadians = _entity.headingRadians,
                                .scale = 2.0f * _entity.radiusMeters / wrecks->Model(*model).lengthMeters}};
  }
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
  std::optional<Stance> stance;
  if (_entity.structure == StructureKind::MiningRig && radius < _entity.radiusMeters)
    stance = RigStance(set->name, model->model, _entity, scale);
  return PlacedModel{.set = set,
                     .model = &model->model,
                     .level = DrawnLevel(entry, _entity.level),
                     .pose = {.position = _entity.position, .headingRadians = _entity.headingRadians, .scale = scale},
                     .tint = model->tint,
                     .stance = stance};
}

std::array<DirectX::XMFLOAT3, 8> Outpost::GameClient::BoundsCorners(const EntityView& _entity) const
{
  std::array<DirectX::XMFLOAT3, 8> corners{};
  if (const std::optional<PlacedModel> placed = PlaceModel(_entity); placed.has_value())
  {
    const Neuron::MeshData& shape = ModelShape(placed->set->name, *placed->model, placed->level);
    const DirectX::XMFLOAT4X4 world = placed->World();
    const DirectX::XMMATRIX matrix = DirectX::XMLoadFloat4x4(&world);
    for (std::size_t i = 0; i < corners.size(); ++i)
    {
      const DirectX::XMVECTOR corner =
        DirectX::XMVectorSet((i & 1U) != 0 ? shape.boundsMax.x : shape.boundsMin.x, (i & 2U) != 0 ? shape.boundsMax.y : shape.boundsMin.y,
                             (i & 4U) != 0 ? shape.boundsMax.z : shape.boundsMin.z, 1.0f);
      DirectX::XMStoreFloat3(&corners[i], DirectX::XMVector3TransformCoord(corner, matrix));
    }
    return corners;
  }
  const float radius = _entity.radiusMeters;
  for (std::size_t i = 0; i < corners.size(); ++i)
  {
    corners[i] = {_entity.position.xMeters + ((i & 1U) != 0 ? radius : -radius), 0.0f,
                  _entity.position.zMeters + ((i & 2U) != 0 ? radius : -radius)};
  }
  return corners;
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

Outpost::Stance Outpost::GameClient::RigStance(std::string_view _set, std::string_view _model, const EntityView& _rig, float _scale) const
{
  // The rig stands at its asteroid's center (design §6), so its entity's radius is the rock's.
  const float rockRadius = _rig.radiusMeters;
  const Neuron::MeshData& rock = ModelShape(ASTEROID_SET, RockModel(rockRadius));
  // Without feet over the rock, the rig's lowest point stands on the rock's top.
  const LoadedModel& rig = LoadedModelOf(_set, _model);
  const Stance onTop{.liftMeters = (m_rockTops[RockIndex(rockRadius)] * rockRadius) - (rig.shape.boundsMin.y * _scale)};
  if (!rig.feet.has_value())
    return onTop;
  std::vector<DirectX::XMFLOAT3> footMeters;
  footMeters.reserve(rig.feet->size());
  for (const DirectX::XMFLOAT3& foot : *rig.feet)
    footMeters.push_back({foot.x * _scale, foot.y * _scale, foot.z * _scale});

  // The rock is turned by its asteroid's heading and the rig by its own, both as WorldMatrix turns them, so a point along
  // the rig's axes lies over the rock's mesh where the rig's turn less the rock's puts it.
  const auto asteroid = std::ranges::find_if(m_entities, [&_rig](const EntityView& _entity)
                                             { return _entity.kind == EntityKind::Asteroid && _entity.position == _rig.position; });
  const float rockHeading = asteroid != m_entities.end() ? asteroid->headingRadians : 0.0f;
  const DirectX::XMMATRIX toRock = DirectX::XMMatrixRotationY(rockHeading - _rig.headingRadians);
  const auto rockAt = [&rock, &toRock, rockRadius](float _xMeters, float _zMeters) -> std::optional<float>
  {
    const DirectX::XMVECTOR over =
      DirectX::XMVector3Transform(DirectX::XMVectorSet(_xMeters / rockRadius, 0.0f, _zMeters / rockRadius, 0.0f), toRock);
    const std::optional<float> surface = Neuron::SurfaceHeightAt(rock, DirectX::XMVectorGetX(over), DirectX::XMVectorGetZ(over));
    return surface.has_value() ? std::optional<float>(*surface * rockRadius) : std::nullopt;
  };
  return StandOnFeet(footMeters, rockAt).value_or(onTop);
}

std::optional<DirectX::XMFLOAT4> Outpost::GameClient::BeamColor(EntityId _shooter) const
{
  const EntityView* shooter = FindById(m_entities, _shooter);
  if (shooter == nullptr)
    return std::nullopt;
  const ModelSet* set = m_catalog.SetForPlayer(shooter->owner);
  if (set == nullptr)
    return std::nullopt;
  const auto toWhite = [](float _channel) { return std::lerp(_channel, 1.0f, BEAM_WHITE_SHARE); };
  return DirectX::XMFLOAT4{toWhite(set->color.x), toWhite(set->color.y), toWhite(set->color.z), 1.0f};
}

std::optional<Outpost::PlanePosition> Outpost::GameClient::MuzzleOf(EntityId _shooter, PlanePosition _target, std::uint8_t _gun) const
{
  const EntityView* shooter = FindById(m_entities, _shooter);
  if (shooter == nullptr)
    return std::nullopt;
  const std::optional<PlacedModel> placed = PlaceModel(*shooter);
  if (!placed.has_value())
    return std::nullopt;
  return NearestMuzzle(ModelHardpoints(placed->set->name, *placed->model, placed->level), placed->pose, _target, _gun);
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
    const EntityView* before = FindById(m_previousEntities, ship.id);
    if (before != nullptr && m_frameSeconds > 0.0f)
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
  // A Mining Rig snaps only to an asteroid the player has seen; anything else is blocked by all there is (ADR-046). A
  // Relay snaps to its sector's node, and waits at its Command Station's node cap, and a rig needs a sector the player holds
  // (ADR-056).
  const GhostPlacement ghost = PlaceGhost(*type, *m_cursorGround, *placing == StructureKind::MiningRig ? m_knownEntities : m_entities,
                                          newest.mapSizeMeters, newest.sectors, newest.player, newest.nodeCap);
  const DirectX::XMFLOAT3 at{ghost.position.xMeters, OVERLAY_LIFT_METERS, ghost.position.zMeters};
  m_pipeline.Draw(_commandList, *m_ring, WorldMatrix(at, 0.0f, ghost.radiusMeters), ghost.valid ? GHOST_VALID_COLOR : GHOST_INVALID_COLOR);
  // The structure itself, shown where it would stand.
  if (const ModelSet* set = m_catalog.SetForPlayer(newest.player))
  {
    if (const StructureModel* model = m_catalog.ModelForStructure(*placing))
    {
      const ModelEntry& entry = set->Model(model->model);
      const DirectX::XMFLOAT3 standing{ghost.position.xMeters, 0.0f, ghost.position.zMeters};
      const DirectX::XMFLOAT4X4 world = WorldMatrix(standing, 0.0f, 2.0f * type->radiusMeters / entry.lengthMeters);
      for (const ModelPiece& piece : ModelPieces(set->name, model->model))
        m_pipeline.Draw(_commandList, *piece.faces, PieceWorld(piece, world), ghost.valid ? GHOST_VALID_COLOR : GHOST_INVALID_COLOR);
    }
  }
}

std::size_t Outpost::GameClient::ModelIndex(std::string_view _set, std::string_view _model, int _level) const
{
  // Each model takes one place a level, as LoadClientAssets loaded them.
  std::size_t first = 0;
  for (const ModelSet& set : m_catalog.sets)
  {
    for (const ModelEntry& model : set.models)
    {
      if (set.name == _set && model.name == _model)
        return first + static_cast<std::size_t>(DrawnLevel(model, _level) - FIRST_MODEL_LEVEL);
      first += static_cast<std::size_t>(ModelLevels(model));
    }
  }
  throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
}

const std::vector<Neuron::MeshHardpoint>& Outpost::GameClient::ModelHardpoints(std::string_view _set, std::string_view _model,
                                                                               int _level) const
{
  return LoadedModelOf(_set, _model, _level).hardpoints;
}

const Neuron::MeshData& Outpost::GameClient::ModelShape(std::string_view _set, std::string_view _model, int _level) const
{
  return LoadedModelOf(_set, _model, _level).shape;
}

const std::vector<Outpost::GameClient::ModelPiece>& Outpost::GameClient::ModelPieces(std::string_view _set, std::string_view _model,
                                                                                     int _level) const
{
  return LoadedModelOf(_set, _model, _level).pieces;
}

DirectX::XMFLOAT4X4 Outpost::GameClient::PieceWorld(const ModelPiece& _piece, const DirectX::XMFLOAT4X4& _world) const noexcept
{
  if (!_piece.spin.has_value())
    return _world;
  // On the view's clock, as the effects are, so that a part turns with the match's time.
  return Neuron::PartWorld(*_piece.spin, m_view.ViewTick() / m_ticksPerSecond, _world);
}

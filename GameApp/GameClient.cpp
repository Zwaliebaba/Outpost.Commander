#include "pch.h"
#include "GameClient.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr const wchar_t* MODELS_FILE = L"Models.json";
constexpr const wchar_t* CAMERA_FILE = L"Camera.json";

// One light from above and behind the default view's top-left, and how much of an object's color the unlit side keeps.
// Presentation, not tuning: the design asks only that the scene reads clearly (design §11).
constexpr DirectX::XMFLOAT3 TOWARD_LIGHT{-0.4f, 0.8f, 0.45f};
constexpr float AMBIENT = 0.3f;

// The grid covers the 2,000 m map (design §4): a line every 100 m, and a brighter one every 500 m.
constexpr float GRID_HALF_EXTENT_METERS = 1000.0f;
constexpr float MINOR_GRID_SPACING_METERS = 100.0f;
constexpr float MINOR_GRID_LINE_WIDTH_METERS = 1.5f;
constexpr float MAJOR_GRID_SPACING_METERS = 500.0f;
constexpr float MAJOR_GRID_LINE_WIDTH_METERS = 4.0f;
constexpr int MINOR_LINES_PER_MAJOR = 5;
constexpr DirectX::XMFLOAT4 MINOR_GRID_COLOR{0.05f, 0.09f, 0.14f, 1.0f};
constexpr DirectX::XMFLOAT4 MAJOR_GRID_COLOR{0.10f, 0.18f, 0.28f, 1.0f};

// Asteroids are drawn with the one asteroid mesh, 2 m across, so its scale is a radius (ADR-011).
constexpr std::string_view ASTEROID_SET = "Asteroids";
constexpr std::string_view ASTEROID_MODEL = "Asteroid";
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

// Lines along x and along z on the ground, every _spacingMeters across the grid, as flat strips facing up. Every
// _skipEvery-th line is left out when it is not zero, where the major grid draws one instead: two strips in one place
// at one depth would flicker.
Neuron::MeshData BuildGrid(float _spacingMeters, float _lineWidthMeters, int _skipEvery)
{
  Neuron::MeshData grid;
  const auto addStrip = [&grid](float _x0, float _z0, float _x1, float _z1)
  {
    const auto first = static_cast<std::uint32_t>(grid.vertices.size());
    constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
    grid.vertices.push_back({{_x0, 0.0f, _z1}, UP});
    grid.vertices.push_back({{_x1, 0.0f, _z1}, UP});
    grid.vertices.push_back({{_x1, 0.0f, _z0}, UP});
    grid.vertices.push_back({{_x0, 0.0f, _z0}, UP});
    // Clockwise seen from above, so the strip faces up (ADR-011).
    for (const std::uint32_t corner : {0u, 1u, 2u, 0u, 2u, 3u})
      grid.indices.push_back(first + corner);
  };

  const float half = _lineWidthMeters / 2.0f;
  const auto lineCount = static_cast<int>(std::lround(2.0f * GRID_HALF_EXTENT_METERS / _spacingMeters));
  for (int line = 0; line <= lineCount; ++line)
  {
    if (_skipEvery != 0 && line % _skipEvery == 0)
      continue;
    const float at = -GRID_HALF_EXTENT_METERS + (static_cast<float>(line) * _spacingMeters);
    addStrip(at - half, -GRID_HALF_EXTENT_METERS, at + half, GRID_HALF_EXTENT_METERS);
    addStrip(-GRID_HALF_EXTENT_METERS, at - half, GRID_HALF_EXTENT_METERS, at + half);
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
} // namespace

Outpost::GameClient::GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond)
  : m_catalog(LoadDataFile(MODELS_FILE, LoadModelCatalog)),
    m_camera(LoadDataFile(CAMERA_FILE, LoadCameraSettings)),
    m_pipeline(_renderer),
    m_view(_ticksPerSecond)
{
  for (const ModelSet& set : m_catalog.sets)
  {
    for (const ModelEntry& model : set.models)
    {
      const std::wstring fileName = ModelFileName(set, model);
      const Neuron::ByteBuffer bytes = ReadAsset(fileName);
      const Neuron::MeshData data = BuildModelMesh(bytes, model, std::format("Assets\\{}", winrt::to_string(fileName)));
      m_modelMeshes.emplace(MeshKey(set.name, model.name), std::make_unique<Neuron::Mesh>(_renderer, data));
    }
  }
  m_minorGrid =
    std::make_unique<Neuron::Mesh>(_renderer, BuildGrid(MINOR_GRID_SPACING_METERS, MINOR_GRID_LINE_WIDTH_METERS, MINOR_LINES_PER_MAJOR));
  m_majorGrid = std::make_unique<Neuron::Mesh>(_renderer, BuildGrid(MAJOR_GRID_SPACING_METERS, MAJOR_GRID_LINE_WIDTH_METERS, 0));
  m_ring = std::make_unique<Neuron::Mesh>(_renderer, BuildRing());
  m_strip = std::make_unique<Neuron::Mesh>(_renderer, BuildStrip());
}

void Outpost::GameClient::Receive(std::vector<Snapshot> _snapshots)
{
  for (Snapshot& snapshot : _snapshots)
    m_view.Receive(std::move(snapshot));

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
  m_view.Advance(_elapsedSeconds);
  m_previousEntities = std::move(m_entities);
  m_entities = m_view.Entities();
  m_viewport = {.widthPixels = _viewportWidthPixels, .heightPixels = _viewportHeightPixels};
  m_camera.Update(_input, _elapsedSeconds, _viewportWidthPixels, _viewportHeightPixels);
  if (!m_view.IsEmpty())
    m_controls.Update(_input, m_entities, m_view.Newest().player, m_camera, m_viewport);
  WatchForResponse();
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
  m_pipeline.BeginDrawing(_commandList, _renderer.FrameIndex(), constants);

  const DirectX::XMFLOAT4X4 identity = WorldMatrix({}, 0.0f, 1.0f);
  m_pipeline.Draw(_commandList, *m_minorGrid, identity, MINOR_GRID_COLOR);
  m_pipeline.Draw(_commandList, *m_majorGrid, identity, MAJOR_GRID_COLOR);
  for (const EntityView& entity : m_entities)
    DrawEntity(_commandList, entity);
  DrawSelection(_commandList);
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
    DrawGroundLine(_commandList, ground[i], ground[(i + 1) % ground.size()], DRAG_BOX_COLOR);
}

void Outpost::GameClient::DrawGroundLine(ID3D12GraphicsCommandList* _commandList, PlanePosition _from, PlanePosition _to,
                                         const DirectX::XMFLOAT4& _color)
{
  const float dx = _to.xMeters - _from.xMeters;
  const float dz = _to.zMeters - _from.zMeters;
  const float length = std::hypot(dx, dz);
  if (length <= 0.0f)
    return;
  // The strip is stretched along x and widened along z, which keeps its upward normal, then turned onto the line.
  const DirectX::XMMATRIX world = DirectX::XMMatrixScaling(length, 1.0f, DRAG_LINE_WIDTH_METERS) *
                                  DirectX::XMMatrixRotationY(-std::atan2(dz, dx)) *
                                  DirectX::XMMatrixTranslation(_from.xMeters, OVERLAY_LIFT_METERS, _from.zMeters);
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
    // The data maps every player and hull the server can send (ModelCatalog); anything else is not drawn.
    const ModelSet* set = m_catalog.SetForPlayer(_entity.owner);
    const std::string* model = m_catalog.ModelForHull(_entity.hull);
    if (set != nullptr && model != nullptr)
      m_pipeline.Draw(_commandList, ModelMesh(set->name, *model), WorldMatrix(position, _entity.headingRadians, 1.0f), set->color);
    break;
  }
  case EntityKind::Asteroid:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    m_pipeline.Draw(_commandList, ModelMesh(ASTEROID_SET, ASTEROID_MODEL),
                    WorldMatrix(position, _entity.headingRadians, _entity.radiusMeters), set.color);
    break;
  }
  case EntityKind::AsteroidField:
  {
    const ModelSet& set = m_catalog.Set(ASTEROID_SET);
    const DirectX::XMFLOAT4 color{set.color.x * FIELD_SHADE, set.color.y * FIELD_SHADE, set.color.z * FIELD_SHADE, set.color.w};
    const Neuron::Mesh& rock = ModelMesh(ASTEROID_SET, ASTEROID_MODEL);
    const float radius = _entity.radiusMeters;
    m_pipeline.Draw(_commandList, rock, WorldMatrix(position, 0.0f, radius * FIELD_CENTER_ROCK_SHARE), color);
    for (int i = 0; i < FIELD_RING_ROCKS; ++i)
    {
      const float angle = FIELD_RING_START_RADIANS + (static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / FIELD_RING_ROCKS);
      const float distance = radius * FIELD_RING_DISTANCE_SHARE;
      const DirectX::XMFLOAT3 at{position.x + (distance * std::cos(angle)), 0.0f, position.z + (distance * std::sin(angle))};
      m_pipeline.Draw(_commandList, rock, WorldMatrix(at, angle * 2.0f, radius * FIELD_RING_ROCK_SHARE), color);
    }
    break;
  }
  case EntityKind::Structure:
  default:
    // Structures are drawn from task 4.2.
    break;
  }
}

const Neuron::Mesh& Outpost::GameClient::ModelMesh(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelMeshes.find(MeshKey(_set, _model));
  if (found == m_modelMeshes.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return *found->second;
}

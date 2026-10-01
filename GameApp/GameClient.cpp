#include "pch.h"
#include "GameClient.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr auto MODELS_FILE = L"Models.json";
constexpr auto CAMERA_FILE = L"Camera.json";
// The HUD's font: installed with Windows, so nothing ships (ADR-015).
constexpr std::wstring_view HUD_FONT = L"Segoe UI";

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

// A damaged ship's or structure's bar floats above it, as long as its footprint is wide and just off it toward -z: green
// above half its hit points, then amber, then red, over a dark full-length bar.
constexpr float HEALTH_BAR_HEIGHT_METERS = 10.0f;
constexpr float HEALTH_BAR_WIDTH_METERS = 2.5f;
constexpr float HEALTH_BAR_GAP_METERS = 4.0f;
constexpr DirectX::XMFLOAT4 HEALTH_BACK_COLOR{0.08f, 0.08f, 0.08f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_GOOD_COLOR{0.2f, 0.85f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_HURT_COLOR{1.0f, 0.7f, 0.1f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_LOW_COLOR{0.95f, 0.2f, 0.15f, 1.0f};
constexpr float HEALTH_HURT_SHARE = 0.5f;
constexpr float HEALTH_LOW_SHARE = 0.25f;
// A structure under construction: its color darkens toward this share at the start, and a blue bar shows the share built,
// just beyond the health bar.
constexpr float UNBUILT_SHADE = 0.35f;
constexpr DirectX::XMFLOAT4 BUILD_BAR_COLOR{0.25f, 0.65f, 1.0f, 1.0f};
// A Mining Rig sits on its asteroid's top: this share of the asteroid's radius above the ground.
constexpr float RIG_LIFT_SHARE = 0.7f;
// The ghost: a flat disc of the footprint, a little above the ground.
constexpr DirectX::XMFLOAT4 GHOST_VALID_COLOR{0.2f, 0.75f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 GHOST_INVALID_COLOR{0.85f, 0.2f, 0.15f, 1.0f};
constexpr int DISC_SEGMENTS = 24;

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
} // namespace

Outpost::GameClient::GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond)
  : m_catalog(LoadDataFile(MODELS_FILE, LoadModelCatalog)),
    m_camera(LoadDataFile(CAMERA_FILE, LoadCameraSettings)),
    m_pipeline(_renderer),
    m_ui(_renderer, HUD_FONT, Hud::FONT_UNITS * Hud::Scale(_renderer.WidthPixels(), _renderer.HeightPixels())),
    m_view(_ticksPerSecond),
    m_effects(_ticksPerSecond)
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
  m_minorGrid = std::make_unique<Neuron::Mesh>(_renderer, BuildGrid(MINOR_GRID_SPACING_METERS, MINOR_GRID_LINE_WIDTH_METERS,
                                                                    MINOR_LINES_PER_MAJOR));
  m_majorGrid = std::make_unique<Neuron::Mesh>(_renderer, BuildGrid(MAJOR_GRID_SPACING_METERS, MAJOR_GRID_LINE_WIDTH_METERS, 0));
  m_ring = std::make_unique<Neuron::Mesh>(_renderer, BuildRing());
  m_disc = std::make_unique<Neuron::Mesh>(_renderer, BuildDisc());
  m_strip = std::make_unique<Neuron::Mesh>(_renderer, BuildStrip());
}

void Outpost::GameClient::Receive(std::vector<Snapshot> _snapshots)
{
  for (Snapshot& snapshot : _snapshots)
  {
    m_effects.Receive(snapshot);
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
  m_view.Advance(_elapsedSeconds);
  m_previousEntities = std::move(m_entities);
  m_entities = m_view.Entities();
  m_effectDraws = m_effects.At(m_view.ViewTick());
  m_viewport = {.widthPixels = _viewportWidthPixels, .heightPixels = _viewportHeightPixels};
  m_camera.Update(_input, _elapsedSeconds, _viewportWidthPixels, _viewportHeightPixels);

  m_cursorGround = m_camera.GroundPointAtPixel(static_cast<float>(_input.cursorXPixels), static_cast<float>(_input.cursorYPixels),
                                               m_viewport);
  HandleHudInput(_input);

  // A press on the HUD is the HUD's, not an order or a selection in the world; releases still reach the controls, so
  // that a drag begun in the world ends wherever it is let go.
  Neuron::InputState input = _input;
  std::erase_if(input.events, [this](const Neuron::InputEvent& _event)
  {
    return _event.kind == Neuron::InputEventKind::ButtonDown && m_hudLayout.Covers(static_cast<float>(_event.xPixels),
                                                                                   static_cast<float>(_event.yPixels));
  });
  if (!m_view.IsEmpty())
  {
    m_controls.Update(input, m_entities, m_view.Newest().player, m_camera, m_viewport);
    const std::vector<PlanePosition> view = ViewOnGround();
    m_hudLayout = Hud::Lay(Hud::Describe(m_view.Newest(), m_entities, m_controls.Selected(), m_controls.Placing()), _viewportWidthPixels,
                           _viewportHeightPixels, view);
  }
  WatchForResponse();
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
      if (const std::optional<Hud::Action> action = m_hudLayout.ActionAt(x, y))
      {
        if (action->kind == Hud::ActionKind::Build)
          m_controls.ArmPlacement(action->structure, m_entities);
        else
          m_controls.Queue(action->producer, action->design);
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
    if (const std::optional<PlanePosition> point = m_hudLayout.MapPointAt(static_cast<float>(_input.cursorXPixels),
                                                                          static_cast<float>(_input.cursorYPixels)))
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
  m_pipeline.BeginDrawing(_commandList, _renderer.FrameIndex(), constants);

  const DirectX::XMFLOAT4X4 identity = WorldMatrix({}, 0.0f, 1.0f);
  m_pipeline.Draw(_commandList, *m_minorGrid, identity, MINOR_GRID_COLOR);
  m_pipeline.Draw(_commandList, *m_majorGrid, identity, MAJOR_GRID_COLOR);
  for (const EntityView& entity : m_entities)
    DrawEntity(_commandList, entity);
  DrawSelection(_commandList);
  DrawGhost(_commandList);
  DrawHealthBars(_commandList);
  DrawEffects(_commandList);
  DrawHud(_commandList, _renderer.FrameIndex());
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
    if (entity.builtPermille < PERMILLE)
    {
      const float left = entity.position.xMeters - entity.radiusMeters;
      const float z = entity.position.zMeters - entity.radiusMeters - (2.0f * HEALTH_BAR_GAP_METERS) - HEALTH_BAR_WIDTH_METERS;
      const float built = static_cast<float>(entity.builtPermille) / static_cast<float>(PERMILLE);
      DrawBand(_commandList, {.xMeters = left, .zMeters = z}, {.xMeters = left + (2.0f * entity.radiusMeters), .zMeters = z},
               HEALTH_BAR_WIDTH_METERS, HEALTH_BAR_HEIGHT_METERS, HEALTH_BACK_COLOR);
      DrawBand(_commandList, {.xMeters = left, .zMeters = z}, {.xMeters = left + (2.0f * entity.radiusMeters * built), .zMeters = z},
               HEALTH_BAR_WIDTH_METERS, HEALTH_BAR_HEIGHT_METERS + OVERLAY_LIFT_METERS, BUILD_BAR_COLOR);
    }
    if (entity.maxHitPointsHundredths <= 0 || entity.hitPointsHundredths >= entity.maxHitPointsHundredths)
      continue;
    const float share = std::clamp(static_cast<float>(entity.hitPointsHundredths) / static_cast<float>(entity.maxHitPointsHundredths), 0.0f,
                                   1.0f);
    const float left = entity.position.xMeters - entity.radiusMeters;
    const float z = entity.position.zMeters - entity.radiusMeters - HEALTH_BAR_GAP_METERS;
    const PlanePosition start{.xMeters = left, .zMeters = z};
    DrawBand(_commandList, start, {.xMeters = left + (2.0f * entity.radiusMeters), .zMeters = z}, HEALTH_BAR_WIDTH_METERS,
             HEALTH_BAR_HEIGHT_METERS, HEALTH_BACK_COLOR);
    const DirectX::XMFLOAT4& color = share > HEALTH_HURT_SHARE
                                       ? HEALTH_GOOD_COLOR
                                       : share > HEALTH_LOW_SHARE
                                       ? HEALTH_HURT_COLOR
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
    case CombatEffects::Shape::Band: default:
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
    const ModelSet* set = m_catalog.SetForPlayer(_entity.owner);
    const std::string* model = _entity.role == ShipRole::Constructor ? &m_catalog.constructor : m_catalog.ModelForHull(_entity.hull);
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
  case EntityKind::Structure: default:
    DrawStructure(_commandList, _entity);
    break;
  }
}

void Outpost::GameClient::DrawStructure(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity)
{
  const ModelSet* set = m_catalog.SetForPlayer(_entity.owner);
  const StructureModel* model = m_catalog.ModelForStructure(_entity.structure);
  if (set == nullptr || model == nullptr)
    return;
  // Drawn across its kind's footprint: a Mining Rig's entity covers its asteroid, but the rig is the size of its kind,
  // standing on top of the rock.
  const Snapshot& newest = m_view.Newest();
  const auto type = std::ranges::find(newest.structureTypes, _entity.structure, &StructureTypeView::structure);
  const float radius = type != newest.structureTypes.end() ? type->radiusMeters : _entity.radiusMeters;
  const float lift = _entity.structure == StructureKind::MiningRig && radius < _entity.radiusMeters
                       ? _entity.radiusMeters * RIG_LIFT_SHARE
                       : 0.0f;
  const ModelEntry& entry = set->Model(model->model);
  const float scale = 2.0f * radius / entry.lengthMeters;
  const float built = static_cast<float>(_entity.builtPermille) / static_cast<float>(PERMILLE);
  const float shade = model->tint * (UNBUILT_SHADE + ((1.0f - UNBUILT_SHADE) * built));
  const DirectX::XMFLOAT4 color{std::min(1.0f, set->color.x * shade), std::min(1.0f, set->color.y * shade),
                                std::min(1.0f, set->color.z * shade), set->color.w};
  const DirectX::XMFLOAT3 position{_entity.position.xMeters, lift, _entity.position.zMeters};
  m_pipeline.Draw(_commandList, ModelMesh(set->name, model->model), WorldMatrix(position, _entity.headingRadians, scale), color);
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

const Neuron::Mesh& Outpost::GameClient::ModelMesh(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelMeshes.find(MeshKey(_set, _model));
  if (found == m_modelMeshes.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return *found->second;
}
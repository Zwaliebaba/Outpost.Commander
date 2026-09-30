#include "pch.h"
#include "GameClient.h"

#include <cmath>

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

// Milestone 1's lineup: each set's hulls in a row, Human above the x axis on screen and Tarkan below it, all facing +x
// (task 1.3). The asteroid is drawn at the size of a home asteroid (OutpostCommander/Assets/Map.json).
constexpr std::array<const char*, 3> LINEUP_HULLS{"Small", "Medium", "Large"};
constexpr std::array<float, 3> LINEUP_X_METERS{-70.0f, -20.0f, 55.0f};
constexpr float HUMAN_ROW_Z_METERS = 60.0f;
constexpr float TARKAN_ROW_Z_METERS = -60.0f;
constexpr DirectX::XMFLOAT3 ASTEROID_POSITION{-200.0f, 0.0f, 150.0f};
constexpr float ASTEROID_RADIUS_METERS = 45.0f;

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

Outpost::GameClient::GameClient(Neuron::Renderer& _renderer)
  : m_catalog(LoadDataFile(MODELS_FILE, LoadModelCatalog)),
    m_camera(LoadDataFile(CAMERA_FILE, LoadCameraSettings)),
    m_pipeline(_renderer)
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
  PlaceLineup();
}

void Outpost::GameClient::Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
                                 std::uint32_t _viewportHeightPixels) noexcept
{
  m_camera.Update(_input, _elapsedSeconds, _viewportWidthPixels, _viewportHeightPixels);
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
  for (const Placement& placement : m_placements)
    m_pipeline.Draw(_commandList, *placement.mesh, placement.world, placement.color);
}

const Neuron::Mesh& Outpost::GameClient::ModelMesh(std::string_view _set, std::string_view _model) const
{
  const auto found = m_modelMeshes.find(MeshKey(_set, _model));
  if (found == m_modelMeshes.end())
    throw Neuron::Exception(std::format("The model {}/{} is not loaded.", _set, _model));
  return *found->second;
}

void Outpost::GameClient::PlaceLineup()
{
  const auto placeRow = [this](std::string_view _setName, float _zMeters)
  {
    const ModelSet& set = m_catalog.Set(_setName);
    for (size_t i = 0; i < LINEUP_HULLS.size(); ++i)
      m_placements.push_back({.mesh = &ModelMesh(set.name, LINEUP_HULLS[i]),
                              .world = WorldMatrix({LINEUP_X_METERS[i], 0.0f, _zMeters}, 0.0f, 1.0f),
                              .color = set.color});
  };
  placeRow("Human", HUMAN_ROW_Z_METERS);
  placeRow("Tarkan", TARKAN_ROW_Z_METERS);

  // The asteroid's mesh is 2 m across, so its scale is its radius.
  const ModelSet& asteroids = m_catalog.Set("Asteroids");
  m_placements.push_back({.mesh = &ModelMesh(asteroids.name, "Asteroid"),
                          .world = WorldMatrix(ASTEROID_POSITION, 0.0f, ASTEROID_RADIUS_METERS),
                          .color = asteroids.color});
}

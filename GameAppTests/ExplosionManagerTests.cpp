#include "pch.h"

#include <algorithm>
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Batch = Outpost::ExplosionManager::Batch;

constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr float TOLERANCE = 1e-3f;
constexpr std::uint64_t SEED = 7;
constexpr DirectX::XMFLOAT4 COLOR{0.8f, 0.4f, 0.2f, 1.0f};
// Vertices a shard is drawn with: a front and a back.
constexpr size_t VERTICES_PER_SHARD = 6;

double Ticks(double _seconds)
{
  return _seconds * TICKS_PER_SECOND;
}

DirectX::XMFLOAT4X4 Identity()
{
  DirectX::XMFLOAT4X4 identity;
  DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
  return identity;
}

void Bound(Neuron::MeshData& _mesh)
{
  _mesh.boundsMin = _mesh.vertices.front().position;
  _mesh.boundsMax = _mesh.vertices.front().position;
  for (const Neuron::MeshVertex& vertex : _mesh.vertices)
  {
    _mesh.boundsMin = {std::min(_mesh.boundsMin.x, vertex.position.x), std::min(_mesh.boundsMin.y, vertex.position.y),
                       std::min(_mesh.boundsMin.z, vertex.position.z)};
    _mesh.boundsMax = {std::max(_mesh.boundsMax.x, vertex.position.x), std::max(_mesh.boundsMax.y, vertex.position.y),
                       std::max(_mesh.boundsMax.z, vertex.position.z)};
  }
}

// A box _halfMeters each way round the origin: six faces of two triangles, 12 in all.
Neuron::MeshData Box(float _halfMeters)
{
  Neuron::MeshData box;
  for (int axis = 0; axis < 3; ++axis)
  {
    for (const float sign : {-1.0f, 1.0f})
    {
      const auto along = [&](int _axis, float _value)
      {
        std::array<float, 3> point{};
        point[static_cast<size_t>(_axis)] = _value;
        return point;
      };
      const std::array<float, 3> normal = along(axis, sign);
      const int u = (axis + 1) % 3;
      const int v = (axis + 2) % 3;
      const auto first = static_cast<std::uint32_t>(box.vertices.size());
      for (const auto& [du, dv] : std::array<std::pair<float, float>, 4>{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}})
      {
        std::array<float, 3> point = along(axis, sign * _halfMeters);
        point[static_cast<size_t>(u)] = du * _halfMeters;
        point[static_cast<size_t>(v)] = dv * _halfMeters;
        box.vertices.push_back({.position = {point[0], point[1], point[2]}, .normal = {normal[0], normal[1], normal[2]}});
      }
      box.indices.insert(box.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }
  }
  Bound(box);
  return box;
}

// A flat grid of _cells by _cells squares, 20 m across, facing up: 2 * _cells^2 triangles.
Neuron::MeshData Grid(int _cells)
{
  Neuron::MeshData grid;
  const float step = 20.0f / static_cast<float>(_cells);
  for (int row = 0; row <= _cells; ++row)
  {
    for (int column = 0; column <= _cells; ++column)
      grid.vertices.push_back({.position = {(static_cast<float>(column) * step) - 10.0f, 0.0f, (static_cast<float>(row) * step) - 10.0f},
                               .normal = {0.0f, 1.0f, 0.0f}});
  }
  const auto width = static_cast<std::uint32_t>(_cells + 1);
  for (std::uint32_t row = 0; row < static_cast<std::uint32_t>(_cells); ++row)
  {
    for (std::uint32_t column = 0; column < static_cast<std::uint32_t>(_cells); ++column)
    {
      const std::uint32_t corner = (row * width) + column;
      grid.indices.insert(grid.indices.end(), {corner, corner + width, corner + width + 1, corner, corner + width + 1, corner + 1});
    }
  }
  Bound(grid);
  return grid;
}

struct Frame
{
  std::vector<Neuron::MeshVertex> vertices;
  std::vector<Batch> batches;
};

Frame FrameAt(Outpost::ExplosionManager& _explosions, double _seconds)
{
  Frame frame;
  _explosions.At(Ticks(_seconds), frame.vertices, frame.batches);
  return frame;
}

// The center of the _shard-th shard's front face.
DirectX::XMFLOAT3 ShardCenter(const Frame& _frame, size_t _shard)
{
  const size_t first = _shard * VERTICES_PER_SHARD;
  DirectX::XMFLOAT3 sum{};
  for (size_t i = first; i < first + 3; ++i)
  {
    sum.x += _frame.vertices[i].position.x;
    sum.y += _frame.vertices[i].position.y;
    sum.z += _frame.vertices[i].position.z;
  }
  return {sum.x / 3.0f, sum.y / 3.0f, sum.z / 3.0f};
}

float Distance(const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b)
{
  return std::sqrt(((_a.x - _b.x) * (_a.x - _b.x)) + ((_a.y - _b.y) * (_a.y - _b.y)) + ((_a.z - _b.z) * (_a.z - _b.z)));
}

bool SamePosition(const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b)
{
  return Distance(_a, _b) <= TOLERANCE;
}
} // namespace

TEST_CLASS(ExplosionManagerTests)
{
public:
  // ADR-026: an explosion plays when the view reaches its start, as every one of its triangles, in its color.
  TEST_METHOD(WaitsForTheView)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Box(2.0f), Identity(), COLOR, Ticks(1.0), SEED);
    Assert::IsTrue(FrameAt(explosions, 0.9).batches.empty());
    const Frame frame = FrameAt(explosions, 1.0);
    Assert::AreEqual(size_t{1}, frame.batches.size());
    Assert::AreEqual(std::uint32_t{VERTICES_PER_SHARD * 12}, frame.batches.front().vertexCount);
    Assert::AreEqual(COLOR.x, frame.batches.front().color.x, TOLERANCE);
  }

  // The mesh pipeline culls back faces, so each shard is drawn once each way round, its normal turned with it.
  TEST_METHOD(EveryShardHasAFrontAndABack)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED);
    const Frame frame = FrameAt(explosions, 0.7);
    Assert::AreEqual(size_t{0}, frame.vertices.size() % VERTICES_PER_SHARD);
    for (size_t first = 0; first < frame.vertices.size(); first += VERTICES_PER_SHARD)
    {
      const auto at = [&frame, first](size_t _offset) -> const Neuron::MeshVertex& { return frame.vertices[first + _offset]; };
      Assert::IsTrue(SamePosition(at(0).position, at(3).position));
      Assert::IsTrue(SamePosition(at(1).position, at(5).position));
      Assert::IsTrue(SamePosition(at(2).position, at(4).position));
      Assert::AreEqual(-at(0).normal.x, at(3).normal.x, TOLERANCE);
      Assert::AreEqual(-at(0).normal.y, at(3).normal.y, TOLERANCE);
      Assert::AreEqual(-at(0).normal.z, at(3).normal.z, TOLERANCE);
    }
  }

  // Shards fly apart, and drag slows each one: it covers less in the second half second than in the first.
  TEST_METHOD(ShardsFlyApartAndSlow)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED);
    const Frame start = FrameAt(explosions, 0.0);
    const Frame half = FrameAt(explosions, 0.5);
    const Frame one = FrameAt(explosions, 1.0);
    const size_t shards = start.vertices.size() / VERTICES_PER_SHARD;
    float startSpread = 0.0f;
    float laterSpread = 0.0f;
    for (size_t shard = 0; shard < shards; ++shard)
    {
      const float firstHalf = Distance(ShardCenter(start, shard), ShardCenter(half, shard));
      const float secondHalf = Distance(ShardCenter(half, shard), ShardCenter(one, shard));
      Assert::IsTrue(firstHalf > 0.0f);
      Assert::IsTrue(secondHalf < firstHalf);
      startSpread += Distance(ShardCenter(start, shard), {});
      laterSpread += Distance(ShardCenter(one, shard), {});
    }
    Assert::IsTrue(laterSpread > startSpread, L"the shards are farther from the center");
  }

  // A shard darkens toward black over its life, shrinks to nothing over its last 40%, and is then forgotten.
  TEST_METHOD(DarkensAndShrinksThenEnds)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED);
    constexpr double LIFE = Outpost::ExplosionManager::LIFE_SECONDS;
    const Frame halfway = FrameAt(explosions, LIFE * 0.5);
    Assert::AreEqual(COLOR.x * 0.5f, halfway.batches.front().color.x, TOLERANCE);
    const Frame late = FrameAt(explosions, LIFE * 0.9);
    const auto edge = [](const Frame& _frame) { return Distance(_frame.vertices[0].position, _frame.vertices[1].position); };
    Assert::AreEqual(edge(halfway) * 0.25f, edge(late), TOLERANCE);

    Assert::IsTrue(FrameAt(explosions, LIFE + 0.1).batches.empty());
    Assert::AreEqual(size_t{0}, explosions.Pending());
  }

  // It breaks where the world matrix put the model.
  TEST_METHOD(BreaksWhereTheModelStood)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    DirectX::XMFLOAT4X4 world;
    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(100.0f, 0.0f, 50.0f));
    explosions.Add(Box(2.0f), world, COLOR, 0.0, SEED);
    const Frame frame = FrameAt(explosions, 0.0);
    DirectX::XMFLOAT3 sum{};
    for (const Neuron::MeshVertex& vertex : frame.vertices)
    {
      sum.x += vertex.position.x;
      sum.y += vertex.position.y;
      sum.z += vertex.position.z;
    }
    const auto count = static_cast<float>(frame.vertices.size());
    Assert::IsTrue(SamePosition({100.0f, 0.0f, 50.0f}, {sum.x / count, sum.y / count, sum.z / count}));
  }

  TEST_METHOD(TheSameSeedGivesTheSameExplosion)
  {
    Outpost::ExplosionManager first(TICKS_PER_SECOND);
    Outpost::ExplosionManager second(TICKS_PER_SECOND);
    Outpost::ExplosionManager other(TICKS_PER_SECOND);
    first.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED);
    second.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED);
    other.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED + 1);
    const auto same = [](const Frame& _a, const Frame& _b)
    {
      return std::ranges::equal(_a.vertices, _b.vertices, [](const Neuron::MeshVertex& _x, const Neuron::MeshVertex& _y)
                                { return SamePosition(_x.position, _y.position); });
    };
    const Frame firstFrame = FrameAt(first, 1.0);
    Assert::IsTrue(same(firstFrame, FrameAt(second, 1.0)));
    Assert::IsFalse(same(firstFrame, FrameAt(other, 1.0)));
  }

  // A mesh with more triangles than MAX_SHARDS gives a selection of them, and copies share the same limit.
  TEST_METHOD(KeepsAtMostMaxShards)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Grid(25), Identity(), COLOR, 0.0, SEED, 3);
    const Frame frame = FrameAt(explosions, 0.0);
    Assert::IsTrue(frame.vertices.size() <= Outpost::ExplosionManager::MAX_SHARDS * VERTICES_PER_SHARD);
    Assert::IsTrue(frame.vertices.size() > Outpost::ExplosionManager::MAX_SHARDS * VERTICES_PER_SHARD / 2);
  }

  // A building breaks three times in DeepSpaceOutpost: each copy is a whole set of shards.
  TEST_METHOD(CopiesBreakMoreShards)
  {
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(Box(2.0f), Identity(), COLOR, 0.0, SEED, 3);
    Assert::AreEqual(std::uint32_t{VERTICES_PER_SHARD * 3 * 12}, FrameAt(explosions, 0.0).batches.front().vertexCount);
  }

  // A triangle with no area, or too small to see go, makes no shard.
  TEST_METHOD(LeavesOutWhatCannotBeSeen)
  {
    Neuron::MeshData mesh;
    constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
    mesh.vertices = {{{-10.0f, 0.0f, -10.0f}, UP}, {{-10.0f, 0.0f, 10.0f}, UP}, {{10.0f, 0.0f, 0.0f}, UP},
                     {{0.0f, 0.0f, 0.0f}, UP},     {{0.001f, 0.0f, 0.0f}, UP},  {{0.0f, 0.0f, 0.001f}, UP},
                     {{1.0f, 0.0f, 1.0f}, UP},     {{2.0f, 0.0f, 2.0f}, UP},    {{3.0f, 0.0f, 3.0f}, UP}};
    mesh.indices = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    Bound(mesh);
    Outpost::ExplosionManager explosions(TICKS_PER_SECOND);
    explosions.Add(mesh, Identity(), COLOR, 0.0, SEED);
    Assert::AreEqual(std::uint32_t{VERTICES_PER_SHARD}, FrameAt(explosions, 0.0).batches.front().vertexCount);
  }
};
} // namespace GameAppTests

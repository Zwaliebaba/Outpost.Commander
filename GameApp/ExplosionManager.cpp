#include "pch.h"
#include "ExplosionManager.h"

#include <algorithm>
#include <cmath>

namespace
{
using Outpost::ExplosionManager;

// Placeholder looks, after DeepSpaceOutpost's explosion.cpp, and presentation rather than tuning (design §11). Its speeds
// were in its own units; here they are in shares of what blew up, so that a big hull's shards fly as far, for its size,
// as a small one's.

// The spins one copy of the shards shares out, each shard taking one at random.
constexpr size_t TUMBLERS_PER_COPY = 5;
// A spin is up to this fast about each axis to start with, and slows as e^(-SPIN_FRICTION * age).
constexpr float MAX_SPIN_RADIANS_PER_SECOND = 4.0f;
constexpr float SPIN_FRICTION = 0.2f;
// A shard starts out from the center at this many times its distance from it a second. DeepSpaceOutpost's 3 was too
// slow for a ship to read as blowing apart (owner, 2026-10-02).
constexpr float OUTWARD_SPEED_PER_SECOND = 5.0f;
// And at up to this share of the explosion's radius a second more in any direction, which stands in for
// DeepSpaceOutpost's upward throw, so that the shards of a flat hull do not all stay in its plane.
constexpr float SCATTER_RADII_PER_SECOND = 0.5f;
// Drag slows a shard by the square of its speed, as DeepSpaceOutpost's friction does, at this over the explosion's
// radius per meter: larger stops the shards sooner. At 1.5, a shard from the rim flies about 2.1 radii in its 3 s life.
constexpr float DRAG_RADII = 1.5f;
// A triangle whose edges add up to less than this share of the explosion's radius is too small to see go.
constexpr float MIN_PERIMETER_RADII = 0.05f;
// A shard darkens to black over its whole life and shrinks to nothing over this last share of it.
constexpr double SHRINK_SHARE = 0.4;

struct Triangle
{
  std::array<DirectX::XMFLOAT3, 3> corners{};
  DirectX::XMFLOAT3 normal{};
  DirectX::XMFLOAT3 center{};
};

DirectX::XMFLOAT3 Stored(DirectX::FXMVECTOR _vector) noexcept
{
  DirectX::XMFLOAT3 result;
  DirectX::XMStoreFloat3(&result, _vector);
  return result;
}

float Perimeter(const Triangle& _triangle) noexcept
{
  float sum = 0.0f;
  for (size_t i = 0; i < 3; ++i)
  {
    const DirectX::XMVECTOR edge =
      DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&_triangle.corners[(i + 1) % 3]), DirectX::XMLoadFloat3(&_triangle.corners[i]));
    sum += DirectX::XMVectorGetX(DirectX::XMVector3Length(edge));
  }
  return sum;
}

// _mesh's triangles placed by _world, each with its corners, its face normal and its center. A triangle with no area,
// or whose vertex normals cancel out, is left out.
std::vector<Triangle> PlacedTriangles(const Neuron::MeshData& _mesh, DirectX::FXMMATRIX _world)
{
  std::vector<Triangle> triangles;
  triangles.reserve(_mesh.indices.size() / 3);
  for (size_t i = 0; i + 2 < _mesh.indices.size(); i += 3)
  {
    Triangle triangle;
    DirectX::XMVECTOR normalSum = DirectX::XMVectorZero();
    DirectX::XMVECTOR centerSum = DirectX::XMVectorZero();
    for (size_t corner = 0; corner < 3; ++corner)
    {
      const Neuron::MeshVertex& vertex = _mesh.vertices[_mesh.indices[i + corner]];
      const DirectX::XMVECTOR placed = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex.position), _world);
      triangle.corners[corner] = Stored(placed);
      centerSum = DirectX::XMVectorAdd(centerSum, placed);
      normalSum = DirectX::XMVectorAdd(normalSum, DirectX::XMLoadFloat3(&vertex.normal));
    }
    // The world matrix scales uniformly, so its upper 3x3 turns a normal correctly once it is normalized (ADR-011).
    const DirectX::XMVECTOR normal = DirectX::XMVector3TransformNormal(normalSum, _world);
    const DirectX::XMVECTOR side1 =
      DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&triangle.corners[1]), DirectX::XMLoadFloat3(&triangle.corners[0]));
    const DirectX::XMVECTOR side2 =
      DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&triangle.corners[2]), DirectX::XMLoadFloat3(&triangle.corners[0]));
    const float twiceArea = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVector3Cross(side1, side2)));
    if (twiceArea <= 1e-6f || DirectX::XMVectorGetX(DirectX::XMVector3Length(normal)) <= 1e-6f)
      continue;
    triangle.normal = Stored(DirectX::XMVector3Normalize(normal));
    triangle.center = Stored(DirectX::XMVectorScale(centerSum, 1.0f / 3.0f));
    triangles.push_back(triangle);
  }
  return triangles;
}

DirectX::XMFLOAT4 Darkened(const DirectX::XMFLOAT4& _color, float _keep) noexcept
{
  return {_color.x * _keep, _color.y * _keep, _color.z * _keep, _color.w};
}
} // namespace

ExplosionManager::ExplosionManager(std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(static_cast<double>(_ticksPerSecond))
{
}

void ExplosionManager::Add(const Neuron::MeshData& _mesh, const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4& _color,
                           double _startTick, std::uint64_t _seed, int _copies)
{
  const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&_world);
  const DirectX::XMFLOAT3 boundsCenter{(_mesh.boundsMin.x + _mesh.boundsMax.x) * 0.5f, (_mesh.boundsMin.y + _mesh.boundsMax.y) * 0.5f,
                                       (_mesh.boundsMin.z + _mesh.boundsMax.z) * 0.5f};
  const DirectX::XMVECTOR center = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&boundsCenter), world);

  std::vector<Triangle> triangles = PlacedTriangles(_mesh, world);
  float radius = 0.0f;
  for (const Triangle& triangle : triangles)
  {
    for (const DirectX::XMFLOAT3& corner : triangle.corners)
    {
      const DirectX::XMVECTOR offset = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&corner), center);
      radius = std::max(radius, DirectX::XMVectorGetX(DirectX::XMVector3Length(offset)));
    }
  }
  if (radius <= 0.0f)
    return;
  std::erase_if(triangles, [radius](const Triangle& _triangle) { return Perimeter(_triangle) < MIN_PERIMETER_RADII * radius; });
  if (triangles.empty())
    return;

  const size_t copies = static_cast<size_t>(std::max(1, _copies));
  const float keepShare = std::min(1.0f, static_cast<float>(MAX_SHARDS) / static_cast<float>(triangles.size() * copies));
  EffectRandom random(_seed);
  Explosion explosion{.startTick = _startTick, .color = _color, .dragPerMeter = DRAG_RADII / radius};
  for (size_t copy = 0; copy < copies; ++copy)
  {
    const size_t firstTumbler = explosion.tumblers.size();
    for (size_t i = 0; i < TUMBLERS_PER_COPY; ++i)
    {
      // Each axis's draw is its own statement, so they are drawn in a fixed order.
      const float x = random.Signed(MAX_SPIN_RADIANS_PER_SECOND);
      const float y = random.Signed(MAX_SPIN_RADIANS_PER_SECOND);
      const float z = random.Signed(MAX_SPIN_RADIANS_PER_SECOND);
      const DirectX::XMVECTOR spin = DirectX::XMVectorSet(x, y, z, 0.0f);
      const float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(spin));
      explosion.tumblers.push_back(speed > 0.0f
                                     ? Tumbler{.axis = Stored(DirectX::XMVectorScale(spin, 1.0f / speed)), .radiansPerSecond = speed}
                                     : Tumbler{.axis = {0.0f, 1.0f, 0.0f}, .radiansPerSecond = 0.0f});
    }

    for (const Triangle& triangle : triangles)
    {
      if (explosion.shards.size() >= MAX_SHARDS)
        break;
      if (keepShare < 1.0f && random.Unit() >= keepShare)
        continue;
      const DirectX::XMVECTOR shardCenter = DirectX::XMLoadFloat3(&triangle.center);
      Shard shard{.center = triangle.center, .normal = triangle.normal};
      for (size_t corner = 0; corner < 3; ++corner)
        shard.corners[corner] = Stored(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&triangle.corners[corner]), shardCenter));

      const float scatterX = random.Signed(SCATTER_RADII_PER_SECOND * radius);
      const float scatterY = random.Signed(SCATTER_RADII_PER_SECOND * radius);
      const float scatterZ = random.Signed(SCATTER_RADII_PER_SECOND * radius);
      const DirectX::XMVECTOR velocity =
        DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMVectorSubtract(shardCenter, center), OUTWARD_SPEED_PER_SECOND),
                             DirectX::XMVectorSet(scatterX, scatterY, scatterZ, 0.0f));
      const float speed = DirectX::XMVectorGetX(DirectX::XMVector3Length(velocity));
      shard.speedMetersPerSecond = speed;
      shard.direction = speed > 0.0f ? Stored(DirectX::XMVectorScale(velocity, 1.0f / speed)) : DirectX::XMFLOAT3{0.0f, 1.0f, 0.0f};
      shard.tumbler = static_cast<std::uint32_t>(firstTumbler + (random.Next() % TUMBLERS_PER_COPY));
      explosion.shards.push_back(shard);
    }
  }
  if (!explosion.shards.empty())
    m_explosions.push_back(std::move(explosion));
}

void ExplosionManager::At(double _viewTick, std::vector<Neuron::MeshVertex>& _vertices, std::vector<Batch>& _batches)
{
  std::erase_if(m_explosions, [&](const Explosion& _explosion) { return Seconds(_viewTick - _explosion.startTick) > LIFE_SECONDS; });

  std::vector<DirectX::XMFLOAT4> spins;
  for (auto explosion = m_explosions.rbegin(); explosion != m_explosions.rend(); ++explosion)
  {
    const double age = Seconds(_viewTick - explosion->startTick);
    if (age < 0.0)
      continue;
    const double lifeShare = age / LIFE_SECONDS;
    const auto keep = static_cast<float>(1.0 - lifeShare);
    const auto shrink = static_cast<float>(std::min(1.0, (1.0 - lifeShare) / SHRINK_SHARE));

    // Each tumbler's turn so far, which slows as e^(-SPIN_FRICTION * age).
    const auto turned = static_cast<float>((1.0 - std::exp(-SPIN_FRICTION * age)) / SPIN_FRICTION);
    spins.clear();
    for (const Tumbler& tumbler : explosion->tumblers)
    {
      DirectX::XMFLOAT4 spin;
      DirectX::XMStoreFloat4(&spin,
                             DirectX::XMQuaternionRotationNormal(DirectX::XMLoadFloat3(&tumbler.axis), tumbler.radiansPerSecond * turned));
      spins.push_back(spin);
    }

    const auto firstVertex = static_cast<std::uint32_t>(_vertices.size());
    for (const Shard& shard : explosion->shards)
    {
      // Drag in the square of the speed: the speed falls as 1 / (1 + k s t), so the distance is ln(1 + k s t) / k.
      const float traveled =
        std::log1p(explosion->dragPerMeter * shard.speedMetersPerSecond * static_cast<float>(age)) / explosion->dragPerMeter;
      const DirectX::XMVECTOR position = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&shard.center),
                                                              DirectX::XMVectorScale(DirectX::XMLoadFloat3(&shard.direction), traveled));
      const DirectX::XMVECTOR spin = DirectX::XMLoadFloat4(&spins[shard.tumbler]);
      const DirectX::XMFLOAT3 front = Stored(DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&shard.normal), spin));
      const DirectX::XMFLOAT3 back{-front.x, -front.y, -front.z};
      std::array<DirectX::XMFLOAT3, 3> corners{};
      for (size_t corner = 0; corner < 3; ++corner)
      {
        const DirectX::XMVECTOR offset = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&shard.corners[corner]), shrink);
        corners[corner] = Stored(DirectX::XMVectorAdd(position, DirectX::XMVector3Rotate(offset, spin)));
      }
      // The front in the mesh's own winding, and the back the other way round.
      _vertices.push_back({.position = corners[0], .normal = front});
      _vertices.push_back({.position = corners[1], .normal = front});
      _vertices.push_back({.position = corners[2], .normal = front});
      _vertices.push_back({.position = corners[0], .normal = back});
      _vertices.push_back({.position = corners[2], .normal = back});
      _vertices.push_back({.position = corners[1], .normal = back});
    }
    _batches.push_back({.firstVertex = firstVertex,
                        .vertexCount = static_cast<std::uint32_t>(_vertices.size()) - firstVertex,
                        .color = Darkened(explosion->color, keep)});
  }
}

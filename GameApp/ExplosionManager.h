#pragma once

namespace Outpost
{
// Explosions after DeepSpaceOutpost's ExplosionManager (ADR-023): what blew up breaks into its own triangles, which fly
// apart from its center and tumble, slowed by drag, darken to black and shrink away. Where a shard is follows from its
// age in a closed form, so what is drawn depends only on the view's tick, as task 3.5's effects do, never on the frame
// rate. Space has no down, so DeepSpaceOutpost's gravity and upward throw are left out. It keeps no GPU state: it says
// which triangles to draw in which color, and GameClient draws them with the mesh pipeline.
class ExplosionManager
{
public:
  // One explosion's shards in a frame: a run of the frame's vertices, drawn in one color.
  struct Batch
  {
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
    // Linear. The mesh pipeline is opaque, so a shard fades by darkening toward black (ADR-011).
    DirectX::XMFLOAT4 color{};
  };

  // The shards one explosion keeps at most, all copies together. A mesh with more triangles gives a random selection of
  // them, as DeepSpaceOutpost's fraction does.
  static constexpr size_t MAX_SHARDS = 400;
  // How long an explosion lasts, in seconds: shorter than DeepSpaceOutpost's 5, so a ship is gone quickly (owner,
  // 2026-10-02).
  static constexpr double LIFE_SECONDS = 3.0;

  explicit ExplosionManager(std::uint32_t _ticksPerSecond);

  // Breaks _mesh, placed in the world by _world, into shards that fly apart from _startTick, in _color. _copies sets of
  // shards break from it at once, each with its own spin: DeepSpaceOutpost breaks a building three times. _seed decides
  // which triangles are kept and how they spin, so the same seed gives the same explosion. A mesh with nothing to break
  // adds nothing.
  void Add(const Neuron::MeshData& _mesh, const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4& _color, double _startTick,
           std::uint64_t _seed, int _copies = 1);

  // The shards at _viewTick: triangles in the world, appended to _vertices, and a batch for each explosion, appended to
  // _batches, newest first so that a frame that cannot take them all drops the oldest. Each shard is drawn twice, once
  // for each face, because the mesh pipeline culls back faces and a tumbling shard shows both. Explosions that have
  // played out are forgotten.
  void At(double _viewTick, std::vector<Neuron::MeshVertex>& _vertices, std::vector<Batch>& _batches);

  // Explosions waiting or playing; for tests.
  [[nodiscard]] size_t Pending() const noexcept
  {
    return m_explosions.size();
  }

private:
  // A spin some of an explosion's shards share, as DeepSpaceOutpost's Tumbler: about a fixed axis, slowing with time.
  struct Tumbler
  {
    DirectX::XMFLOAT3 axis{};
    float radiansPerSecond = 0.0f;
  };

  struct Shard
  {
    // Where the shard starts, in the world, and its corners and face normal about that point.
    DirectX::XMFLOAT3 center{};
    std::array<DirectX::XMFLOAT3, 3> corners{};
    DirectX::XMFLOAT3 normal{};
    // A unit vector, and the speed it starts at along it.
    DirectX::XMFLOAT3 direction{};
    float speedMetersPerSecond = 0.0f;
    std::uint32_t tumbler = 0;
  };

  struct Explosion
  {
    double startTick = 0.0;
    DirectX::XMFLOAT4 color{};
    // How hard drag slows a shard: its speed falls as 1 / (1 + drag * initial speed * age).
    float dragPerMeter = 0.0f;
    std::vector<Tumbler> tumblers;
    std::vector<Shard> shards;
  };

  [[nodiscard]] double Seconds(double _ticks) const noexcept
  {
    return _ticks / m_ticksPerSecond;
  }

  double m_ticksPerSecond = 0.0;
  std::vector<Explosion> m_explosions;
};
} // namespace Outpost

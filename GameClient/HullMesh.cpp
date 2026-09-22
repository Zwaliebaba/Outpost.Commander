#include "pch.h"

#include "HullMesh.h"

#include <array>

namespace Outpost
{

namespace
{
/// The three the client uploads at M1.9. The other ten in the catalog are M2's modules and
/// asteroids; listing them here would allocate buffers for geometry nothing draws.
constexpr std::array<std::string_view, 3> SHIPPED_AT_M1{"Scout", "Frigate", "Station"};

/// **A CHANNEL, AS A FRACTION.** The file stores the colour as a 32-bit DWORD the converter wrote
/// little-endian, so the low byte is red: `0x00BBGGRR` with alpha on top.
[[nodiscard]] constexpr float Channel(std::uint32_t _color, unsigned _shift) noexcept
{
  return static_cast<float>((_color >> _shift) & 0xFFu) / 255.0f;
}
} // namespace

HullVertex ToWorldVertex(const Neuron::CmoVertex& _vertex) noexcept
{
  HullVertex out;

  // world x = authored z, world y = -authored x, world z = authored y. See the header for why the
  // middle one is negated and what it costs in winding.
  out.x = _vertex.positionZ;
  out.y = -_vertex.positionX;
  out.z = _vertex.positionY;

  out.normalX = _vertex.normalZ;
  out.normalY = -_vertex.normalX;
  out.normalZ = _vertex.normalY;

  out.teamBlend = Channel(_vertex.color, 0);
  out.hullTone = Channel(_vertex.color, 8);
  return out;
}

std::string_view MeshNameForHull(HullId _hull) noexcept
{
  switch (_hull)
  {
  case HullId::Scout:
    return "Scout";
  case HullId::Frigate:
    return "Frigate";
  case HullId::Station:
    return "Station";
  case HullId::ModuleFrame:
    return "ModuleFrame";
  case HullId::Cruiser:
    // NOTHING AUTHORED ONE, because nothing builds one. The catalog states its size at 150 and M4
    // authors a mesh to that number rather than the other way round.
    return {};
  }
  return {};
}

std::string_view MeshNameForDesign(DesignId _design) noexcept
{
  if (static_cast<std::size_t>(_design) >= Designs().size())
  {
    return {};
  }
  return MeshNameForHull(Design(_design).hull);
}

std::span<const std::string_view> MeshesShippedAtM1() noexcept
{
  return SHIPPED_AT_M1;
}

bool LoadHullMesh(const Neuron::CmoMesh& _read, float _longestUnits, HullMesh& _outMesh)
{
  if (_read.vertices.empty() || _read.indices.empty())
  {
    return false;
  }
  if ((_read.indices.size() % 3) != 0)
  {
    // A triangle list that is not whole. Drawing two thirds of a triangle is worse than drawing
    // nothing, and the reader has already checked that every index names a vertex.
    return false;
  }

  HullMesh lifted;
  lifted.longestUnits = _longestUnits;

  lifted.vertices.reserve(_read.vertices.size());
  for (const Neuron::CmoVertex& vertex : _read.vertices)
  {
    lifted.vertices.push_back(ToWorldVertex(vertex));
  }

  // **REVERSED, BECAUSE THE CONVERSION ABOVE IS A REFLECTION.** Left-handed to right-handed flips
  // the sense of every triangle, so a mesh copied straight through would face inward and vanish
  // under back-face culling -- which looks like the mesh failing to load rather than like a
  // handedness bug, and is why this is one line with a paragraph on it.
  lifted.indices.reserve(_read.indices.size());
  for (std::size_t triangle = 0; triangle < _read.indices.size(); triangle += 3)
  {
    lifted.indices.push_back(_read.indices[triangle + 2]);
    lifted.indices.push_back(_read.indices[triangle + 1]);
    lifted.indices.push_back(_read.indices[triangle + 0]);
  }

  _outMesh = std::move(lifted);
  return true;
}

void HullToneColor(float _tone, float& _outRed, float& _outGreen, float& _outBlue) noexcept
{
  const float clamped = (_tone < 0.0f) ? 0.0f : ((_tone > 1.0f) ? 1.0f : _tone);

  // Two segments: DEEP to BASE over the first half, BASE to EDGE over the second. The content
  // quantizes the channel to 0, 128 and 255, so nothing lands between the stops today -- and the
  // ramp is written anyway, because the shader does this and a CPU statement that did something
  // else would be two answers to one question.
  const std::size_t low = (clamped < 0.5f) ? 0u : 1u;
  const float within = (clamped < 0.5f) ? (clamped * 2.0f) : ((clamped - 0.5f) * 2.0f);

  _outRed = HULL_PALETTE[low][0] + ((HULL_PALETTE[low + 1][0] - HULL_PALETTE[low][0]) * within);
  _outGreen = HULL_PALETTE[low][1] + ((HULL_PALETTE[low + 1][1] - HULL_PALETTE[low][1]) * within);
  _outBlue = HULL_PALETTE[low][2] + ((HULL_PALETTE[low + 1][2] - HULL_PALETTE[low][2]) * within);
}

void TeamColor(PlayerId _player, float& _outRed, float& _outGreen, float& _outBlue) noexcept
{
  if ((_player == NO_PLAYER) || (static_cast<std::size_t>(_player) > MAX_PLAYERS))
  {
    // Nobody's, which takes the hull's own base tone rather than reading off the end of the table.
    _outRed = HULL_PALETTE[1][0];
    _outGreen = HULL_PALETTE[1][1];
    _outBlue = HULL_PALETTE[1][2];
    return;
  }

  const std::size_t index = static_cast<std::size_t>(_player) - 1;
  _outRed = TEAM_PALETTE[index][0];
  _outGreen = TEAM_PALETTE[index][1];
  _outBlue = TEAM_PALETTE[index][2];
}

} // namespace Outpost

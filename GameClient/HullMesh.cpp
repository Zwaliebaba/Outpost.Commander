#include "pch.h"

#include "HullMesh.h"

#include <array>
#include <cmath>

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

void ToWorldDirection(float _authoredX, float _authoredY, float _authoredZ, float& _outX, float& _outY, float& _outZ) noexcept
{
  // world x = authored z, world y = -authored x, world z = authored y. See the header for why the
  // middle one is negated and what it costs in winding.
  _outX = _authoredZ;
  _outY = -_authoredX;
  _outZ = _authoredY;
}

HullVertex ToWorldVertex(const Neuron::CmoVertex& _vertex) noexcept
{
  HullVertex out;

  ToWorldDirection(_vertex.positionX, _vertex.positionY, _vertex.positionZ, out.x, out.y, out.z);
  ToWorldDirection(_vertex.normalX, _vertex.normalY, _vertex.normalZ, out.normalX, out.normalY, out.normalZ);

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

Neuron::MeshPass::Look ShipLook() noexcept
{
  Neuron::MeshPass::Look look;

  for (std::size_t channel = 0; channel < 3; ++channel)
  {
    look.hullDeep[channel] = HULL_PALETTE[0][channel];
    look.hullBase[channel] = HULL_PALETTE[1][channel];
    look.hullEdge[channel] = HULL_PALETTE[2][channel];
    look.ambient[channel] = AMBIENT_COLOR[channel];
  }

  // **CONVERTED WITH THE GEOMETRY, WHICH IT WAS NOT WHEN M1.9 FIRST DREW A HULL.** The rig is
  // authored Y-up like everything else the handoff states; passed through unconverted, the key
  // points very nearly along the plane instead of down onto it, and a station's top plate -- which
  // is most of what a raking camera shows -- is carried by ambient alone. It read as the hulls
  // being vague rather than as the light being wrong, which is why it is worth this paragraph.
  ToWorldDirection(KEY_LIGHT_DIRECTION[0], KEY_LIGHT_DIRECTION[1], KEY_LIGHT_DIRECTION[2], look.keyLight[0], look.keyLight[1],
                   look.keyLight[2]);
  ToWorldDirection(FILL_LIGHT_DIRECTION[0], FILL_LIGHT_DIRECTION[1], FILL_LIGHT_DIRECTION[2], look.fillLight[0], look.fillLight[1],
                   look.fillLight[2]);

  // The `w` of each light is its intensity, which is how six `float4` carry nine values without a
  // seventh register. The three palette stops leave theirs at zero and nothing reads them.
  look.keyLight[3] = KEY_LIGHT_INTENSITY;
  look.fillLight[3] = FILL_LIGHT_INTENSITY;
  look.ambient[3] = AMBIENT_INTENSITY;
  return look;
}

Neuron::MeshInstance InstanceFor(float _worldX, float _worldY, Neuron::Angle _heading, PlayerId _owner) noexcept
{
  // The binary angle to radians. 65,536 to a turn (ADR-002), and this is the renderer rather than
  // the simulation -- R16 does not reach here, which is what lets it be a float at all.
  constexpr float FULL_TURN_RADIANS = 6.28318530717958647692f;
  const float radians = (static_cast<float>(_heading) / 65536.0f) * FULL_TURN_RADIANS;

  Neuron::MeshInstance instance;
  instance.positionX = _worldX;
  instance.positionY = _worldY;
  instance.headingCosine = std::cos(radians);
  instance.headingSine = std::sin(radians);
  TeamColor(_owner, instance.teamRed, instance.teamGreen, instance.teamBlue);
  return instance;
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

#include "pch.h"

#include "Wrecks.h"

#include "HullMesh.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Outpost
{

namespace
{
/// How bright a wreck starts, against a live hull's one, and it darkens linearly to nothing.
inline constexpr float WRECK_START_BRIGHTNESS = 0.35f;

/// The gray a wreck's team faces take.
inline constexpr float WRECK_GRAY = 0.25f;

/// A slow tumble: a sixteenth of a turn a second, in binary angle units (65,536 a turn).
inline constexpr float TUMBLE_ANGLE_PER_SECOND = 4096.0f;

inline constexpr int BLAST_RAYS = 8;

/// A blast's rays reach this many times the hull's size across at the end, from half of it at the start.
inline constexpr float BLAST_REACH_OF_SIZE = 0.9f;

[[nodiscard]] float SizeUnitsOf(const EntityRecord& _record) noexcept
{
  if (_record.designIdentity >= Designs().size())
  {
    return 60.0f;
  }
  return static_cast<float>(Hull(Design(static_cast<DesignId>(_record.designIdentity)).hull).sizeUnits);
}

[[nodiscard]] float WorldX(const EntityRecord& _record) noexcept
{
  return static_cast<float>(DequantizePosition(_record.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
}

[[nodiscard]] float WorldY(const EntityRecord& _record) noexcept
{
  return static_cast<float>(DequantizePosition(_record.positionY)) / static_cast<float>(Neuron::FIXED_ONE);
}
} // namespace

WreckSet::WreckSet()
{
  m_wrecks.reserve(MAX_WRECKS);
}

void WreckSet::Spawn(std::span<const EntityRecord> _removed, std::uint64_t _nowMilliseconds)
{
  for (const EntityRecord& record : _removed)
  {
    if (m_wrecks.size() >= MAX_WRECKS)
    {
      m_wrecks.erase(m_wrecks.begin());
    }
    m_wrecks.push_back(Wreck{.last = record, .diedMilliseconds = _nowMilliseconds});
  }
}

void WreckSet::Expire(std::uint64_t _nowMilliseconds) noexcept
{
  std::erase_if(m_wrecks, [&](const Wreck& _wreck) { return _nowMilliseconds >= (_wreck.diedMilliseconds + WRECK_LIFETIME_MILLISECONDS); });
}

void WreckSet::Clear() noexcept
{
  m_wrecks.clear();
}

Neuron::MeshInstance WreckInstance(const Wreck& _wreck, std::uint64_t _nowMilliseconds) noexcept
{
  const std::uint64_t ageMilliseconds = (_nowMilliseconds > _wreck.diedMilliseconds) ? (_nowMilliseconds - _wreck.diedMilliseconds) : 0;
  const float life = std::min(1.0f, static_cast<float>(ageMilliseconds) / static_cast<float>(WRECK_LIFETIME_MILLISECONDS));

  // The tumble's direction from the identity, so neighbors do not all turn together. Presentation only.
  const float direction = ((_wreck.last.identity & 1u) != 0) ? 1.0f : -1.0f;
  const float drift = direction * TUMBLE_ANGLE_PER_SECOND * (static_cast<float>(ageMilliseconds) / 1000.0f);
  const float heading = static_cast<float>(DequantizeWireHeading(_wreck.last.heading)) + drift;
  const auto wrapped = static_cast<Neuron::Angle>(static_cast<std::int64_t>(std::fmod(heading, 65536.0f) + 65536.0f) % 65536);

  Neuron::MeshInstance instance = InstanceFor(WorldX(_wreck.last), WorldY(_wreck.last), wrapped, NO_PLAYER);
  instance.teamRed = WRECK_GRAY;
  instance.teamGreen = WRECK_GRAY;
  instance.teamBlue = WRECK_GRAY;
  instance.brightness = WRECK_START_BRIGHTNESS * (1.0f - life);
  return instance;
}

void AppendBlasts(const WreckSet& _wrecks, std::uint64_t _nowMilliseconds, float _unitsPerPixel,
                  std::vector<Neuron::BeamInstance>& _ioBeams)
{
  for (const Wreck& wreck : _wrecks.Wrecks())
  {
    if ((_nowMilliseconds < wreck.diedMilliseconds) || (_nowMilliseconds >= (wreck.diedMilliseconds + BLAST_MILLISECONDS)))
    {
      continue;
    }
    const float age = static_cast<float>(_nowMilliseconds - wreck.diedMilliseconds) / static_cast<float>(BLAST_MILLISECONDS);
    const float fade = 1.0f - age;
    const float reach = SizeUnitsOf(wreck.last) * BLAST_REACH_OF_SIZE * (0.5f + (0.5f * age));
    const float centerX = WorldX(wreck.last);
    const float centerY = WorldY(wreck.last);

    for (int ray = 0; ray < BLAST_RAYS; ++ray)
    {
      const float angle = (static_cast<float>(ray) + (0.5f * static_cast<float>(wreck.last.identity & 1u))) *
                          (2.0f * std::numbers::pi_v<float> / static_cast<float>(BLAST_RAYS));
      Neuron::BeamInstance beam;
      beam.from[0] = centerX;
      beam.from[1] = centerY;
      beam.to[0] = centerX + (std::cos(angle) * reach);
      beam.to[1] = centerY + (std::sin(angle) * reach);
      beam.widthUnits = std::max(8.0f, 3.0f * _unitsPerPixel);

      // White-hot at the start, cooling to orange as it fades.
      beam.color[0] = 1.0f * fade;
      beam.color[1] = (0.85f - (0.45f * age)) * fade;
      beam.color[2] = (0.6f - (0.55f * age)) * fade;
      _ioBeams.push_back(beam);
    }
  }
}

} // namespace Outpost

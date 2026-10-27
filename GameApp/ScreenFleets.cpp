#include "pch.h"
#include "ScreenFleets.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>

namespace
{
// Groups by union and find, each set named by its first member.
class Groups
{
public:
  explicit Groups(std::size_t _count)
    : m_parent(_count)
  {
    std::iota(m_parent.begin(), m_parent.end(), std::size_t{0});
  }

  [[nodiscard]] std::size_t Root(std::size_t _member) noexcept
  {
    while (m_parent[_member] != _member)
    {
      m_parent[_member] = m_parent[m_parent[_member]];
      _member = m_parent[_member];
    }
    return _member;
  }

  // Whether the two were apart.
  bool Join(std::size_t _first, std::size_t _second) noexcept
  {
    const std::size_t first = Root(_first);
    const std::size_t second = Root(_second);
    if (first == second)
      return false;
    m_parent[std::max(first, second)] = std::min(first, second);
    return true;
  }

private:
  std::vector<std::size_t> m_parent;
};

// Whether two plates, standing on _first and _second and _platePixels wide and tall, would meet.
bool PlatesMeet(DirectX::XMFLOAT2 _first, DirectX::XMFLOAT2 _second, DirectX::XMFLOAT2 _platePixels) noexcept
{
  return std::abs(_first.x - _second.x) < _platePixels.x && std::abs(_first.y - _second.y) < _platePixels.y;
}

// Where a plate over _members of _ships stands: centered over their feet and level with the highest; nothing for none.
std::optional<DirectX::XMFLOAT2> PlateFoot(std::span<const Outpost::ScreenFleets::Ship> _ships, std::span<const std::size_t> _members)
{
  if (_members.empty())
    return std::nullopt;
  float sumX = 0.0f;
  float highest = std::numeric_limits<float>::max();
  for (const std::size_t member : _members)
  {
    sumX += _ships[member].footPixels.x;
    highest = std::min(highest, _ships[member].footPixels.y);
  }
  return DirectX::XMFLOAT2{sumX / static_cast<float>(_members.size()), highest};
}
} // namespace

void Outpost::ScreenFleets::Update(std::span<const Ship> _ships, DirectX::XMFLOAT2 _platePixels, float _elapsedSeconds)
{
  // Ships whose plates would meet on the screen, or who stand as near on the ground as a formation of their side's widest
  // ship keeps its ships, are of one fleet.
  std::vector<std::pair<PlayerId, float>> widest;
  for (const Ship& ship : _ships)
  {
    const auto side = std::ranges::find(widest, ship.owner, &std::pair<PlayerId, float>::first);
    if (side == widest.end())
      widest.emplace_back(ship.owner, ship.radiusMeters);
    else
      side->second = std::max(side->second, ship.radiusMeters);
  }
  Groups groups(_ships.size());
  for (std::size_t first = 0; first < _ships.size(); ++first)
  {
    const float reachMeters =
      FORMATION_REACH_RADII * std::ranges::find(widest, _ships[first].owner, &std::pair<PlayerId, float>::first)->second;
    for (std::size_t second = first + 1; second < _ships.size(); ++second)
    {
      const Ship& a = _ships[first];
      const Ship& b = _ships[second];
      if (a.owner != b.owner)
        continue;
      const float apartMeters = std::hypot(a.position.xMeters - b.position.xMeters, a.position.zMeters - b.position.zMeters);
      if (PlatesMeet(a.footPixels, b.footPixels, _platePixels) || apartMeters <= reachMeters)
        groups.Join(first, second);
    }
  }

  // Each fleet's members, by the index of its first; and then fleets of one owner whose plates would meet are joined, until
  // no two do.
  const auto membersOf = [&_ships, &groups]
  {
    std::vector<std::vector<std::size_t>> members(_ships.size());
    for (std::size_t ship = 0; ship < _ships.size(); ++ship)
      members[groups.Root(ship)].push_back(ship);
    std::erase_if(members, [](const std::vector<std::size_t>& _members) { return _members.empty(); });
    return members;
  };
  std::vector<std::vector<std::size_t>> fleets = membersOf();
  for (bool joined = true; joined;)
  {
    joined = false;
    for (std::size_t first = 0; first < fleets.size() && !joined; ++first)
    {
      for (std::size_t second = first + 1; second < fleets.size() && !joined; ++second)
      {
        if (_ships[fleets[first].front()].owner == _ships[fleets[second].front()].owner &&
            PlatesMeet(PlateFoot(_ships, fleets[first]).value_or(DirectX::XMFLOAT2{}),
                       PlateFoot(_ships, fleets[second]).value_or(DirectX::XMFLOAT2{}), _platePixels))
          joined = groups.Join(fleets[first].front(), fleets[second].front());
      }
    }
    if (joined)
      fleets = membersOf();
  }

  // Each takes the identifier of the last frame's fleet it shares the most ships with, the largest shares first; a tie goes
  // to the older identifier, so that the same frames always name the same fleets.
  std::vector<std::tuple<std::size_t, std::uint32_t, std::size_t, std::size_t>> shares;
  for (std::size_t fleet = 0; fleet < fleets.size(); ++fleet)
  {
    for (std::size_t last = 0; last < m_fleets.size(); ++last)
    {
      const std::size_t shared = static_cast<std::size_t>(std::ranges::count_if(
        fleets[fleet], [&](std::size_t _member) { return std::ranges::binary_search(m_fleets[last].ships, _ships[_member].id); }));
      if (shared > 0)
        shares.emplace_back(shared, m_fleets[last].id, fleet, last);
    }
  }
  std::ranges::sort(shares, [](const auto& _a, const auto& _b)
                    { return std::get<0>(_a) != std::get<0>(_b) ? std::get<0>(_a) > std::get<0>(_b) : std::get<1>(_a) < std::get<1>(_b); });
  std::vector<std::optional<std::size_t>> lastOf(fleets.size());
  std::vector<Fleet> made;
  std::vector<bool> taken(m_fleets.size(), false);
  for (const auto& share : shares)
  {
    const std::size_t fleet = std::get<2>(share);
    const std::size_t last = std::get<3>(share);
    if (!lastOf[fleet].has_value() && !taken[last])
    {
      lastOf[fleet] = last;
      taken[last] = true;
    }
  }

  // A kept fleet's plate starts where it stood last frame, moved as its ships still in it moved on the screen, so that a
  // pan or a turn of the camera carries it and a ship joining, leaving or lost does not; what is left between there and
  // where it belongs settles away.
  const float settle = std::exp(-std::max(_elapsedSeconds, 0.0f) / SETTLE_SECONDS);
  const auto lastFoot = [this](EntityId _ship) -> std::optional<DirectX::XMFLOAT2>
  {
    const auto found = std::ranges::lower_bound(m_lastFeet, _ship, {}, &std::pair<EntityId, DirectX::XMFLOAT2>::first);
    return found != m_lastFeet.end() && found->first == _ship ? std::optional(found->second) : std::nullopt;
  };
  for (std::size_t fleet = 0; fleet < fleets.size(); ++fleet)
  {
    Fleet next{.id = 0, .owner = _ships[fleets[fleet].front()].owner, .ships = {}, .footPixels = {}};
    for (const std::size_t member : fleets[fleet])
      next.ships.push_back(_ships[member].id);
    std::ranges::sort(next.ships);
    next.footPixels = PlateFoot(_ships, fleets[fleet]).value_or(DirectX::XMFLOAT2{});
    if (const std::optional<std::size_t> last = lastOf[fleet]; last.has_value())
    {
      const Fleet& before = m_fleets[*last];
      next.id = before.id;
      DirectX::XMFLOAT2 moved{};
      float still = 0.0f;
      for (const std::size_t member : fleets[fleet])
      {
        const std::optional<DirectX::XMFLOAT2> was = lastFoot(_ships[member].id);
        if (was.has_value() && std::ranges::binary_search(before.ships, _ships[member].id))
        {
          moved.x += _ships[member].footPixels.x - was->x;
          moved.y += _ships[member].footPixels.y - was->y;
          still += 1.0f;
        }
      }
      if (still > 0.0f)
      {
        const DirectX::XMFLOAT2 from{before.footPixels.x + (moved.x / still), before.footPixels.y + (moved.y / still)};
        next.footPixels = {next.footPixels.x + ((from.x - next.footPixels.x) * settle),
                           next.footPixels.y + ((from.y - next.footPixels.y) * settle)};
      }
    }
    else
      next.id = m_nextId++;
    made.push_back(std::move(next));
  }
  std::ranges::sort(made, {}, &Fleet::id);
  m_fleets = std::move(made);
  m_lastFeet.clear();
  for (const Ship& ship : _ships)
    m_lastFeet.emplace_back(ship.id, ship.footPixels);
  std::ranges::sort(m_lastFeet, {}, &std::pair<EntityId, DirectX::XMFLOAT2>::first);
}

void Outpost::ScreenFleets::Clear() noexcept
{
  m_fleets.clear();
  m_lastFeet.clear();
}

const Outpost::ScreenFleets::Fleet* Outpost::ScreenFleets::Find(std::uint32_t _id) const noexcept
{
  const auto found = std::ranges::find(m_fleets, _id, &Fleet::id);
  return found != m_fleets.end() ? &*found : nullptr;
}

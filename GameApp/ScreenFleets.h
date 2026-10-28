#pragma once

namespace Outpost
{
// The fleets on the screen, which the plates stand over (ADR-102 decision 1; Game §interface.marks): a side's ships that
// stand near each other, found afresh each frame from where the camera shows them and kept from one frame to the next by
// the ships they share, so that two fleets whose plates would meet become one as the camera pulls back, and part as it
// comes in. A fleet is the client's, from what the snapshot shows; the simulation holds none.
class ScreenFleets
{
public:
  // How near on the ground two ships stand to be of one fleet at any zoom, in footprint radii of the widest of their side's
  // ships on the screen: a formation keeps three of its widest ship's radii between neighbors (ADR-010 decision 4), so a
  // formation is one fleet however far in the camera is.
  static constexpr float FORMATION_REACH_RADII = 3.5f;
  // How long a plate takes to settle where it belongs once a ship joins its fleet or leaves it, rather than jumping there:
  // what is left of the jump shrinks by e in this time.
  static constexpr float SETTLE_SECONDS = 0.15f;

  // A ship as the screen shows it: whose it is, where it stands on the ground, its footprint, and the top of its model on
  // the screen, where a bar over it would stand, in back-buffer pixels.
  struct Ship
  {
    EntityId id;
    PlayerId owner;
    PlanePosition position;
    float radiusMeters = 0.0f;
    DirectX::XMFLOAT2 footPixels{};
  };

  // A fleet: its identifier, kept while it shares a ship with the last frame's; whose it is; its ships, in identifier order;
  // and where its plate stands, centered over its ships and level with the highest of them on the screen, in back-buffer
  // pixels.
  struct Fleet
  {
    std::uint32_t id = 0;
    PlayerId owner;
    std::vector<EntityId> ships;
    DirectX::XMFLOAT2 footPixels{};
  };

  // Finds this frame's fleets among _ships, _elapsedSeconds after the last frame's. Two ships of one owner are of one fleet
  // when their plates, _platePixels wide and tall, would meet on the screen, or when they stand as near on the ground as a
  // formation of their side's widest ship keeps its ships; and two fleets of one owner whose plates would meet become one.
  // A fleet takes the identifier of the last frame's fleet it shares the most ships with, unless a fleet sharing more has
  // taken it, and a new identifier otherwise. A kept fleet's plate moves with its ships, and when one joins, leaves or is
  // lost it starts where it stood and settles over SETTLE_SECONDS.
  void Update(std::span<const Ship> _ships, DirectX::XMFLOAT2 _platePixels, float _elapsedSeconds);

  // Forgets every fleet, as when no plate is shown.
  void Clear() noexcept;

  // In identifier order.
  [[nodiscard]] std::span<const Fleet> Fleets() const noexcept
  {
    return m_fleets;
  }

  // The fleet with identifier _id, or nullptr when there is none this frame.
  [[nodiscard]] const Fleet* Find(std::uint32_t _id) const noexcept;

private:
  std::vector<Fleet> m_fleets;
  // Where each ship of the last frame stood on the screen, in identifier order, which tells how a kept fleet moved.
  std::vector<std::pair<EntityId, DirectX::XMFLOAT2>> m_lastFeet;
  std::uint32_t m_nextId = 1;
};
} // namespace Outpost

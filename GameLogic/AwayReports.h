#pragma once

namespace Outpost
{
// What the host keeps of its seats beside the universe's state, which a save holds (ADR-080 decision 7): each seat's report
// of what its player missed while away, by player, in the order they began; and the players who have held their seats, in
// the order they first did.
struct SeatReports
{
  std::vector<std::pair<PlayerId, AwayReport>> reports;
  std::vector<PlayerId> held;

  friend bool operator==(const SeatReports&, const SeatReports&) = default;
};

// What happened to each seat's empire while its player was away (ADR-080 decision 7). The server's thread keeps a
// seat's report from the events of the snapshots its deputy plays, saves the reports with the universe, and hands a seat's to
// its player with the first snapshot after the player takes the seat again. A seat its player has never held has no
// report: the deputy plays it from the universe's start, and its player missed nothing of an empire it never had (ADR-079
// decision 4).
class AwayReports
{
public:
  // Takes the seat's snapshot of a tick before it is sent. While its player plays the seat, the seat is held from then
  // on, and a report the seat has goes with the snapshot, and is gone. While _deputyPlays, the tick's events go to a held
  // seat's report, which begins at the first such tick.
  void Take(Snapshot& _snapshot, bool _deputyPlays);

  // The seats' reports and the seats held, as a save holds them.
  [[nodiscard]] const SeatReports& Reports() const noexcept
  {
    return m_seats;
  }

  // What a save held, for a universe that comes back from it. A seat with a report counts as held.
  void Restore(SeatReports _seats) noexcept
  {
    m_seats = std::move(_seats);
  }

private:
  SeatReports m_seats;
};
} // namespace Outpost

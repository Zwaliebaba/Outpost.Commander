#pragma once

namespace Outpost
{
// What happened to each seat's empire while its player was away (Phase 5 design §11, ADR-080). The server's thread keeps a
// seat's report from the events of the snapshots its deputy plays, saves the reports with the world, and hands a seat's to
// its player with the first snapshot after the player takes the seat again.
class AwayReports
{
public:
  // Takes the seat's snapshot of a tick before it is sent. While _deputyPlays, the tick's events go to the seat's report,
  // which begins at the first such tick; once the player plays the seat again, the report goes with the snapshot, and is
  // gone.
  void Take(Snapshot& _snapshot, bool _deputyPlays);

  // Each seat's report, by player, in the order they began, as a save holds them.
  [[nodiscard]] const std::vector<std::pair<PlayerId, AwayReport>>& Reports() const noexcept
  {
    return m_reports;
  }

  // The reports a save held, for a world that comes back from it.
  void Restore(std::vector<std::pair<PlayerId, AwayReport>> _reports) noexcept
  {
    m_reports = std::move(_reports);
  }

private:
  std::vector<std::pair<PlayerId, AwayReport>> m_reports;
};
} // namespace Outpost

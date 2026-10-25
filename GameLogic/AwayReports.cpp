#include "pch.h"
#include "AwayReports.h"

#include <algorithm>

void Outpost::AwayReports::Take(Snapshot& _snapshot, bool _deputyPlays)
{
  std::vector<std::pair<PlayerId, AwayReport>>& reports = m_seats.reports;
  auto report = std::ranges::find(reports, _snapshot.player, &std::pair<PlayerId, AwayReport>::first);
  const bool held = std::ranges::contains(m_seats.held, _snapshot.player);
  if (!_deputyPlays)
  {
    if (!held)
      m_seats.held.push_back(_snapshot.player);
    if (report != reports.end())
    {
      _snapshot.away = std::move(report->second);
      reports.erase(report);
    }
    return;
  }
  if (report == reports.end())
  {
    if (!held)
      return;
    report = reports.insert(reports.end(), {_snapshot.player, AwayReport{.sinceTick = _snapshot.tick}});
  }
  for (const EventView& event : _snapshot.events)
    report->second.Record(event);
}

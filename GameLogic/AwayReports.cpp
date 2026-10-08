#include "pch.h"
#include "AwayReports.h"

#include <algorithm>

void Outpost::AwayReports::Take(Snapshot& _snapshot, bool _deputyPlays)
{
  auto report = std::ranges::find(m_reports, _snapshot.player, &std::pair<PlayerId, AwayReport>::first);
  if (!_deputyPlays)
  {
    if (report != m_reports.end())
    {
      _snapshot.away = std::move(report->second);
      m_reports.erase(report);
    }
    return;
  }
  if (report == m_reports.end())
    report = m_reports.insert(m_reports.end(), {_snapshot.player, AwayReport{.sinceTick = _snapshot.tick}});
  for (const EventView& event : _snapshot.events)
    report->second.Record(event);
}

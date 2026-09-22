#include "pch.h"

#include "OrderMarker.h"

#include <algorithm>

namespace Outpost
{

void OrderMarkerSet::Add(OrderMarker _marker)
{
  m_markers.push_back(std::move(_marker));
}

std::size_t OrderMarkerSet::ClearAcknowledged(std::uint16_t _lastCommandSequenceApplied) noexcept
{
  const std::size_t before = m_markers.size();

  // A marker is cleared when its sequence is NOT newer than the acknowledgment -- which is "at or
  // before" written the only way that survives the wrap. One later sequence therefore clears every
  // marker it covers, which is the case a player produces by tapping three times in a second.
  const auto acknowledged = [_lastCommandSequenceApplied](const OrderMarker& _marker) noexcept
  { return !SequenceIsNewer(_marker.commandSequence, _lastCommandSequenceApplied); };

  m_markers.erase(std::remove_if(m_markers.begin(), m_markers.end(), acknowledged), m_markers.end());
  return before - m_markers.size();
}

} // namespace Outpost

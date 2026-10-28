#include "pch.h"
#include "OverseerModel.h"

void Outpost::OverseerModel::Take(std::span<const std::byte> _message, std::chrono::steady_clock::time_point _now)
{
  OverseerMessage message = DecodeOverseerMessage(_message);
  if (auto* welcome = std::get_if<OverseerWelcomeMessage>(&message))
  {
    // A new connection: the server sends it whole views first, and the log's last records again, which replace those kept.
    m_catalog = CatalogMessage{.sectors = welcome->sectors, .fields = welcome->fields};
    m_welcome = std::move(*welcome);
    m_welcomed = true;
    m_views = HeldStates();
    m_records.clear();
    m_closing.reset();
    m_lostWhy.reset();
    m_lostAt.reset();
    return;
  }
  if (auto* health = std::get_if<HealthMessage>(&message))
  {
    m_health.push_back({.heard = _now, .health = std::move(*health)});
    while (m_health.size() > HEALTH_KEPT)
      m_health.pop_front();
    return;
  }
  if (auto* records = std::get_if<LogRecordsMessage>(&message))
  {
    std::ranges::move(records->lines, std::back_inserter(m_records));
    while (m_records.size() > RECORDS_KEPT)
      m_records.pop_front();
    return;
  }
  if (auto* closing = std::get_if<ClosingMessage>(&message))
  {
    m_closing = std::move(*closing);
    return;
  }
  throw Neuron::Exception("The server sent the overseer a message only an overseer sends.");
}

std::optional<std::uint64_t> Outpost::OverseerModel::TakeView(std::span<const std::byte> _view)
{
  if (!m_welcomed)
    return std::nullopt;
  StateMessage state = m_views.Take(DecodeMessage(_view));
  const std::uint64_t tick = state.snapshot.tick;
  PutCatalog(m_catalog, state.snapshot);
  m_view = std::move(state.snapshot);
  return tick;
}

void Outpost::OverseerModel::Lost(std::string _why, std::chrono::steady_clock::time_point _now)
{
  m_lostWhy = std::move(_why);
  m_lostAt = _now;
  m_welcomed = false;
}

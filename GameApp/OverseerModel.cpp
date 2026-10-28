#include "pch.h"
#include "OverseerModel.h"

void Outpost::OverseerModel::Take(std::span<const std::byte> _message, std::chrono::steady_clock::time_point _now)
{
  OverseerMessage message = DecodeOverseerMessage(_message);
  if (auto* welcome = std::get_if<OverseerWelcomeMessage>(&message))
  {
    // A new connection: the server sends it whole views first, and the log's last records again, which replace those kept.
    m_catalog = CatalogMessage{.sectors = welcome->sectors, .fields = welcome->fields};
    m_dayRecords.assign(std::make_move_iterator(welcome->dayRecords.begin()), std::make_move_iterator(welcome->dayRecords.end()));
    welcome->dayRecords.clear();
    for (const std::string& record : m_dayRecords)
      m_newestTick = std::max(m_newestTick, HeadOf(record).value_or(RecordHead{}).tick);
    m_welcome = std::move(*welcome);
    m_welcomed = true;
    m_views = HeldStates();
    m_records.clear();
    m_closing.reset();
    m_closingAt.reset();
    m_lostWhy.reset();
    m_lostAt.reset();
    return;
  }
  if (auto* health = std::get_if<HealthMessage>(&message))
  {
    m_newestTick = std::max(m_newestTick, health->tick);
    m_health.push_back({.heard = _now, .health = std::move(*health)});
    while (m_health.size() > HEALTH_KEPT)
      m_health.pop_front();
    KeepTheDay();
    return;
  }
  if (auto* records = std::get_if<LogRecordsMessage>(&message))
  {
    for (const std::string& line : records->lines)
    {
      if (const std::optional<RecordHead> head = HeadOf(line))
      {
        m_newestTick = std::max(m_newestTick, head->tick);
        // The log's last records, sent again after the welcome, hold day's records the welcome already gave, and older ones the
        // day has dropped, among them a start that would go back past newer records: only one not yet kept, of the day, is new.
        if (IsDayRecord(head->kind) && head->tick >= DayStart() && std::ranges::find(m_dayRecords, line) == m_dayRecords.end())
          m_dayRecords.push_back(line);
      }
    }
    std::ranges::move(records->lines, std::back_inserter(m_records));
    while (m_records.size() > RECORDS_KEPT)
      m_records.pop_front();
    KeepTheDay();
    return;
  }
  if (auto* closing = std::get_if<ClosingMessage>(&message))
  {
    m_closing = std::move(*closing);
    m_closingAt = _now;
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

std::uint64_t Outpost::OverseerModel::DayStart() const noexcept
{
  const std::uint64_t day = DAY_RECORDS_SECONDS * (m_welcome.has_value() ? m_welcome->ticksPerSecond : 0);
  return m_newestTick > day ? m_newestTick - day : 0;
}

void Outpost::OverseerModel::KeepTheDay()
{
  KeepDayRecords(m_dayRecords, DayStart());
}

void Outpost::OverseerModel::Lost(std::string _why, std::chrono::steady_clock::time_point _now)
{
  m_lostWhy = std::move(_why);
  m_lostAt = _now;
  m_welcomed = false;
}

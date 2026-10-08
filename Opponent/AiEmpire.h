#pragma once

namespace Outpost
{
// A world's AI empire (Phase 5 design §6): the AI, playing a seat of its own on the server's thread, to win (ADR-079). Made
// afresh when the server restarts, it plays on from the world the server restored (ADR-020 decision 16); and made afresh
// when its seat restarts after a loss, it lays its plan round its new Command Station (design §8).
class AiEmpire final : public HostedPlayer
{
public:
  AiEmpire(AiSettings _settings, std::uint32_t _ticksPerSecond)
    : m_settings(_settings),
      m_ticksPerSecond(_ticksPerSecond),
      m_ai(std::move(_settings), _ticksPerSecond)
  {
  }

  [[nodiscard]] std::vector<Command> Play(const Snapshot& _snapshot) override
  {
    if (std::ranges::any_of(_snapshot.events, [](const EventView& _event) { return _event.kind == EventKind::EmpireRestarted; }))
      m_ai = AiPlayer(m_settings, m_ticksPerSecond);
    return m_ai.Update(_snapshot);
  }

private:
  AiSettings m_settings;
  std::uint32_t m_ticksPerSecond = 0;
  AiPlayer m_ai;
};
} // namespace Outpost

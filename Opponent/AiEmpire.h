#pragma once

namespace Outpost
{
// A world's AI empire (Phase 5 design §6): the AI, playing a seat of its own on the server's thread, to win (ADR-079). Made
// afresh when the server restarts, it plays on from the world the server restored (ADR-020 decision 16).
class AiEmpire final : public HostedPlayer
{
public:
  AiEmpire(AiSettings _settings, std::uint32_t _ticksPerSecond)
    : m_ai(std::move(_settings), _ticksPerSecond)
  {
  }

  [[nodiscard]] std::vector<Command> Play(const Snapshot& _snapshot) override
  {
    return m_ai.Update(_snapshot);
  }

private:
  AiPlayer m_ai;
};
} // namespace Outpost

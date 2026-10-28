#pragma once

namespace Outpost
{
// Where a universe's server listens for its overseer, and the token the overseer shows (ADR-101).
struct OverseerDesc
{
  // An IPv4 or IPv6 address, "*" for every address of both, or empty for the loopback, as QuicListener::Desc takes it; and
  // the port, zero letting the system choose one.
  std::string address;
  std::uint16_t port = 0;
  SeatToken token{};
};

// The server's end of its overseer's channel (ADR-101): a QUIC listener of its own beside the game's, presenting the
// universe's kept certificate, and the one overseer that holds the channel, the newest connection to show the token. It
// welcomes an overseer on MsQuic's thread with what it was made with and the day's records it keeps, so that it does while
// the server's thread is stuck.
// The server's main thread sends Health and Closing through it, and the server's thread the views and the log's records.
// Every member may be called from any thread, apart from where it says otherwise.
class OverseerLink : Neuron::NonCopyable
{
public:
  // Listens on _desc's address, presenting the certificate kept in _identity, and welcomes an overseer with _welcome, with the
  // day's records of the last DAY_RECORDS_SECONDS of universe time, _dayRecords and those written since, and then _records,
  // the universe log's last ones. Throws Neuron::Exception when it cannot listen.
  OverseerLink(const OverseerDesc& _desc, const std::filesystem::path& _identity, OverseerWelcomeMessage _welcome,
               std::deque<std::string> _dayRecords, std::vector<std::string> _records);
  // Stops listening and closes the overseer's connection.
  ~OverseerLink();

  [[nodiscard]] std::uint16_t Port() const noexcept;
  [[nodiscard]] const Neuron::CertificateHash& Certificate() const noexcept;

  // Closes the connections that have said no hello in time, lets go of those that have gone, and returns the overseer's
  // comings and goings since the last call, for the universe log: the server's thread's, once a tick, never MsQuic's.
  [[nodiscard]] std::vector<OverseerRecord> Sweep();

  // Whether an overseer holds the channel; and the seat whose sight it watches, no player for full sight.
  [[nodiscard]] bool Held() const;
  [[nodiscard]] PlayerId Sight() const;

  // A view of the universe, without its catalogue and its events, as the game's own state message: whole, or as what changed
  // since the newest view the overseer holds (ADR-098), on a stream of its own that a newer view abandons (ADR-096). The
  // server's thread's.
  void SendView(Snapshot _view);

  // Records the universe log wrote, in order: it keeps the last OVERSEER_RECORDS_KEPT of them for the next overseer, and the
  // day's records of the last DAY_RECORDS_SECONDS of universe time for its welcome, and sends them to the one that holds the
  // channel.
  void SendRecords(std::vector<std::string> _lines);

  // Health, to the overseer that holds the channel, if one does.
  void SendHealth(const HealthMessage& _health);

  // Closing, to the overseer that holds the channel, if one does; then waits up to _wait for it to have all that was sent,
  // and closes its connection with OverseerCloseReason::Finished. Never from MsQuic's thread.
  void SendClosing(const ClosingMessage& _closing, std::chrono::milliseconds _wait);

private:
  // What the listener does with a connection: its first message is a hello that shows the token, and every message after it
  // a sight or the view it holds. Called on MsQuic's thread.
  [[nodiscard]] Neuron::QuicChannel::Receiver Admit(const std::shared_ptr<Neuron::QuicChannel>& _channel);

  // Gives the channel to _channel, sending it the welcome and the records kept, and closes the overseer that held it.
  void Welcome(std::shared_ptr<Neuron::QuicChannel> _channel);

  // Whether _channel holds the channel now, which one a newer connection took it from no longer does.
  [[nodiscard]] bool Holds(const Neuron::QuicChannel* _channel) const;

  // Keeps a refusal for the universe log, and closes _channel saying why.
  void Refuse(Neuron::QuicChannel& _channel, OverseerOutcome _outcome, OverseerCloseReason _reason);

  // The first tick of the day of universe time the day's records cover, which ends at the newest record written. Under the lock.
  [[nodiscard]] std::uint64_t DayStart() const noexcept;

  SeatToken m_token{};
  // The welcome, without the day's records, which are added to it as it is sent; and the seats a sight may name.
  OverseerWelcomeMessage m_welcome;
  std::vector<PlayerId> m_seats;

  // Guards what follows it, which MsQuic's threads and the server's two threads all touch. Every message to the overseer's
  // stream is sent under it, so that the welcome comes first and the records kept follow it before any newer one.
  mutable std::mutex m_mutex;
  // The overseer that holds the channel, and how many have taken it; the sight it watches and the tick of the newest view it
  // holds; the universe log's last records; and the comings and goings Sweep has yet to take.
  std::shared_ptr<Neuron::QuicChannel> m_overseer;
  std::uint32_t m_takings = 0;
  PlayerId m_sight;
  std::optional<std::uint64_t> m_held;
  std::deque<std::string> m_records;
  // The day's records kept, and the tick of the newest record written, which the day they cover ends at.
  std::deque<std::string> m_dayRecords;
  std::uint64_t m_newestTick = 0;
  std::vector<OverseerRecord> m_comings;

  // The views sent to the overseer of one taking, which the next is sent as a change of: the server's thread's alone.
  SentStates m_viewsSent;
  std::uint32_t m_viewsTaking = 0;

  // Last, so that it is destroyed first: no callback of MsQuic's outlives what it touches.
  std::unique_ptr<Neuron::QuicListener> m_listener;
};
} // namespace Outpost

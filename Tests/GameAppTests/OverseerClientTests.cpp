#include "pch.h"

#include <mutex>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameAppTests
{
namespace
{
using Clock = std::chrono::steady_clock;

// The server's end of the overseer's channel, as a test plays it on a listener of its own: it keeps what each connection
// says, welcomes a hello, and holds the newest connection, which it sends views on and closes.
class OverseerServer
{
public:
  OverseerServer()
    : m_listener({.applicationProtocol = std::string(Outpost::OVERSEER_APPLICATION_PROTOCOL),
                  .messageLimitBytes = Outpost::OVERSEER_MESSAGE_LIMIT_BYTES},
                 [this](const std::shared_ptr<Neuron::QuicChannel>& _channel)
                 {
                   {
                     const std::scoped_lock lock(m_mutex);
                     m_newest = _channel;
                     ++m_connections;
                   }
                   return [this](Neuron::QuicChannel& _from, std::vector<std::byte> _message)
                   {
                     const Outpost::OverseerMessage message = Outpost::DecodeOverseerMessage(_message);
                     if (std::holds_alternative<Outpost::OverseerHelloMessage>(message))
                       _from.Send(Outpost::EncodeOverseerMessage(Outpost::OverseerWelcomeMessage{.ticksPerSecond = 20, .folderUtf8 = "U"}));
                     const std::scoped_lock lock(m_mutex);
                     m_heard.push_back(message);
                   };
                 })
  {
  }

  [[nodiscard]] Outpost::OverseerTicket TicketWith(const Outpost::SeatToken& _token) const
  {
    return {.address = {.host = "127.0.0.1", .port = m_listener.Port(), .certificate = m_listener.Certificate(), .token = _token}};
  }

  [[nodiscard]] std::vector<Outpost::OverseerMessage> Heard()
  {
    const std::scoped_lock lock(m_mutex);
    return m_heard;
  }

  [[nodiscard]] std::size_t Connections()
  {
    const std::scoped_lock lock(m_mutex);
    return m_connections;
  }

  [[nodiscard]] std::shared_ptr<Neuron::QuicChannel> Newest()
  {
    const std::scoped_lock lock(m_mutex);
    return m_newest;
  }

private:
  std::mutex m_mutex;
  std::vector<Outpost::OverseerMessage> m_heard;
  std::shared_ptr<Neuron::QuicChannel> m_newest;
  std::size_t m_connections = 0;
  // Last, so that it closes its channels, and their receivers stop, before the rest goes.
  Neuron::QuicListener m_listener;
};

// Updates _client at the clock, _skew ahead, until _done says so or 10 s have gone; whether it did.
template <typename Done> bool UpdateUntil(Outpost::OverseerClient& _client, Done _done, Clock::duration _skew = {})
{
  const auto deadline = Clock::now() + 10s;
  while (Clock::now() < deadline)
  {
    _client.Update(Clock::now() + _skew);
    if (_done())
      return true;
    std::this_thread::sleep_for(10ms);
  }
  return false;
}

// What the server heard of type T, in order.
template <typename T> std::vector<T> HeardOf(OverseerServer& _server)
{
  std::vector<T> found;
  for (const Outpost::OverseerMessage& message : _server.Heard())
  {
    if (const auto* typed = std::get_if<T>(&message))
      found.push_back(*typed);
  }
  return found;
}
} // namespace

TEST_CLASS(OverseerClientTests)
{
public:
  // ADR-102: the overseer connects with its file's address and certificate, says hello with its token and then the sight it
  // watches, tells the server each view it holds, and, once the connection goes, says why and joins again after
  // REJOIN_INTERVAL. QUIC runs in CI's Windows run only.
  TEST_METHOD(SaysHelloAndTheSightAndJoinsAgain)
  {
    OverseerServer server;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Outpost::OverseerClient client(server.TicketWith(token));
    client.Watch(Outpost::PlayerId{2});
    Assert::IsTrue(UpdateUntil(client, [&] { return client.Model().Welcome().has_value(); }), L"welcomed");
    Assert::IsTrue(client.Connected());
    Assert::IsTrue(UpdateUntil(client, [&] { return HeardOf<Outpost::WatchSightMessage>(server).size() == 1; }), L"the sight told");
    const std::vector<Outpost::OverseerHelloMessage> hellos = HeardOf<Outpost::OverseerHelloMessage>(server);
    Assert::AreEqual(std::size_t{1}, hellos.size());
    Assert::IsTrue(hellos.front().token == token, L"with the file's token");
    Assert::IsTrue(std::holds_alternative<Outpost::OverseerHelloMessage>(server.Heard().front()), L"hello first");
    Assert::AreEqual(std::uint32_t{2}, HeardOf<Outpost::WatchSightMessage>(server).front().seat.value, L"the seat it watches");

    // A view, whole, which the overseer holds and says so.
    Outpost::Snapshot view{.tick = 40};
    Assert::IsTrue(server.Newest()->SendNewest(Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = view})));
    Assert::IsTrue(UpdateUntil(client, [&] { return !HeardOf<Outpost::ViewHeldMessage>(server).empty(); }), L"the view held told");
    Assert::AreEqual(std::uint64_t{40}, HeardOf<Outpost::ViewHeldMessage>(server).front().tick);
    Assert::AreEqual(std::uint64_t{40}, client.Model().View().value_or(Outpost::Snapshot{}).tick);

    // The server takes the channel away, saying why; the overseer says so and joins again after REJOIN_INTERVAL.
    server.Newest()->Shutdown(static_cast<std::uint64_t>(Outpost::OverseerCloseReason::Taken));
    Assert::IsTrue(UpdateUntil(client, [&] { return !client.Connected() && !client.Connecting(); }), L"gone");
    Assert::AreEqual(
      std::string(Outpost::DescribeOverseerClose(static_cast<std::uint64_t>(Outpost::OverseerCloseReason::Taken)).value_or("")),
      client.Model().LostWhy().value_or(""), L"saying the server's reason");
    client.Update(Clock::now());
    Assert::IsFalse(client.Connecting(), L"not at once");
    Assert::IsTrue(UpdateUntil(
                     client, [&] { return server.Connections() == 2 && HeardOf<Outpost::OverseerHelloMessage>(server).size() == 2; },
                     Outpost::OverseerClient::REJOIN_INTERVAL),
                   L"joined again after the interval");
    Assert::IsTrue(
      UpdateUntil(client, [&] { return HeardOf<Outpost::WatchSightMessage>(server).size() == 2; }, 3s), L"telling the sight again");
  }
};
} // namespace GameAppTests

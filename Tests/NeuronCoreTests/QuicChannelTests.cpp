#include "pch.h"

#include <algorithm>
#include <mutex>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace NeuronCoreTests
{
namespace
{
constexpr std::string_view PROTOCOL = "neuron-tests/1";
// How long a test waits for what it expects over the loopback before it fails.
constexpr auto PATIENCE = 10s;

using Message = std::vector<std::byte>;

Message Bytes(std::string_view _text)
{
  Message message(_text.size());
  std::ranges::transform(_text, message.begin(), [](char _character) { return static_cast<std::byte>(_character); });
  return message;
}

std::unique_ptr<Neuron::QuicChannel> ConnectTo(const Neuron::QuicListener& _listener)
{
  return Neuron::QuicChannel::Connect({.host = "127.0.0.1",
                                       .port = _listener.Port(),
                                       .applicationProtocol = std::string(PROTOCOL),
                                       .serverCertificate = _listener.Certificate()});
}

// The messages _channel receives until it has _count of them.
std::vector<Message> ReceiveMessages(Neuron::QuicChannel& _channel, size_t _count)
{
  std::vector<Message> received;
  const auto deadline = std::chrono::steady_clock::now() + PATIENCE;
  while (received.size() < _count && std::chrono::steady_clock::now() < deadline)
  {
    for (Message& message : _channel.Receive(100ms))
      received.push_back(std::move(message));
  }
  Assert::AreEqual(_count, received.size());
  return received;
}

// The messages of which only the newest matters that _channel hands on, until the last of them is _last; the messages of its
// connection's stream that come meanwhile go to _ordered.
std::vector<Message> ReceiveNewestUntil(Neuron::QuicChannel& _channel, const Message& _last, std::vector<Message>& _ordered)
{
  std::vector<Message> received;
  const auto deadline = std::chrono::steady_clock::now() + PATIENCE;
  while ((received.empty() || received.back() != _last) && std::chrono::steady_clock::now() < deadline)
  {
    for (Message& message : _channel.Receive(100ms))
      _ordered.push_back(std::move(message));
    for (Message& message : _channel.ReceiveNewest())
      received.push_back(std::move(message));
  }
  Assert::IsTrue(!received.empty() && received.back() == _last, L"the newest arrives");
  return received;
}

// _channel's figures once its backlog has come back to nothing, or as they are when it has not within the patience.
Neuron::QuicStatistics Settled(const Neuron::QuicChannel& _channel)
{
  const auto deadline = std::chrono::steady_clock::now() + PATIENCE;
  Neuron::QuicStatistics statistics = _channel.Statistics();
  while (statistics.backlogBytes != 0 && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(10ms);
    statistics = _channel.Statistics();
  }
  return statistics;
}

// Why _channel's connection went, once it has; what arrived before then goes to _before.
std::string FailureOf(Neuron::QuicChannel& _channel, std::vector<Message>& _before)
{
  const auto deadline = std::chrono::steady_clock::now() + PATIENCE;
  while (std::chrono::steady_clock::now() < deadline)
  {
    try
    {
      for (Message& message : _channel.Receive(100ms))
        _before.push_back(std::move(message));
    }
    catch (const Neuron::Exception& error)
    {
      return error.what();
    }
  }
  // Assert::Fail does not return.
  Assert::Fail(L"the connection stayed open");
}
} // namespace

// The QUIC channel under the game's transport (ADR-004, ADR-060): length-framed byte strings over one stream, which knows
// no game concept (R9).
TEST_CLASS(QuicChannelTests)
{
public:
  // Messages arrive whole and in order both ways, an empty one and one too large for a single read among them.
  TEST_METHOD(CarriesMessagesBothWaysInOrder)
  {
    // The server's end answers each message with its bytes reversed.
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                        {
                                          return [](Neuron::QuicChannel& _channel, Message _message)
                                          {
                                            std::ranges::reverse(_message);
                                            _channel.Send(_message);
                                          };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);

    Message large(300'000);
    for (size_t i = 0; i < large.size(); ++i)
      large[i] = static_cast<std::byte>((i * 7) % 251);
    const std::vector<Message> sent{Bytes("first"), Message{}, large, Bytes("last")};
    for (const Message& message : sent)
      client->Send(message);

    const std::vector<Message> received = ReceiveMessages(*client, sent.size());
    for (size_t i = 0; i < sent.size(); ++i)
    {
      Message expected = sent[i];
      std::ranges::reverse(expected);
      Assert::IsTrue(expected == received[i], std::format(L"message {}", i).c_str());
    }
  }

  // A message over the limit is refused before it is sent: the peer would close the connection on it.
  TEST_METHOD(RefusesToSendAMessageOverTheLimit)
  {
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver {
                                          return [](Neuron::QuicChannel& _channel, const Message& _message) { _channel.Send(_message); };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    constexpr size_t LIMIT_BYTES = size_t{64} * 1024 * 1024;
    const Message over(LIMIT_BYTES + 1);
    std::string message;
    try
    {
      client->Send(over);
    }
    catch (const Neuron::Exception& error)
    {
      message = error.what();
    }
    Assert::AreEqual(std::string("A message of 67108865 bytes is over the limit of 67108864."), message);

    // The channel is still open after the refusal.
    client->Send(Bytes("still here"));
    Assert::IsTrue(Bytes("still here") == ReceiveMessages(*client, 1).front());
  }

  // A Receiver that throws shuts the connection down with FRAMING_ERROR_CODE, takes no message after, and the client is
  // told why. The answer to the good message is waited for first: a shutdown need not deliver what is still queued.
  TEST_METHOD(ClosesTheConnectionWhenTheReceiverThrows)
  {
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                        {
                                          return [](Neuron::QuicChannel& _channel, Message _message)
                                          {
                                            if (_message == Bytes("bad"))
                                              throw Neuron::Exception("the receiver cannot take it");
                                            _channel.Send(_message);
                                          };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    client->Send(Bytes("good"));
    Assert::IsTrue(Bytes("good") == ReceiveMessages(*client, 1).front());
    client->Send(Bytes("bad"));
    client->Send(Bytes("never answered"));

    std::vector<Message> after;
    const std::string failure = FailureOf(*client, after);
    Assert::AreEqual(std::format("The peer closed the QUIC connection with error code {}.", Neuron::FRAMING_ERROR_CODE), failure);
    Assert::IsTrue(after.empty(), L"nothing after the bad message was taken");
  }

  // Once a channel is closed it says so, and what is sent on it is dropped rather than thrown.
  TEST_METHOD(SaysWhenItIsClosed)
  {
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                        { return [](Neuron::QuicChannel&, const Message&) {}; });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    client->Close();
    client->Send(Bytes("dropped"));
    std::string message;
    try
    {
      (void)client->Receive();
    }
    catch (const Neuron::Exception& error)
    {
      message = error.what();
    }
    Assert::AreEqual(std::string("The QUIC connection is closed."), message);
  }

  // With nothing listening, Connect gives up within its timeout, naming where it tried.
  TEST_METHOD(GivesUpWhenNothingListens)
  {
    std::uint16_t port = 0;
    {
      const Neuron::QuicListener gone({.applicationProtocol = std::string(PROTOCOL)},
                                      [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver { return {}; });
      port = gone.Port();
    }
    const auto started = std::chrono::steady_clock::now();
    std::string message;
    try
    {
      (void)Neuron::QuicChannel::Connect(
        {.host = "127.0.0.1", .port = port, .applicationProtocol = std::string(PROTOCOL), .connectTimeout = 1'000ms});
    }
    catch (const Neuron::Exception& error)
    {
      message = error.what();
    }
    Assert::IsTrue(message.starts_with(std::format("Could not connect to 127.0.0.1:{} over QUIC", port)),
                   std::wstring(message.begin(), message.end()).c_str());
    Assert::IsTrue(std::chrono::steady_clock::now() - started < PATIENCE, L"it gave up in time");
  }

  // ADR-078 decision 6: a listener on every address takes a client that reaches it on either family, localhost among them,
  // and one on IPv4's every interface has none for a client that reaches it on IPv6, which Connect names.
  TEST_METHOD(ListensOnEveryAddressOfBothFamilies)
  {
    const auto accept = [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver { return {}; };
    const Neuron::QuicListener everywhere(
      {.applicationProtocol = std::string(PROTOCOL), .address = std::string(Neuron::QuicListener::EVERY_ADDRESS)}, accept);
    for (const char* host : {"127.0.0.1", "::1", "localhost"})
      (void)Neuron::QuicChannel::Connect({.host = host,
                                          .port = everywhere.Port(),
                                          .applicationProtocol = std::string(PROTOCOL),
                                          .serverCertificate = everywhere.Certificate()});

    const Neuron::QuicListener ipv4({.applicationProtocol = std::string(PROTOCOL), .address = "0.0.0.0"}, accept);
    std::string message;
    try
    {
      (void)Neuron::QuicChannel::Connect(
        {.host = "::1", .port = ipv4.Port(), .applicationProtocol = std::string(PROTOCOL), .serverCertificate = ipv4.Certificate()});
    }
    catch (const Neuron::Exception& error)
    {
      message = error.what();
    }
    Assert::IsTrue(message.contains("0x80410007") && message.contains("no listener"), std::wstring(message.begin(), message.end()).c_str());
  }

  // ADR-060 decision 2: a client is held to the listener's limit until its Receiver sets another, which holds from the
  // very next message, though both came in one read.
  TEST_METHOD(HoldsAPeerToTheLimitItsReceiverSets)
  {
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL), .messageLimitBytes = 8},
                                        [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                        {
                                          return [](Neuron::QuicChannel& _channel, const Message& _message)
                                          {
                                            _channel.LimitMessages(64);
                                            _channel.Send(_message);
                                          };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    const Message longer(40, std::byte{7});
    client->Send(Bytes("hello"));
    client->Send(longer);
    const std::vector<Message> echoed = ReceiveMessages(*client, 2);
    Assert::IsTrue(echoed[1] == longer, L"the second message under the limit the first one's Receiver set");
    client->Send(Message(65));
    std::vector<Message> after;
    Assert::AreEqual(std::format("The peer closed the QUIC connection with error code {}.", Neuron::FRAMING_ERROR_CODE),
                     FailureOf(*client, after), L"one over it breaks the framing");

    const std::unique_ptr<Neuron::QuicChannel> early = ConnectTo(listener);
    early->Send(Message(9));
    Assert::AreEqual(std::format("The peer closed the QUIC connection with error code {}.", Neuron::FRAMING_ERROR_CODE),
                     FailureOf(*early, after), L"a first message over the listener's limit");
  }

  // ADR-060 decision 7: a sweep lets go of the channels whose connections MsQuic has finished with and that nothing else
  // holds, shuts down the ones that have said nothing for the time it is given, and keeps the rest. The connection that goes
  // is one its server's end shuts down, as a server does a refused hello's or a seat's taken again: MsQuic may hold one its
  // client closed until its idle timeout.
  TEST_METHOD(SweepsAwayWhatHasGoneAndClosesWhatSaysNothing)
  {
    constexpr std::uint64_t SILENT_CODE = 42;
    Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                  [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                  {
                                    return [](Neuron::QuicChannel& _channel, const Message& _message)
                                    {
                                      if (_message == Bytes("bye"))
                                        _channel.Shutdown(0);
                                      else
                                        _channel.Send(_message);
                                    };
                                  });
    const std::unique_ptr<Neuron::QuicChannel> talker = ConnectTo(listener);
    talker->Send(Bytes("here"));
    (void)ReceiveMessages(*talker, 1);
    const std::unique_ptr<Neuron::QuicChannel> silent = ConnectTo(listener);
    const std::unique_ptr<Neuron::QuicChannel> leaving = ConnectTo(listener);
    leaving->Send(Bytes("bye"));
    Assert::AreEqual(size_t{3}, listener.ChannelCount());

    // Swept as a server's tick sweeps it, until the one shut down has been let go and the silent one has gone too.
    std::vector<Message> after;
    std::string why;
    const auto deadline = std::chrono::steady_clock::now() + PATIENCE;
    while (std::chrono::steady_clock::now() < deadline && (why.empty() || listener.ChannelCount() != 1))
    {
      listener.Sweep(200ms, SILENT_CODE);
      if (why.empty() && silent->HasGone())
        why = FailureOf(*silent, after);
      std::this_thread::sleep_for(10ms);
    }
    Assert::AreEqual(std::format("The peer closed the QUIC connection with error code {}.", SILENT_CODE), why, L"the silent one");
    Assert::AreEqual(size_t{1}, listener.ChannelCount(), L"only the one that spoke is kept");
    talker->Send(Bytes("still here"));
    Assert::IsTrue(Bytes("still here") == ReceiveMessages(*talker, 1).front(), L"and it plays on");
  }

  // ADR-004 decision 6: each end reports its connection's round trip, the packets it has sent and its congestion window, and
  // a backlog that counts a message until the peer has acknowledged it and comes back to nothing once the peer has every one.
  TEST_METHOD(ReportsItsConnection)
  {
    // Declared before the listener, so that the server's end is let go of after the listener has closed it.
    std::mutex mutex;
    std::shared_ptr<Neuron::QuicChannel> serverEnd;
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [&](const std::shared_ptr<Neuron::QuicChannel>& _accepted) -> Neuron::QuicChannel::Receiver
                                        {
                                          const std::scoped_lock lock(mutex);
                                          serverEnd = _accepted;
                                          return [](Neuron::QuicChannel& _channel, const Message& _message) { _channel.Send(_message); };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    Assert::AreEqual(std::uint64_t{0}, client->Statistics().backlogBytes, L"nothing sent, nothing waiting");

    // Far more than a loopback carries in the moment between the send and the figures.
    const Message large(2'000'000, std::byte{7});
    client->Send(large);
    Assert::IsTrue(client->Statistics().backlogBytes >= large.size(), L"a message counts until the peer has it");
    Assert::IsTrue(large == ReceiveMessages(*client, 1).front(), L"and comes back");

    const Neuron::QuicStatistics clientFigures = Settled(*client);
    Assert::AreEqual(std::uint64_t{0}, clientFigures.backlogBytes, L"the server has the client's message");
    Assert::IsTrue(clientFigures.roundTripMicroseconds > 0, L"a round trip, once a message has crossed");
    Assert::IsTrue(clientFigures.leastRoundTripMicroseconds <= clientFigures.roundTripMicroseconds, L"the least no more than the smoothed");
    Assert::IsTrue(clientFigures.packetsSent > 100, L"the packets a long message takes");
    Assert::IsTrue(clientFigures.congestionWindowBytes > 0, L"a congestion window");

    std::shared_ptr<Neuron::QuicChannel> server;
    {
      const std::scoped_lock lock(mutex);
      server = serverEnd;
    }
    Assert::IsTrue(server != nullptr, L"the server's end");
    const Neuron::QuicStatistics serverFigures = Settled(*server);
    Assert::AreEqual(std::uint64_t{0}, serverFigures.backlogBytes, L"the client has the server's answer");
    Assert::IsTrue(serverFigures.roundTripMicroseconds > 0 && serverFigures.packetsSent > 100, L"and its own figures");
  }

  // ADR-096: the server's end sends messages of which only the newest matters, each on a stream of its own beside the
  // connection's, and the client's end hands on each that arrives whole and newer than the last. While two are on their
  // way, one more is dropped rather than either abandoned; once they have arrived, the next goes.
  TEST_METHOD(CarriesTheNewestMessagesApart)
  {
    std::mutex mutex;
    std::shared_ptr<Neuron::QuicChannel> serverEnd;
    const Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                        [&](const std::shared_ptr<Neuron::QuicChannel>& _accepted) -> Neuron::QuicChannel::Receiver
                                        {
                                          const std::scoped_lock lock(mutex);
                                          serverEnd = _accepted;
                                          return [](Neuron::QuicChannel& _channel, const Message& _message) { _channel.Send(_message); };
                                        });
    const std::unique_ptr<Neuron::QuicChannel> client = ConnectTo(listener);
    client->Send(Bytes("hello"));
    (void)ReceiveMessages(*client, 1);
    std::shared_ptr<Neuron::QuicChannel> server;
    {
      const std::scoped_lock lock(mutex);
      server = serverEnd;
    }
    Assert::IsTrue(server != nullptr, L"the server's end");

    std::vector<Message> ordered;
    server->SendNewest(Bytes("first"));
    Assert::AreEqual(size_t{1}, ReceiveNewestUntil(*client, Bytes("first"), ordered).size(), L"one alone arrives");
    Assert::AreEqual(std::uint64_t{0}, Settled(*server).backlogBytes, L"and is acknowledged");

    // Two too long to cross in the moment are both on their way when a third is sent.
    const Message older(2'000'000, std::byte{1});
    const Message newer(2'000'000, std::byte{2});
    server->SendNewest(older);
    server->SendNewest(newer);
    server->SendNewest(Bytes("dropped"));
    server->Send(Bytes("beside"));
    std::vector<Message> received = ReceiveNewestUntil(*client, newer, ordered);
    Assert::IsTrue(received.size() == 1 || (received.size() == 2 && received.front() == older), L"each newer than the one before it");
    Assert::AreEqual(std::uint64_t{0}, Settled(*server).backlogBytes, L"nothing sent was abandoned, and all of it arrived");

    server->SendNewest(Bytes("next"));
    for (Message& message : ReceiveNewestUntil(*client, Bytes("next"), ordered))
      received.push_back(std::move(message));
    Assert::IsTrue(std::ranges::find(received, Bytes("dropped")) == received.end(), L"the one sent while two were on their way is dropped");
    if (ordered.empty())
      ordered = ReceiveMessages(*client, 1);
    Assert::IsTrue(ordered == std::vector<Message>{Bytes("beside")}, L"the connection's stream carries its own beside them");
  }
};
} // namespace NeuronCoreTests

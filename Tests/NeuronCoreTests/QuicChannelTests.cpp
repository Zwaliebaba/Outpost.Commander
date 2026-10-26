#include "pch.h"

#include <algorithm>
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

  // ADR-060 decision 7: a sweep lets go of the channels whose connections have gone and that nothing else holds, shuts down
  // the ones that have said nothing for the time it is given, and keeps the rest.
  TEST_METHOD(SweepsAwayWhatHasGoneAndClosesWhatSaysNothing)
  {
    constexpr std::uint64_t SILENT_CODE = 42;
    Neuron::QuicListener listener({.applicationProtocol = std::string(PROTOCOL)},
                                  [](const std::shared_ptr<Neuron::QuicChannel>&) -> Neuron::QuicChannel::Receiver
                                  { return [](Neuron::QuicChannel& _channel, const Message& _message) { _channel.Send(_message); }; });
    const std::unique_ptr<Neuron::QuicChannel> talker = ConnectTo(listener);
    talker->Send(Bytes("here"));
    (void)ReceiveMessages(*talker, 1);
    const std::unique_ptr<Neuron::QuicChannel> silent = ConnectTo(listener);
    ConnectTo(listener)->Close();
    Assert::AreEqual(size_t{3}, listener.ChannelCount());

    // Swept as a server's tick sweeps it, until the closed connection has been let go and the silent one has gone too.
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
};
} // namespace NeuronCoreTests

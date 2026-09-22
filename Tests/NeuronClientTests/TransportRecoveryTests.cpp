#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
constexpr std::uint64_t INTERVAL = Neuron::TransportRecovery::REOPEN_INTERVAL_MILLISECONDS;
} // namespace

/// A failed socket is reopened rather than ending the client, and **every one of these runs without a
/// socket** -- the cadence is arithmetic and the socket is not.
TEST_CLASS(TheTransportRecovery)
{
public:
  /// Only a failure is recovered from. Opening is a connect in flight, and Closed is the caller's choice.
  TEST_METHOD(OnlyAFailedTransportIsReopened)
  {
    Neuron::TransportRecovery recovery;
    Assert::IsFalse(recovery.ShouldReopen(Neuron::TransportState::Closed, 0));
    Assert::IsFalse(recovery.ShouldReopen(Neuron::TransportState::Opening, 0));
    Assert::IsFalse(recovery.ShouldReopen(Neuron::TransportState::Ready, 0));
    Assert::AreEqual(0u, recovery.ReopenCount());
  }

  /// **THE FIRST FAILURE IS ANSWERED AT ONCE**, which is the resume: there is no reason to leave a player
  /// looking at the overlay for a second before trying.
  TEST_METHOD(TheFirstFailureIsReopenedImmediately)
  {
    Neuron::TransportRecovery recovery;
    Assert::IsTrue(recovery.ShouldReopen(Neuron::TransportState::Failed, 5000));
    Assert::AreEqual(1u, recovery.ReopenCount());
  }

  /// A failure that does not go away is retried once a second, not once a frame.
  TEST_METHOD(ARepeatedFailureWaitsTheInterval)
  {
    Neuron::TransportRecovery recovery;
    Assert::IsTrue(recovery.ShouldReopen(Neuron::TransportState::Failed, 5000));
    Assert::IsFalse(recovery.ShouldReopen(Neuron::TransportState::Failed, 5000));
    Assert::IsFalse(recovery.ShouldReopen(Neuron::TransportState::Failed, 5000 + INTERVAL - 1));
    Assert::IsTrue(recovery.ShouldReopen(Neuron::TransportState::Failed, 5000 + INTERVAL));
    Assert::AreEqual(2u, recovery.ReopenCount());
  }

  /// It never gives up. The common cause is a socket closed under a suspended client, and a later one
  /// is as recoverable as the first.
  TEST_METHOD(ItKeepsTryingForever)
  {
    Neuron::TransportRecovery recovery;
    std::uint64_t now = 0;
    for (std::uint32_t attempt = 0; attempt < 1000; ++attempt)
    {
      Assert::IsTrue(recovery.ShouldReopen(Neuron::TransportState::Failed, now));
      now += INTERVAL;
    }
    Assert::AreEqual(1000u, recovery.ReopenCount());
  }
};

} // namespace NeuronClientTests

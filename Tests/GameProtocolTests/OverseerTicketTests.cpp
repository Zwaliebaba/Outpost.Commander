#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
// An overseer's file of the shape WriteOverseerTicket writes, with _from replaced by _to.
std::string TicketText(std::string_view _from, std::string_view _to)
{
  std::string text = R"({ "host": "127.0.0.1", "port": 45001,
  "certificate": "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "token": "00112233445566778899aabbccddeeff" })";
  if (!_from.empty())
    text.replace(text.find(_from), _from.size(), _to);
  return text;
}

void ExpectRefused(std::string_view _from, std::string_view _to, const wchar_t* _why)
{
  const std::string text = TicketText(_from, _to);
  Assert::ExpectException<Neuron::Exception>([&text] { (void)Outpost::ReadOverseerTicket(text); }, _why);
}
} // namespace

// ADR-101: Overseer.json, what the overseer needs to reach a universe's server.
TEST_CLASS(OverseerTicketTests)
{
public:
  TEST_METHOD(ReadsBackAsItWasWritten)
  {
    Outpost::OverseerTicket ticket{.address = {.host = "a \"quoted\" host", .port = 47'002, .token = Outpost::NewSeatToken()}};
    for (std::size_t i = 0; i < ticket.address.certificate.size(); ++i)
      ticket.address.certificate[i] = static_cast<std::uint8_t>(255 - i);
    const Outpost::OverseerTicket read = Outpost::ReadOverseerTicket(Outpost::WriteOverseerTicket(ticket));
    Assert::AreEqual(ticket.address.host, read.address.host);
    Assert::AreEqual(ticket.address.port, read.address.port);
    Assert::IsTrue(ticket.address.certificate == read.address.certificate);
    Assert::IsTrue(ticket.address.token == read.address.token);
    Assert::AreEqual(std::uint16_t{45'001}, Outpost::ReadOverseerTicket(TicketText("", "")).address.port);
  }

  TEST_METHOD(RefusesWhatIsNotOne)
  {
    ExpectRefused("\"host\": \"127.0.0.1\"", "\"host\": \"\"", L"no host");
    ExpectRefused("45001", "0", L"no port");
    ExpectRefused("45001", "70000", L"a port out of range");
    ExpectRefused("00112233445566778899aabbccddeeff", "00112233445566778899aabbccddee", L"a token too short");
    ExpectRefused("0e0f10", "0e0fXX", L"a hash that is not hexadecimal");
    ExpectRefused("\"token\"", "\"player\": 1, \"token\"", L"a member it does not have");
    ExpectRefused(", \"token\": \"00112233445566778899aabbccddeeff\"", "", L"no token");
    Assert::ExpectException<Neuron::Exception>([] { (void)Outpost::ReadOverseerTicket("not JSON"); });
  }
};
} // namespace GameProtocolTests

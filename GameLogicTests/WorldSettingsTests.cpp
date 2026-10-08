#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
// A settings file of the shape WriteWorldSettings writes, with _change made to its text.
std::string SettingsText(std::string_view _from, std::string_view _to)
{
  std::string text = R"({ "address": "0.0.0.0", "port": 45000, "host": "friends.example.net", "seed": "18446744073709551615",
  "seats": [ { "player": 1, "token": "00112233445566778899aabbccddeeff" }, { "player": 2, "token": "ffeeddccbbaa99887766554433221100" } ] })";
  if (!_from.empty())
    text.replace(text.find(_from), _from.size(), _to);
  return text;
}

void ExpectRefused(std::string_view _from, std::string_view _to, const wchar_t* _why)
{
  const std::string text = SettingsText(_from, _to);
  Assert::ExpectException<Neuron::Exception>([&text] { (void)Outpost::ReadWorldSettings(text); }, _why);
}
} // namespace

// Phase 5 design §4, ADR-078: a world's settings, which its owner makes once, and the join file each seat's player is
// handed.
TEST_CLASS(WorldSettingsTests)
{
public:
  // New settings have a seat for each player, each with a token of its own, and read back as they were written; a seed
  // too large for a JSON number survives as a string.
  TEST_METHOD(ReadBackAsTheyWereWritten)
  {
    Outpost::WorldSettings settings = Outpost::NewWorldSettings(0xFFFF'FFFF'FFFF'FFFFull, 3);
    settings.host = "a \"quoted\" host\\name";
    settings.port = 47001;
    // The third an AI empire's (ADR-079).
    settings.seats[2].ai = "Hard";
    settings.seats[2].token = {};
    Assert::AreEqual(size_t{3}, settings.seats.size());
    Assert::AreEqual(std::uint32_t{1}, settings.seats[0].player.value);
    Assert::AreEqual(std::uint32_t{2}, settings.seats[1].player.value);
    Assert::IsFalse(settings.seats[0].token == settings.seats[1].token, L"each seat its own token");
    Assert::IsFalse(settings.seats[0].token == Outpost::SeatToken{}, L"and not zero");

    const Outpost::WorldSettings read = Outpost::ReadWorldSettings(Outpost::WriteWorldSettings(settings));
    Assert::AreEqual(settings.address, read.address);
    Assert::AreEqual(settings.port, read.port);
    Assert::AreEqual(settings.host, read.host);
    Assert::AreEqual(settings.seed, read.seed);
    Assert::AreEqual(settings.seats.size(), read.seats.size());
    for (std::size_t i = 0; i < settings.seats.size(); ++i)
    {
      Assert::IsTrue(settings.seats[i].player == read.seats[i].player);
      Assert::IsTrue(settings.seats[i].token == read.seats[i].token);
      Assert::AreEqual(settings.seats[i].ai, read.seats[i].ai);
    }
    Assert::AreEqual(std::uint64_t{0xFFFF'FFFF'FFFF'FFFFull}, Outpost::ReadWorldSettings(SettingsText("", "")).seed);
  }

  // Settings that are not settings are refused, saying what is wrong, rather than running a world with them.
  TEST_METHOD(RefusesWhatIsNotSettings)
  {
    ExpectRefused("\"port\": 45000", "\"port\": 70000", L"a port out of range");
    ExpectRefused("\"port\": 45000", "\"port\": 0", L"no port");
    ExpectRefused("\"seed\": \"18446744073709551615\"", "\"seed\": 7", L"a seed that is a number, not a string");
    ExpectRefused("\"seed\": \"18446744073709551615\"", "\"seed\": \"18446744073709551616\"", L"a seed past 64 bits");
    ExpectRefused("\"host\": \"friends.example.net\"", "\"host\": \"\"", L"no host");
    ExpectRefused("00112233445566778899aabbccddeeff", "00112233445566778899aabbccddee", L"a token too short");
    ExpectRefused("00112233445566778899aabbccddeeff", "00112233445566778899aabbccddeeXX", L"a token that is not hexadecimal");
    ExpectRefused("{ \"player\": 2", "{ \"player\": 1", L"two seats for one player");
    ExpectRefused("\"token\": \"ffeeddccbbaa99887766554433221100\"", "\"ai\": \"Brutal\"", L"a difficulty there is none of");
    ExpectRefused("\"token\": \"ffeeddccbbaa99887766554433221100\"", "\"ai\": \"Hard\", \"token\": \"ffeeddccbbaa99887766554433221100\"",
                  L"a seat both a player's and an AI's");
    ExpectRefused(", \"token\": \"ffeeddccbbaa99887766554433221100\"", "", L"a seat neither");
    Assert::AreEqual(
      std::string("Normal"),
      Outpost::ReadWorldSettings(SettingsText("\"token\": \"ffeeddccbbaa99887766554433221100\"", "\"ai\": \"Normal\"")).seats[1].ai,
      L"an AI empire's seat");
    ExpectRefused("\"host\"", "\"hots\"", L"a member misspelled");
    ExpectRefused(
      R"([ { "player": 1, "token": "00112233445566778899aabbccddeeff" }, { "player": 2, "token": "ffeeddccbbaa99887766554433221100" } ])",
      "[]", L"no seat");
  }

  // A seat's join file names the settings' host and port, the server's certificate, the seat's player and its token, and
  // reads back as it was written.
  TEST_METHOD(WritesJoinFilesThatReadBack)
  {
    const Outpost::WorldSettings settings = Outpost::ReadWorldSettings(SettingsText("", ""));
    Neuron::CertificateHash certificate{};
    for (std::size_t i = 0; i < certificate.size(); ++i)
      certificate[i] = static_cast<std::uint8_t>(i * 7);
    const Outpost::JoinTicket ticket = Outpost::TicketFor(settings, settings.seats[1], certificate);
    const Outpost::JoinTicket read = Outpost::ReadJoinTicket(Outpost::WriteJoinTicket(ticket));
    Assert::AreEqual(std::string("friends.example.net"), read.address.host);
    Assert::AreEqual(std::uint16_t{45000}, read.address.port);
    Assert::IsTrue(read.address.certificate == certificate);
    Assert::IsTrue(read.address.token == settings.seats[1].token);
    Assert::IsTrue(read.player == Outpost::PlayerId{2});
    Assert::AreEqual(std::string("Join-Player2.json"), Outpost::JoinFileName(read.player));

    Assert::ExpectException<Neuron::Exception>([] { (void)Outpost::ReadJoinTicket("{}"); }, L"an empty join file");
    std::string shortCertificate = Outpost::WriteJoinTicket(ticket);
    shortCertificate.replace(shortCertificate.find(Outpost::ToHex(certificate)), 2, "");
    Assert::ExpectException<Neuron::Exception>([&shortCertificate] { (void)Outpost::ReadJoinTicket(shortCertificate); },
                                               L"a hash too short");
  }

  // A token and a hash cross a file as hexadecimal, two digits a byte, either case read.
  TEST_METHOD(WritesBytesAsHexadecimal)
  {
    const std::array<std::uint8_t, 3> bytes{0x00, 0xAB, 0x7F};
    Assert::AreEqual(std::string("00ab7f"), Outpost::ToHex(bytes));
    std::array<std::uint8_t, 3> read{};
    Assert::IsTrue(Outpost::FromHex("00AB7f", read));
    Assert::IsTrue(read == bytes);
    Assert::IsFalse(Outpost::FromHex("00ab7", read), L"an odd digit");
    Assert::IsFalse(Outpost::FromHex("00ab7g", read), L"not a digit");
  }
};
} // namespace GameLogicTests

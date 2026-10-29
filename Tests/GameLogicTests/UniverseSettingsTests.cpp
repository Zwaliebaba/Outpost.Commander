#include "pch.h"
#include "UniverseMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
// A settings file of the shape WriteUniverseSettings writes, with _change made to its text.
std::string SettingsText(std::string_view _from, std::string_view _to)
{
  std::string text = R"({ "address": "0.0.0.0", "port": 45000, "host": "friends.example.net", "seed": "18446744073709551615",
  "seats": [ { "player": 1, "token": "00112233445566778899aabbccddeeff" }, { "player": 2, "token": "ffeeddccbbaa99887766554433221100" } ] })";
  if (!_from.empty())
    text.replace(text.find(_from), _from.size(), _to);
  return text;
}

std::string ReadFile(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

void ExpectRefused(std::string_view _from, std::string_view _to, const wchar_t* _why)
{
  const std::string text = SettingsText(_from, _to);
  Assert::ExpectException<Neuron::Exception>([&text] { (void)Outpost::ReadUniverseSettings(text); }, _why);
}
} // namespace

// ADR-078 decisions 2 and 8: a universe's settings, which its owner makes once, and the join file each seat's player is
// handed.
TEST_CLASS(UniverseSettingsTests)
{
public:
  // New settings have a seat for each player, each with a token of its own, and read back as they were written; a seed
  // too large for a JSON number survives as a string.
  TEST_METHOD(ReadBackAsTheyWereWritten)
  {
    Outpost::UniverseSettings settings = Outpost::NewUniverseSettings(0xFFFF'FFFF'FFFF'FFFFull, 3);
    settings.host = "a \"quoted\" host\\name";
    settings.port = 47001;
    settings.overseerAddress = "::";
    settings.overseerPort = 47002;
    // The third an AI empire's (ADR-079).
    settings.seats[2].ai = "Hard";
    settings.seats[2].token = {};
    Assert::AreEqual(size_t{3}, settings.seats.size());
    Assert::AreEqual(std::uint32_t{1}, settings.seats[0].player.value);
    Assert::AreEqual(std::uint32_t{2}, settings.seats[1].player.value);
    Assert::IsFalse(settings.seats[0].token == settings.seats[1].token, L"each seat its own token");
    Assert::IsFalse(settings.seats[0].token == Outpost::SeatToken{}, L"and not zero");

    const Outpost::UniverseSettings read = Outpost::ReadUniverseSettings(Outpost::WriteUniverseSettings(settings));
    Assert::AreEqual(settings.address, read.address);
    Assert::AreEqual(settings.port, read.port);
    Assert::AreEqual(settings.overseerAddress, read.overseerAddress);
    Assert::AreEqual(settings.overseerPort, read.overseerPort);
    Assert::AreEqual(settings.host, read.host);
    Assert::AreEqual(settings.seed, read.seed);
    Assert::AreEqual(settings.seats.size(), read.seats.size());
    for (std::size_t i = 0; i < settings.seats.size(); ++i)
    {
      Assert::IsTrue(settings.seats[i].player == read.seats[i].player);
      Assert::IsTrue(settings.seats[i].token == read.seats[i].token);
      Assert::AreEqual(settings.seats[i].ai, read.seats[i].ai);
    }
    Assert::AreEqual(std::uint64_t{0xFFFF'FFFF'FFFF'FFFFull}, Outpost::ReadUniverseSettings(SettingsText("", "")).seed);
  }

  // Settings that are not settings are refused, saying what is wrong, rather than running a universe with them.
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
      Outpost::ReadUniverseSettings(SettingsText("\"token\": \"ffeeddccbbaa99887766554433221100\"", "\"ai\": \"Normal\"")).seats[1].ai,
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
    const Outpost::UniverseSettings settings = Outpost::ReadUniverseSettings(SettingsText("", ""));
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

  // ADR-104: a seat's new token written back into Universe.json, renamed over it, and into the seat's join file, the rest of
  // the settings as they were; an AI empire's seat, a player with none, and a folder without settings have no token to write.
  TEST_METHOD(WritesASeatsTokenBack)
  {
    const TemporaryFolder folder;
    Outpost::UniverseSettings settings = Outpost::NewUniverseSettings(7, 2);
    settings.seats[1] = {.player = Outpost::PlayerId{2}, .ai = "Normal"};
    {
      std::ofstream file(folder.Path() / Outpost::UNIVERSE_SETTINGS_FILE, std::ios::binary);
      file << Outpost::WriteUniverseSettings(settings);
    }
    Neuron::CertificateHash certificate{};
    certificate[0] = 0x42;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Assert::IsFalse(token == settings.seats[0].token);
    const std::filesystem::path joinFile = Outpost::WriteSeatToken(folder.Path(), Outpost::PlayerId{1}, token, certificate);
    Assert::IsTrue(joinFile == folder.Path() / Outpost::JoinFileName(Outpost::PlayerId{1}));

    const Outpost::UniverseSettings written = Outpost::ReadUniverseSettings(ReadFile(folder.Path() / Outpost::UNIVERSE_SETTINGS_FILE));
    Assert::IsTrue(written.seats.at(0).token == token, L"the seat's new token");
    Assert::AreEqual(std::string("Normal"), written.seats.at(1).ai, L"and the rest as it was");
    Assert::AreEqual(std::uint64_t{7}, written.seed);
    const Outpost::JoinTicket ticket = Outpost::ReadJoinTicket(ReadFile(joinFile));
    Assert::IsTrue(ticket.address.token == token && ticket.address.certificate == certificate && ticket.player == Outpost::PlayerId{1},
                   L"the seat's join file");
    Assert::IsTrue(std::ranges::none_of(std::filesystem::directory_iterator(folder.Path()),
                                        [](const std::filesystem::directory_entry& _entry)
                                        { return _entry.path().extension() == ".partial"; }),
                   L"nothing left half written");

    Assert::ExpectException<Neuron::Exception>(
      [&] { (void)Outpost::WriteSeatToken(folder.Path(), Outpost::PlayerId{2}, token, certificate); }, L"an AI empire's seat");
    Assert::ExpectException<Neuron::Exception>(
      [&] { (void)Outpost::WriteSeatToken(folder.Path(), Outpost::PlayerId{3}, token, certificate); }, L"a player with no seat");
    const TemporaryFolder empty;
    Assert::ExpectException<Neuron::Exception>(
      [&] { (void)Outpost::WriteSeatToken(empty.Path(), Outpost::PlayerId{1}, token, certificate); }, L"no settings");
  }

  // Settings made before the overseer read as its defaults, the loopback and the port after the game's, and settings that
  // name its address and port read as they name them (ADR-101).
  TEST_METHOD(ReadsWhereTheOverseerIsListenedFor)
  {
    const Outpost::UniverseSettings older = Outpost::ReadUniverseSettings(SettingsText("", ""));
    Assert::AreEqual(std::string("127.0.0.1"), older.overseerAddress);
    Assert::AreEqual(std::uint16_t{45'001}, older.overseerPort);
    const Outpost::UniverseSettings named = Outpost::ReadUniverseSettings(
      SettingsText("\"port\": 45000", "\"port\": 45000, \"overseerAddress\": \"*\", \"overseerPort\": 46000"));
    Assert::AreEqual(std::string("*"), named.overseerAddress);
    Assert::AreEqual(std::uint16_t{46'000}, named.overseerPort);
    ExpectRefused("\"port\": 45000", "\"port\": 45000, \"overseerPort\": 0", L"no overseer's port");
    ExpectRefused("\"port\": 45000", "\"port\": 45000, \"overseerPort\": 70000", L"an overseer's port out of range");
    ExpectRefused("\"port\": 45000", "\"port\": 45000, \"overseerAddress\": 7", L"an overseer's address that is no string");
  }

  // The overseer's file names the address the server listens for it on, or its host for players when it listens on every
  // address, the overseer's port, the certificate and the token, and reads back as it was written (ADR-101).
  TEST_METHOD(WritesTheOverseersFile)
  {
    Outpost::UniverseSettings settings = Outpost::ReadUniverseSettings(SettingsText("", ""));
    Neuron::CertificateHash certificate{};
    for (std::size_t i = 0; i < certificate.size(); ++i)
      certificate[i] = static_cast<std::uint8_t>(i * 5);
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    const Outpost::OverseerTicket ticket =
      Outpost::ReadOverseerTicket(Outpost::WriteOverseerTicket(Outpost::OverseerTicketFor(settings, certificate, token)));
    Assert::AreEqual(std::string("127.0.0.1"), ticket.address.host);
    Assert::AreEqual(std::uint16_t{45'001}, ticket.address.port);
    Assert::IsTrue(ticket.address.certificate == certificate);
    Assert::IsTrue(ticket.address.token == token);
    for (const std::string_view every : {"*", "0.0.0.0", "::"})
    {
      settings.overseerAddress = std::string(every);
      Assert::AreEqual(std::string("friends.example.net"), Outpost::OverseerTicketFor(settings, certificate, token).address.host);
    }
    settings.overseerAddress = "192.168.1.20";
    Assert::AreEqual(std::string("192.168.1.20"), Outpost::OverseerTicketFor(settings, certificate, token).address.host);
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

#include "pch.h"

#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

/// ADR-013's token file, and the same split `HostAddress` takes: the parsing and the formatting are
/// pure and tested, the half that reaches `LocalState` needs a packaged application and no suite in
/// this tree is one.
TEST_CLASS(TheSessionTokenFile)
{
public:
  TEST_METHOD(SixteenDigitsRoundTrip)
  {
    for (const std::uint64_t token :
         {std::uint64_t{1}, std::uint64_t{0x0123456789ABCDEFull}, ~std::uint64_t{0}, std::uint64_t{0x8040201008040201ull}})
    {
      const std::string text = Neuron::SessionTokenToFileContents(token);
      Assert::AreEqual(static_cast<std::size_t>(16), text.size());
      Assert::AreEqual(token, Neuron::SessionTokenFromFileContents(text));
    }
  }

  /// Lower case, no prefix, no newline -- the exact shape the reader demands, because a formatting
  /// flag left set somewhere else is not a thing to discover on a device.
  TEST_METHOD(TheShapeIsExact)
  {
    Assert::AreEqual(std::string{"0000000000000001"}, Neuron::SessionTokenToFileContents(1));
    Assert::AreEqual(std::string{"ffffffffffffffff"}, Neuron::SessionTokenToFileContents(~std::uint64_t{0}));
  }

  /// A file a person has looked at has a newline, and possibly a space.
  TEST_METHOD(TheFirstLineIsTakenAndTrimmed)
  {
    Assert::AreEqual(std::uint64_t{0x00000000000000FFull}, Neuron::SessionTokenFromFileContents("  00000000000000ff  \r\nnote\n"));
  }

  /// Upper case is accepted although it is never written -- a file edited by hand is the only way
  /// to get one, and refusing it would be a client that rejoins as somebody new for a reason
  /// nobody can see.
  TEST_METHOD(UpperCaseIsAccepted)
  {
    Assert::AreEqual(std::uint64_t{0xABCDEF0123456789ull}, Neuron::SessionTokenFromFileContents("ABCDEF0123456789"));
  }

  /// **EVERY WAY OF BEING WRONG IS "I HAVE NO TOKEN"**, which is a state the protocol already
  /// handles on a first run. There is no failure here worth more than rejoining as new.
  TEST_METHOD(AnythingElseIsNoToken)
  {
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents(""));
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("   \r\n"));
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("0123456789abcde"), L"fifteen digits");
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("0123456789abcdef0"), L"seventeen");
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("0x23456789abcdef"), L"a prefix is not a digit");
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("012345678 abcdef"));
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents("gggggggggggggggg"));
  }

  /// **A TRUNCATED FILE IS REFUSED BY ITS LENGTH BEFORE A DIGIT IS LOOKED AT.** A half-written file
  /// would otherwise parse into a small number that names nobody's session; the client would be
  /// seated as new either way, and this is the version that is obvious.
  TEST_METHOD(EveryTruncationIsRefused)
  {
    const std::string whole = Neuron::SessionTokenToFileContents(0x1122334455667788ull);
    for (std::size_t length = 0; length < whole.size(); ++length)
    {
      Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents(std::string_view{whole}.substr(0, length)));
    }
    Assert::AreEqual(0x1122334455667788ull, Neuron::SessionTokenFromFileContents(whole));
  }

  /// A byte above 127 in the file is a signed `char` gone negative, which is the trap that made
  /// this parser not use `std::isxdigit`.
  TEST_METHOD(AHighByteIsNotADigit)
  {
    std::string text(16, '0');
    text[3] = static_cast<char>(0xE9);
    Assert::AreEqual(Neuron::NO_TOKEN, Neuron::SessionTokenFromFileContents(text));
  }
};

/// Two clients on one machine share `LocalState`, so each names its files by the slot it claimed --
/// and a lone client, slot zero, must find every file exactly where it always was.
TEST_CLASS(TheInstanceFileName)
{
public:
  TEST_METHOD(SlotZeroIsTheNameUnchanged)
  {
    Assert::AreEqual(std::wstring{L"session.txt"}, Neuron::InstanceFileName(Neuron::SESSION_TOKEN_FILE_NAME, 0));
    Assert::AreEqual(std::wstring{L"probe-log.txt"}, Neuron::InstanceFileName(L"probe-log.txt", 0));
  }

  TEST_METHOD(AnotherSlotGoesBeforeTheExtension)
  {
    Assert::AreEqual(std::wstring{L"session-1.txt"}, Neuron::InstanceFileName(L"session.txt", 1));
    Assert::AreEqual(std::wstring{L"probe-log-3.txt"}, Neuron::InstanceFileName(L"probe-log.txt", 3));
    Assert::AreEqual(std::wstring{L"archive.tar-2.gz"}, Neuron::InstanceFileName(L"archive.tar.gz", 2));
  }

  /// No extension, or a name that is only one, takes the suffix at the end rather than splitting.
  TEST_METHOD(ANameWithNoExtensionTakesItAtTheEnd)
  {
    Assert::AreEqual(std::wstring{L"session-1"}, Neuron::InstanceFileName(L"session", 1));
    Assert::AreEqual(std::wstring{L".hidden-1"}, Neuron::InstanceFileName(L".hidden", 1));
  }

  TEST_METHOD(EverySlotNamesADifferentFile)
  {
    for (std::uint32_t first = 0; first < Neuron::INSTANCE_SLOT_COUNT; ++first)
    {
      for (std::uint32_t second = first + 1; second < Neuron::INSTANCE_SLOT_COUNT; ++second)
      {
        Assert::AreNotEqual(Neuron::InstanceFileName(L"session.txt", first), Neuron::InstanceFileName(L"session.txt", second));
      }
    }
  }
};

} // namespace NeuronClientTests

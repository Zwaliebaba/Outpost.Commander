#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr UINT TEXEL_BYTES = 4;

void PutUInt32(Neuron::ByteBuffer& _bytes, size_t _offset, std::uint32_t _value)
{
  std::memcpy(_bytes.data() + _offset, &_value, sizeof(_value));
}

// A .dds file with the legacy header, 8-bit BGRA texels with alpha, and one level of _texels.
Neuron::ByteBuffer MakeDds(std::uint32_t _width, std::uint32_t _height, const Neuron::ByteBuffer& _texels)
{
  Neuron::ByteBuffer bytes(128);
  std::memcpy(bytes.data(), "DDS ", 4);
  PutUInt32(bytes, 4, 124);
  PutUInt32(bytes, 8, 0x100F);
  PutUInt32(bytes, 12, _height);
  PutUInt32(bytes, 16, _width);
  PutUInt32(bytes, 76, 32);
  PutUInt32(bytes, 80, 0x41);
  PutUInt32(bytes, 88, 32);
  PutUInt32(bytes, 92, 0x00FF'0000);
  PutUInt32(bytes, 96, 0x0000'FF00);
  PutUInt32(bytes, 100, 0x0000'00FF);
  PutUInt32(bytes, 104, 0xFF00'0000);
  PutUInt32(bytes, 108, 0x1000);
  bytes.insert(bytes.end(), _texels.begin(), _texels.end());
  return bytes;
}

std::uint8_t Alpha(const Neuron::TextureData& _texture, UINT _x, UINT _y)
{
  return _texture.levels.front()[(((size_t{_y} * _texture.width) + _x) * TEXEL_BYTES) + 3];
}

void ExpectRejected(const Neuron::ByteBuffer& _bytes)
{
  Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::ParseDds(_bytes, "test.dds"); });
}

// What ParseDds says is wrong with _bytes, after the file's name.
std::string ReasonRejected(const Neuron::ByteBuffer& _bytes)
{
  constexpr std::string_view PREFIX = "The texture test.dds cannot be read: ";
  std::string message;
  try
  {
    (void)Neuron::ParseDds(_bytes, "test.dds");
  }
  catch (const Neuron::Exception& error)
  {
    message = error.what();
  }
  Assert::IsTrue(message.starts_with(PREFIX), std::wstring(message.begin(), message.end()).c_str());
  return message.substr(PREFIX.size());
}

// A .dds file with the DX10 header after the legacy one, and one level of _texels; a single 2D texture unless the
// dimension, the flags or the array size say otherwise.
Neuron::ByteBuffer MakeDx10Dds(std::uint32_t _width, std::uint32_t _height, DXGI_FORMAT _format, const Neuron::ByteBuffer& _texels,
                               std::uint32_t _dimension = 3, std::uint32_t _miscFlags = 0, std::uint32_t _arraySize = 1)
{
  Neuron::ByteBuffer bytes = MakeDds(_width, _height, {});
  PutUInt32(bytes, 80, 0x4);
  std::memcpy(bytes.data() + 84, "DX10", 4);
  Neuron::ByteBuffer header(20);
  PutUInt32(header, 0, static_cast<std::uint32_t>(_format));
  PutUInt32(header, 4, _dimension);
  PutUInt32(header, 8, _miscFlags);
  PutUInt32(header, 12, _arraySize);
  bytes.insert(bytes.end(), header.begin(), header.end());
  bytes.insert(bytes.end(), _texels.begin(), _texels.end());
  return bytes;
}
} // namespace

TEST_CLASS(TextureDataTests)
{
public:
  // The sky's sprite: white, with its shape in its alpha, bright at the center and empty at the corners (ADR-022).
  TEST_METHOD(ReadsTheStarburst)
  {
    const Neuron::TextureData texture = Neuron::ParseDds(ReadRepositoryAsset("Textures\\starburst.dds"), "starburst.dds");
    Assert::AreEqual(128u, texture.width);
    Assert::AreEqual(128u, texture.height);
    Assert::IsTrue(texture.format == DXGI_FORMAT_B8G8R8A8_UNORM);
    Assert::AreEqual(size_t{1}, texture.levels.size());
    Assert::AreEqual(size_t{128} * 128 * TEXEL_BYTES, texture.levels.front().size());
    Assert::IsTrue(Alpha(texture, 64, 64) > 200);
    Assert::IsTrue(Alpha(texture, 0, 0) == 0);
  }

  TEST_METHOD(BuildsAFullMipChain)
  {
    Neuron::TextureData texture = Neuron::ParseDds(ReadRepositoryAsset("Textures\\starburst.dds"), "starburst.dds");
    Neuron::BuildMipLevels(texture);
    Assert::AreEqual(size_t{8}, texture.levels.size());
    UINT side = 128;
    for (const Neuron::ByteBuffer& level : texture.levels)
    {
      Assert::AreEqual(size_t{side} * side * TEXEL_BYTES, level.size());
      side = std::max(1u, side / 2);
    }
  }

  // Each texel of a level is the rounded average of the 2 by 2 above it, channel by channel; a side of 1 stays 1.
  TEST_METHOD(AveragesTwoByTwo)
  {
    const Neuron::ByteBuffer texels{0, 10, 100, 255, 2, 10, 100, 0, 4, 10, 101, 255, 5, 10, 101, 0};
    Neuron::TextureData wide = Neuron::ParseDds(MakeDds(4, 1, texels), "test.dds");
    Neuron::BuildMipLevels(wide);
    Assert::AreEqual(size_t{3}, wide.levels.size());
    const Neuron::ByteBuffer expectedHalf{1, 10, 100, 128, 5, 10, 101, 128};
    Assert::IsTrue(wide.levels[1] == expectedHalf);
    const Neuron::ByteBuffer expectedLast{3, 10, 101, 128};
    Assert::IsTrue(wide.levels[2] == expectedLast);
  }

  TEST_METHOD(RejectsWhatItCannotRead)
  {
    const Neuron::ByteBuffer good = MakeDds(1, 1, {1, 2, 3, 4});
    (void)Neuron::ParseDds(good, "test.dds");

    Neuron::ByteBuffer notDds = good;
    notDds[0] = 'X';
    ExpectRejected(notDds);

    ExpectRejected(Neuron::ByteBuffer(good.begin(), good.end() - 1));

    Neuron::ByteBuffer leftOver = good;
    leftOver.push_back(0);
    ExpectRejected(leftOver);

    // DXT1, a compressed format.
    Neuron::ByteBuffer compressed = good;
    PutUInt32(compressed, 80, 0x4);
    std::memcpy(compressed.data() + 84, "DXT1", 4);
    ExpectRejected(compressed);

    Neuron::ByteBuffer cubeMap = good;
    PutUInt32(cubeMap, 112, 0xFE00);
    ExpectRejected(cubeMap);
  }

  // Each refusal names the file and says what is wrong, so that a broken asset can be found from the message alone.
  TEST_METHOD(SaysWhyItCannotRead)
  {
    const Neuron::ByteBuffer good = MakeDds(1, 1, {1, 2, 3, 4});
    Neuron::ByteBuffer notDds = good;
    notDds[0] = 'X';
    Assert::AreEqual(std::string("it is not a .dds file."), ReasonRejected(notDds));
    Assert::AreEqual(std::string("it is not a .dds file."), ReasonRejected(Neuron::ByteBuffer(good.begin(), good.begin() + 127)));
    Assert::AreEqual(std::string("the file ends early."), ReasonRejected(Neuron::ByteBuffer(good.begin(), good.end() - 1)));
    Neuron::ByteBuffer leftOver = good;
    leftOver.push_back(0);
    Assert::AreEqual(std::string("it has bytes left over."), ReasonRejected(leftOver));
    Neuron::ByteBuffer volume = good;
    PutUInt32(volume, 112, 0x200000);
    Assert::AreEqual(std::string("it is a cube map or a volume, not a 2D texture."), ReasonRejected(volume));
    Assert::AreEqual(std::string("it is empty."), ReasonRejected(MakeDds(0, 4, {})));
    Assert::AreEqual(std::string("it is empty."), ReasonRejected(MakeDds(4, 0, {})));
  }

  // The legacy header's masks say where each channel sits: red in the low byte is RGBA, red in the third is BGRA.
  TEST_METHOD(ReadsLegacyRgbaTexels)
  {
    Neuron::ByteBuffer rgba = MakeDds(1, 1, {1, 2, 3, 4});
    PutUInt32(rgba, 92, 0x0000'00FF);
    PutUInt32(rgba, 100, 0x00FF'0000);
    const Neuron::TextureData texture = Neuron::ParseDds(rgba, "test.dds");
    Assert::IsTrue(texture.format == DXGI_FORMAT_R8G8B8A8_UNORM);
    Assert::IsTrue(texture.levels.front() == Neuron::ByteBuffer{1, 2, 3, 4}, L"texels are kept as they are");
  }

  // Only 32-bit texels with alpha, laid out as BGRA or RGBA, are read from the legacy header.
  TEST_METHOD(RefusesLegacyTexelsOfAnotherLayout)
  {
    const Neuron::ByteBuffer good = MakeDds(1, 1, {1, 2, 3, 4});
    const auto changed = [&](size_t _offset, std::uint32_t _value)
    {
      Neuron::ByteBuffer bytes = good;
      PutUInt32(bytes, _offset, _value);
      return bytes;
    };
    const std::string unsupported("its texels are not 8-bit BGRA or RGBA.");
    // No alpha; no RGB; 24 bits a texel; the alpha or the green somewhere else; red where green belongs.
    for (const Neuron::ByteBuffer& bytes : {changed(80, 0x40), changed(80, 0x1), changed(88, 24), changed(104, 0x0000'00FF),
                                            changed(96, 0x00FF'0000), changed(92, 0x0000'FF00)})
      Assert::AreEqual(unsupported, ReasonRejected(bytes));
  }

  // The DX10 header names the format itself, sRGB included, and the texels follow it.
  TEST_METHOD(ReadsTheDx10Header)
  {
    const Neuron::ByteBuffer texels{1, 2, 3, 4, 5, 6, 7, 8};
    for (const DXGI_FORMAT format :
         {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB})
    {
      const Neuron::TextureData texture = Neuron::ParseDds(MakeDx10Dds(2, 1, format, texels), "test.dds");
      Assert::IsTrue(texture.format == format);
      Assert::AreEqual(2u, texture.width);
      Assert::AreEqual(1u, texture.height);
      Assert::AreEqual(size_t{1}, texture.levels.size());
      Assert::IsTrue(texture.levels.front() == texels);
    }
  }

  // Through the DX10 header too, only a single 2D texture of 8-bit BGRA or RGBA texels is read.
  TEST_METHOD(RefusesADx10TextureOfAnotherKind)
  {
    const Neuron::ByteBuffer texels{1, 2, 3, 4, 5, 6, 7, 8};
    const std::string notSingle2d("it is not a single 2D texture.");
    // A 1D texture, a volume, a cube map, and an array of two.
    Assert::AreEqual(notSingle2d, ReasonRejected(MakeDx10Dds(2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texels, 2)));
    Assert::AreEqual(notSingle2d, ReasonRejected(MakeDx10Dds(2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texels, 4)));
    Assert::AreEqual(notSingle2d, ReasonRejected(MakeDx10Dds(2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texels, 3, 0x4)));
    Assert::AreEqual(notSingle2d, ReasonRejected(MakeDx10Dds(2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texels, 3, 0, 2)));
    // Compressed, and 16-bit floats.
    Assert::AreEqual(std::string("its texels are not 8-bit BGRA or RGBA."),
                     ReasonRejected(MakeDx10Dds(4, 4, DXGI_FORMAT_BC1_UNORM, Neuron::ByteBuffer(8))));
    Assert::AreEqual(std::string("its texels are not 8-bit BGRA or RGBA."),
                     ReasonRejected(MakeDx10Dds(1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, Neuron::ByteBuffer(8))));
    // The DX10 header itself cut short.
    const Neuron::ByteBuffer whole = MakeDx10Dds(2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, texels);
    Assert::AreEqual(std::string("the file ends early."), ReasonRejected(Neuron::ByteBuffer(whole.begin(), whole.begin() + 128 + 19)));
  }

  // With the mip count flag, the file's own levels are read, each half the one before down to 1; without it, only the
  // largest, whatever the count says.
  TEST_METHOD(ReadsTheMipLevelsInTheFile)
  {
    // 4 by 2, then 2 by 1, then 1 by 1.
    Neuron::ByteBuffer texels(32 + 8 + 4);
    for (size_t i = 0; i < texels.size(); ++i)
      texels[i] = static_cast<std::uint8_t>(i);
    Neuron::ByteBuffer withLevels = MakeDds(4, 2, texels);
    PutUInt32(withLevels, 8, 0x100F | 0x20000);
    PutUInt32(withLevels, 28, 3);
    const Neuron::TextureData texture = Neuron::ParseDds(withLevels, "test.dds");
    Assert::AreEqual(size_t{3}, texture.levels.size());
    Assert::IsTrue(texture.levels[0] == Neuron::ByteBuffer(texels.begin(), texels.begin() + 32));
    Assert::IsTrue(texture.levels[1] == Neuron::ByteBuffer(texels.begin() + 32, texels.begin() + 40));
    Assert::IsTrue(texture.levels[2] == Neuron::ByteBuffer(texels.begin() + 40, texels.end()));

    // A count of 0 is one level.
    Neuron::ByteBuffer zeroCount = MakeDds(4, 2, Neuron::ByteBuffer(texels.begin(), texels.begin() + 32));
    PutUInt32(zeroCount, 8, 0x100F | 0x20000);
    PutUInt32(zeroCount, 28, 0);
    Assert::AreEqual(size_t{1}, Neuron::ParseDds(zeroCount, "test.dds").levels.size());

    // Without the flag the count is not read.
    Neuron::ByteBuffer noFlag = MakeDds(4, 2, Neuron::ByteBuffer(texels.begin(), texels.begin() + 32));
    PutUInt32(noFlag, 28, 3);
    Assert::AreEqual(size_t{1}, Neuron::ParseDds(noFlag, "test.dds").levels.size());

    // A level the count promises and the file does not hold.
    PutUInt32(withLevels, 28, 4);
    Assert::AreEqual(std::string("the file ends early."), ReasonRejected(withLevels));
  }
};
} // namespace GameAppTests

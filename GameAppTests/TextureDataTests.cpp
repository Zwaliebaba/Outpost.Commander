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
    Assert::AreEqual(size_t{128 * 128 * TEXEL_BYTES}, texture.levels.front().size());
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
};
} // namespace GameAppTests

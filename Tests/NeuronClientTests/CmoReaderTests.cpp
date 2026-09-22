#include "pch.h"

#include <cstring>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
/// A CMO written by hand, so the suite can build the files this content does not contain -- one with
/// skinning, bones and animation clips above all, which is what M1.9 says the reader must survive.
///
/// **IT IS A SECOND IMPLEMENTATION OF THE FORMAT AND THAT IS THE POINT.** A test that round-tripped
/// through the reader's own writer would agree with itself about a layout that was wrong.
class CmoBuilder
{
public:
  void UInt32(std::uint32_t _value)
  {
    Append(&_value, sizeof(_value));
  }

  void UInt16(std::uint16_t _value)
  {
    Append(&_value, sizeof(_value));
  }

  void UInt8(std::uint8_t _value)
  {
    Append(&_value, sizeof(_value));
  }

  void Float(float _value)
  {
    Append(&_value, sizeof(_value));
  }

  void Floats(std::size_t _count, float _value = 0.0f)
  {
    for (std::size_t index = 0; index < _count; ++index)
    {
      Float(_value);
    }
  }

  /// A UINT count of UTF-16 code units followed by that many, the terminating null included.
  void WideString(const char* _ascii)
  {
    const std::size_t length = std::strlen(_ascii);
    UInt32(static_cast<std::uint32_t>(length + 1));
    for (std::size_t index = 0; index < length; ++index)
    {
      UInt16(static_cast<std::uint16_t>(_ascii[index]));
    }
    UInt16(0);
  }

  void Material(const char* _name)
  {
    WideString(_name);
    // Ambient, diffuse, specular: float4 each. Then the specular power, one float. Then emissive,
    // float4. Seventeen floats, and the reader walking them as float3 is the failure this pins.
    Floats(4 + 4 + 4 + 1 + 4);
    Floats(16); // UV transform
    WideString("lambert.dgsl");
    for (int slot = 0; slot < 8; ++slot)
    {
      WideString("");
    }
  }

  void Vertex(float _x, float _y, float _z, std::uint32_t _color)
  {
    Float(_x);
    Float(_y);
    Float(_z);
    Float(0.0f);
    Float(1.0f);
    Float(0.0f); // normal
    Floats(4);   // tangent, dead
    UInt32(_color);
    Floats(2); // texture coordinate, dead
  }

  void Extents(float _min, float _max)
  {
    Floats(3);   // center
    Float(1.0f); // radius
    Float(_min);
    Float(_min);
    Float(_min);
    Float(_max);
    Float(_max);
    Float(_max);
  }

  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept
  {
    return std::span<const std::byte>{m_bytes.data(), m_bytes.size()};
  }

  [[nodiscard]] std::vector<std::byte>& Storage() noexcept
  {
    return m_bytes;
  }

private:
  void Append(const void* _source, std::size_t _size)
  {
    const auto* from = static_cast<const std::byte*>(_source);
    m_bytes.insert(m_bytes.end(), from, from + _size);
  }

  std::vector<std::byte> m_bytes;
};

/// The ordinary case: one mesh, one material, one index buffer, one vertex buffer, no skinning.
[[nodiscard]] CmoBuilder PlainMesh()
{
  CmoBuilder cmo;
  cmo.UInt32(1); // mesh count
  cmo.WideString("Scout");
  cmo.UInt32(1); // material count
  cmo.Material("OC_Hull");
  cmo.UInt8(0);  // no skeletal animation
  cmo.UInt32(1); // submesh count
  for (int field = 0; field < 5; ++field)
  {
    cmo.UInt32(0);
  }
  cmo.UInt32(1); // index buffer count
  cmo.UInt32(3);
  cmo.UInt16(0);
  cmo.UInt16(1);
  cmo.UInt16(2);
  cmo.UInt32(1); // vertex buffer count
  cmo.UInt32(3);
  cmo.Vertex(-1.0f, 0.0f, 0.0f, 0xFF0080FFu);
  cmo.Vertex(1.0f, 0.0f, 0.0f, 0xFF008000u);
  cmo.Vertex(0.0f, 2.0f, 45.0f, 0xFFFFFFFFu);
  cmo.UInt32(0); // no skinning vertex buffers
  cmo.Extents(-1.0f, 45.0f);
  return cmo;
}
} // namespace

/// ADR-005's file, read. **The reader is the risk in M1.9 and these tests are its.**
TEST_CLASS(TheCmoReader)
{
public:
  TEST_METHOD(APlainMeshReadsWhole)
  {
    const CmoBuilder cmo = PlainMesh();
    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::None);

    Assert::AreEqual(static_cast<std::size_t>(3), mesh.vertices.size());
    Assert::AreEqual(static_cast<std::size_t>(3), mesh.indices.size());
    Assert::AreEqual(45.0f, mesh.vertices[2].positionZ);
    Assert::AreEqual(2.0f, mesh.vertices[2].positionY);
    Assert::AreEqual(45.0f, mesh.maxX);
    Assert::AreEqual(-1.0f, mesh.minZ);
  }

  /// **A READER THAT IGNORES VERTEX COLOR RENDERS EVERY HULL WHITE**, which the manifest names as the
  /// thing to watch for: the material's diffuse is white on purpose because the albedo arrives
  /// entirely through this channel.
  TEST_METHOD(TheVertexColorSurvivesUnchanged)
  {
    const CmoBuilder cmo = PlainMesh();
    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::None);

    Assert::AreEqual(0xFF0080FFu, mesh.vertices[0].color);
    Assert::AreEqual(0xFF008000u, mesh.vertices[1].color);
    Assert::AreEqual(0xFFFFFFFFu, mesh.vertices[2].color);
  }

  /// **THE ONE FIELD THAT DOES NOT SURVIVE MEMORY.** A material's ambient, diffuse, specular and
  /// emissive are `float4` with the specular power a single float between specular and emissive. A
  /// `float3` reading walks off the end inside the eight texture slots and fails several kilobytes
  /// later, somewhere unrelated -- so this pins the width by adding a SECOND material, which doubles
  /// the error and makes the failure unmistakable rather than subtle.
  TEST_METHOD(TwoMaterialsStillLeaveTheWalkInStep)
  {
    CmoBuilder cmo;
    cmo.UInt32(1);
    cmo.WideString("Twice");
    cmo.UInt32(2);
    cmo.Material("OC_Hull");
    cmo.Material("OC_Hull_Second");
    cmo.UInt8(0);
    cmo.UInt32(0); // no submeshes
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.UInt16(0);
    cmo.UInt16(1);
    cmo.UInt16(2);
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.Vertex(0.0f, 0.0f, 0.0f, 1);
    cmo.Vertex(1.0f, 0.0f, 0.0f, 2);
    cmo.Vertex(0.0f, 1.0f, 0.0f, 3);
    cmo.UInt32(0);
    cmo.Extents(0.0f, 1.0f);

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::None);
    Assert::AreEqual(static_cast<std::size_t>(3), mesh.vertices.size());
  }

  /// **M1.9's OWN EXIT CRITERION.** CMO carries a skinning vertex buffer, a bone hierarchy and
  /// animation clips, none of which the MVP uses and **all of which it must skip rather than reject**
  /// -- so the suite feeds it a file that has them.
  TEST_METHOD(AFileCarryingSkinningBonesAndClipsIsRead)
  {
    CmoBuilder cmo;
    cmo.UInt32(1);
    cmo.WideString("Skinned");
    cmo.UInt32(1);
    cmo.Material("OC_Hull");
    cmo.UInt8(1); // skeletal animation IS present
    cmo.UInt32(0);
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.UInt16(2);
    cmo.UInt16(1);
    cmo.UInt16(0);
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.Vertex(0.0f, 0.0f, 0.0f, 0x11u);
    cmo.Vertex(1.0f, 0.0f, 0.0f, 0x22u);
    cmo.Vertex(0.0f, 1.0f, 0.0f, 0x33u);

    // Two skinning vertex buffers, three vertices each: four bone indices and four weights apiece.
    cmo.UInt32(2);
    for (int buffer = 0; buffer < 2; ++buffer)
    {
      cmo.UInt32(3);
      for (int vertex = 0; vertex < 3; ++vertex)
      {
        for (int bone = 0; bone < 4; ++bone)
        {
          cmo.UInt32(static_cast<std::uint32_t>(bone));
        }
        cmo.Floats(4, 0.25f);
      }
    }

    cmo.Extents(0.0f, 1.0f);

    // Two bones: a name and three 4x4 matrices each.
    cmo.UInt32(2);
    for (int bone = 0; bone < 2; ++bone)
    {
      cmo.WideString("bone");
      cmo.Floats(16 * 3);
    }

    // One clip with three keyframes: a bone index, a time and a 4x4 transform each.
    cmo.UInt32(1);
    cmo.WideString("idle");
    cmo.Float(0.0f);
    cmo.Float(1.0f);
    cmo.UInt32(3);
    for (int keyframe = 0; keyframe < 3; ++keyframe)
    {
      cmo.UInt32(0);
      cmo.Float(static_cast<float>(keyframe));
      cmo.Floats(16);
    }

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::None, L"the reader refused a skinned file");
    Assert::AreEqual(static_cast<std::size_t>(3), mesh.vertices.size());
    Assert::AreEqual(0x33u, mesh.vertices[2].color);
    Assert::AreEqual(static_cast<std::uint16_t>(2), mesh.indices[0]);
  }

  /// **PARSED TO EXACTLY ZERO TRAILING BYTES**, which is the check that catches a format change. It is
  /// why the reader walks the parts it does not use instead of seeking past them.
  TEST_METHOD(TrailingBytesAreRefused)
  {
    CmoBuilder cmo = PlainMesh();
    cmo.Storage().push_back(std::byte{0});

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::TrailingBytes);
  }

  /// Every truncation, at every length. A file that ends inside a field is refused rather than read as
  /// whatever was next in memory.
  TEST_METHOD(EveryTruncationIsRefused)
  {
    const CmoBuilder cmo = PlainMesh();
    const std::span<const std::byte> whole = cmo.Bytes();

    for (std::size_t length = 0; length < whole.size(); ++length)
    {
      Neuron::CmoMesh mesh;
      const Neuron::CmoFault fault = Neuron::ReadCmo(whole.subspan(0, length), mesh);
      Assert::IsTrue(fault != Neuron::CmoFault::None, L"a truncated file read as a whole one");
      Assert::AreEqual(static_cast<std::size_t>(0), mesh.vertices.size(), L"a refused read still wrote to its output");
    }
  }

  /// **AN INDEX PAST THE END DOES NOT FAIL ON A GPU, IT READS WHATEVER IS NEXT** and draws a triangle
  /// somewhere nobody asked for. Refused before anything is drawn with it.
  TEST_METHOD(AnIndexPastTheEndIsRefused)
  {
    CmoBuilder cmo;
    cmo.UInt32(1);
    cmo.WideString("Bad");
    cmo.UInt32(0); // no materials
    cmo.UInt8(0);
    cmo.UInt32(0);
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.UInt16(0);
    cmo.UInt16(1);
    cmo.UInt16(9); // there is no vertex 9
    cmo.UInt32(1);
    cmo.UInt32(3);
    cmo.Vertex(0.0f, 0.0f, 0.0f, 0);
    cmo.Vertex(1.0f, 0.0f, 0.0f, 0);
    cmo.Vertex(0.0f, 1.0f, 0.0f, 0);
    cmo.UInt32(0);
    cmo.Extents(0.0f, 1.0f);

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::IndexOutOfRange);
  }

  /// A count the rest of the file cannot satisfy is refused **before anything is reserved for it** --
  /// the same rule the command decoder applies to a selection count, and for the same reason: the
  /// count came off a disk and the allocation did not.
  TEST_METHOD(AnImpossibleCountIsRefusedBeforeItIsAllocated)
  {
    CmoBuilder cmo;
    cmo.UInt32(1);
    cmo.WideString("Huge");
    cmo.UInt32(0);
    cmo.UInt8(0);
    cmo.UInt32(0);
    cmo.UInt32(1);
    cmo.UInt32(0xFFFFFFF0u); // an index count no file this size could hold

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::ImpossibleCount);
  }

  /// Several meshes in one file is legal CMO and is not what this content ships; every consumer here
  /// indexes one buffer pair, so it is refused rather than half-read.
  TEST_METHOD(MoreThanOneMeshIsRefused)
  {
    CmoBuilder cmo;
    cmo.UInt32(2);

    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(cmo.Bytes(), mesh) == Neuron::CmoFault::MeshCount);

    CmoBuilder none;
    none.UInt32(0);
    Assert::IsTrue(Neuron::ReadCmo(none.Bytes(), mesh) == Neuron::CmoFault::MeshCount);
  }

  TEST_METHOD(AnEmptyFileIsTruncatedRatherThanEmpty)
  {
    Neuron::CmoMesh mesh;
    Assert::IsTrue(Neuron::ReadCmo(std::span<const std::byte>{}, mesh) == Neuron::CmoFault::Truncated);
  }

  /// Fifty-two bytes a vertex, which is the stride Q44 declined to repack.
  TEST_METHOD(TheVertexStrideIsFiftyTwo)
  {
    Assert::AreEqual(static_cast<std::size_t>(52), Neuron::CMO_VERTEX_STRIDE);
  }
};

} // namespace NeuronClientTests

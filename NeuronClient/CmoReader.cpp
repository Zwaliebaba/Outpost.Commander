#include "pch.h"

#include "CmoReader.h"

#include <cstring>

namespace Neuron
{

namespace
{
/// A cursor over the file, with the same latching discipline `ByteReader` has and for the same reason:
/// every buffer it is handed came off a disk and may be truncated, padded or simply wrong.
///
/// **IT IS NOT `ByteReader`.** That one is the wire's and reads big records field by field with a
/// bounds check each time; this walks kilobytes of structure it does not use, so what it needs is a
/// cheap skip and one fault flag -- and putting a `float` reader on the wire's type would invite one
/// into a packet, which R16 spent a table avoiding.
class Walk
{
public:
  explicit Walk(std::span<const std::byte> _bytes) noexcept
    : m_bytes(_bytes)
  {
  }

  [[nodiscard]] bool Faulted() const noexcept
  {
    return m_faulted;
  }

  [[nodiscard]] std::size_t Offset() const noexcept
  {
    return m_offset;
  }

  [[nodiscard]] std::size_t Remaining() const noexcept
  {
    return m_faulted ? 0 : (m_bytes.size() - m_offset);
  }

  [[nodiscard]] std::uint32_t UInt32() noexcept
  {
    std::uint32_t value = 0;
    Take(&value, sizeof(value));
    return value;
  }

  [[nodiscard]] std::uint16_t UInt16() noexcept
  {
    std::uint16_t value = 0;
    Take(&value, sizeof(value));
    return value;
  }

  [[nodiscard]] std::uint8_t UInt8() noexcept
  {
    std::uint8_t value = 0;
    Take(&value, sizeof(value));
    return value;
  }

  [[nodiscard]] float Float() noexcept
  {
    float value = 0.0f;
    Take(&value, sizeof(value));
    return value;
  }

  void Skip(std::size_t _bytes) noexcept
  {
    if (m_faulted || (_bytes > (m_bytes.size() - m_offset)))
    {
      m_faulted = true;
      return;
    }
    m_offset += _bytes;
  }

  /// A UINT count of UTF-16 code units followed by that many, the terminating null included. The
  /// contents are never needed: a mesh name, a shader name and eight empty texture slots.
  void SkipWideString() noexcept
  {
    const std::uint32_t units = UInt32();
    if (m_faulted)
    {
      return;
    }

    // Checked as a product against what is left, before it becomes an offset -- the count is
    // file-controlled and 0xFFFFFFFF times two overflows a 32-bit size.
    const std::size_t bytes = static_cast<std::size_t>(units) * 2;
    Skip(bytes);
  }

private:
  void Take(void* _destination, std::size_t _size) noexcept
  {
    if (m_faulted || (_size > (m_bytes.size() - m_offset)))
    {
      m_faulted = true;
      return;
    }
    std::memcpy(_destination, m_bytes.data() + m_offset, _size);
    m_offset += _size;
  }

  std::span<const std::byte> m_bytes;
  std::size_t m_offset = 0;
  bool m_faulted = false;
};

/// A material block. Everything in it is skipped and the widths are the point.
void SkipMaterial(Walk& _walk) noexcept
{
  _walk.SkipWideString();

  // **FOUR, FOUR, FOUR, ONE, FOUR.** Ambient, diffuse and specular are `float4`; the specular power is
  // a single float BETWEEN specular and emissive; emissive is `float4`. Reading any of the four as a
  // `float3` walks off the end inside the texture slots below and fails somewhere unrelated.
  _walk.Skip(sizeof(float) * (4 + 4 + 4 + 1 + 4));

  // The UV transform.
  _walk.Skip(sizeof(float) * 16);

  // The pixel shader name, then eight texture slots -- empty in this content and each still
  // length-prefixed, so each still costs its four bytes.
  _walk.SkipWideString();
  for (int slot = 0; slot < 8; ++slot)
  {
    _walk.SkipWideString();
  }
}

/// The bones and the animation clips, which the MVP uses none of and must walk rather than refuse.
void SkipSkeleton(Walk& _walk) noexcept
{
  const std::uint32_t boneCount = _walk.UInt32();
  for (std::uint32_t bone = 0; (bone < boneCount) && !_walk.Faulted(); ++bone)
  {
    _walk.SkipWideString();

    // An inverse bind pose, a bind pose and a local transform: three 4x4 matrices.
    _walk.Skip(sizeof(float) * 16 * 3);
  }

  const std::uint32_t clipCount = _walk.UInt32();
  for (std::uint32_t clip = 0; (clip < clipCount) && !_walk.Faulted(); ++clip)
  {
    _walk.SkipWideString();
    _walk.Skip(sizeof(float) * 2); // start time, end time

    const std::uint32_t keyframeCount = _walk.UInt32();
    for (std::uint32_t keyframe = 0; (keyframe < keyframeCount) && !_walk.Faulted(); ++keyframe)
    {
      // A bone index, a time, and a 4x4 transform.
      _walk.Skip(sizeof(std::uint32_t) + sizeof(float) + (sizeof(float) * 16));
    }
  }
}

/// True when a count could not possibly be satisfied by what is left. **Checked before anything is
/// reserved for it**, which is the same rule `Command`'s decoder applies to a selection count and for
/// the same reason: the count came off a disk and the allocation did not.
[[nodiscard]] bool ImpossibleCount(const Walk& _walk, std::uint32_t _count, std::size_t _bytesEach) noexcept
{
  if (_count > CMO_MAXIMUM_ELEMENTS)
  {
    return true;
  }
  return (static_cast<std::size_t>(_count) * _bytesEach) > _walk.Remaining();
}
} // namespace

CmoFault ReadCmo(std::span<const std::byte> _bytes, CmoMesh& _outMesh)
{
  Walk walk{_bytes};

  const std::uint32_t meshCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  if (meshCount != 1)
  {
    return CmoFault::MeshCount;
  }

  walk.SkipWideString();

  const std::uint32_t materialCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  for (std::uint32_t material = 0; (material < materialCount) && !walk.Faulted(); ++material)
  {
    SkipMaterial(walk);
  }

  const std::uint8_t skeletal = walk.UInt8();

  const std::uint32_t submeshCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  if (ImpossibleCount(walk, submeshCount, 20))
  {
    return CmoFault::ImpossibleCount;
  }
  walk.Skip(static_cast<std::size_t>(submeshCount) * 20);

  CmoMesh lifted;

  const std::uint32_t indexBufferCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  for (std::uint32_t buffer = 0; (buffer < indexBufferCount) && !walk.Faulted(); ++buffer)
  {
    const std::uint32_t indexCount = walk.UInt32();
    if (walk.Faulted())
    {
      return CmoFault::Truncated;
    }
    if (ImpossibleCount(walk, indexCount, sizeof(std::uint16_t)))
    {
      return CmoFault::ImpossibleCount;
    }

    lifted.indices.reserve(lifted.indices.size() + indexCount);
    for (std::uint32_t index = 0; index < indexCount; ++index)
    {
      lifted.indices.push_back(walk.UInt16());
    }
  }

  const std::uint32_t vertexBufferCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  if (vertexBufferCount != 1)
  {
    return CmoFault::VertexBufferCount;
  }

  const std::uint32_t vertexCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  if (ImpossibleCount(walk, vertexCount, CMO_VERTEX_STRIDE))
  {
    return CmoFault::ImpossibleCount;
  }

  lifted.vertices.reserve(vertexCount);
  for (std::uint32_t vertex = 0; vertex < vertexCount; ++vertex)
  {
    CmoVertex lifted_vertex;
    lifted_vertex.positionX = walk.Float();
    lifted_vertex.positionY = walk.Float();
    lifted_vertex.positionZ = walk.Float();
    lifted_vertex.normalX = walk.Float();
    lifted_vertex.normalY = walk.Float();
    lifted_vertex.normalZ = walk.Float();

    // The tangent's sixteen bytes, dead in this content (Q44).
    walk.Skip(sizeof(float) * 4);

    lifted_vertex.color = walk.UInt32();

    // The texture coordinate's eight, dead for the same reason.
    walk.Skip(sizeof(float) * 2);

    lifted.vertices.push_back(lifted_vertex);
  }

  // **THE SKINNING BUFFERS ARE WALKED, NOT REFUSED.** The MVP uses none of them, and a reader facing a
  // file it did not produce has no standing to say the file is wrong for containing them.
  const std::uint32_t skinningBufferCount = walk.UInt32();
  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }
  for (std::uint32_t buffer = 0; (buffer < skinningBufferCount) && !walk.Faulted(); ++buffer)
  {
    const std::uint32_t skinCount = walk.UInt32();
    if (walk.Faulted())
    {
      return CmoFault::Truncated;
    }

    // Four bone indices and four weights a vertex.
    constexpr std::size_t SKINNING_VERTEX_STRIDE = (sizeof(std::uint32_t) * 4) + (sizeof(float) * 4);
    if (ImpossibleCount(walk, skinCount, SKINNING_VERTEX_STRIDE))
    {
      return CmoFault::ImpossibleCount;
    }
    walk.Skip(static_cast<std::size_t>(skinCount) * SKINNING_VERTEX_STRIDE);
  }

  lifted.centerX = walk.Float();
  lifted.centerY = walk.Float();
  lifted.centerZ = walk.Float();
  lifted.radius = walk.Float();
  lifted.minX = walk.Float();
  lifted.minY = walk.Float();
  lifted.minZ = walk.Float();
  lifted.maxX = walk.Float();
  lifted.maxY = walk.Float();
  lifted.maxZ = walk.Float();

  if (skeletal != 0)
  {
    SkipSkeleton(walk);
  }

  if (walk.Faulted())
  {
    return CmoFault::Truncated;
  }

  // **PARSED TO EXACTLY ZERO TRAILING BYTES.** Bytes left over do not mean the file is long, they mean
  // this walk is out of step with it -- and that is the check that catches a format change, which is
  // why the reader parses the parts it does not use instead of seeking past them.
  if (walk.Offset() != _bytes.size())
  {
    return CmoFault::TrailingBytes;
  }

  for (const std::uint16_t index : lifted.indices)
  {
    if (static_cast<std::size_t>(index) >= lifted.vertices.size())
    {
      // Refused before anything is drawn with it. An index past the end does not fail on a GPU, it
      // reads whatever is next and draws a triangle somewhere nobody asked for.
      return CmoFault::IndexOutOfRange;
    }
  }

  _outMesh = std::move(lifted);
  return CmoFault::None;
}

} // namespace Neuron

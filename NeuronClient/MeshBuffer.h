#pragma once

#include "GraphicsDevice.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct MeshBufferBinding;

/// A vertex buffer and an index buffer on the device, for one mesh.
///
/// **IT IS AN UPLOAD HEAP AND NOT A DEFAULT ONE, WHICH IS A TRADE WORTH NAMING.** A default heap is
/// faster to read and needs a copy command list, a staging buffer and a fence wait to fill; an upload
/// heap is written by the processor and read across the bus every frame. The whole delivered set is
/// **436 kilobytes over thirteen meshes** and the MVP draws 2,674 triangles a frame, so the bandwidth
/// is not measurable against a 1,118-microsecond frame -- and what it buys is that a mesh is ready
/// the instant it is written, with no second queue to synchronize and no upload that can still be in
/// flight when the first frame draws.
///
/// **THE DAY THAT STOPS BEING TRUE IS THE DAY THERE IS SOMETHING TO MEASURE**, and the change is
/// local to this class.
class MeshBuffer
{
public:
  MeshBuffer() noexcept;
  ~MeshBuffer() noexcept;

  MeshBuffer(const MeshBuffer&) = delete;
  MeshBuffer& operator=(const MeshBuffer&) = delete;
  MeshBuffer(MeshBuffer&&) noexcept;
  MeshBuffer& operator=(MeshBuffer&&) noexcept;

  /// Creates both buffers and copies the data in. _vertexStride is the caller's, because the engine
  /// does not know what a vertex of this mesh contains (R9).
  ///
  /// False on an empty mesh, on a stride of zero, or on a failed allocation; `LastHresult` carries
  /// the reason.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, std::span<const std::byte> _vertices, std::size_t _vertexStride,
                            std::span<const std::uint16_t> _indices) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  [[nodiscard]] std::uint32_t IndexCount() const noexcept;
  [[nodiscard]] std::uint32_t VertexCount() const noexcept;

  /// The device addresses and strides, for whoever records the draw. **Returned as plain integers so
  /// this header does not drag `d3d12.h` into every consumer** -- the same reason `SceneTarget` hands
  /// out format codes rather than `DXGI_FORMAT`.
  [[nodiscard]] std::uint64_t VertexBufferAddress() const noexcept;
  [[nodiscard]] std::uint64_t IndexBufferAddress() const noexcept;
  [[nodiscard]] std::uint32_t VertexBufferBytes() const noexcept;
  [[nodiscard]] std::uint32_t IndexBufferBytes() const noexcept;
  [[nodiscard]] std::uint32_t VertexStride() const noexcept;

private:
  std::shared_ptr<MeshBufferBinding> m_binding;
};

/// One instance's worth of what the ship shader needs, and **the layout is the shader's input
/// element for slot one** -- seven floats, twenty-eight bytes.
///
/// R8: a public aggregate.
struct MeshInstance
{
  /// On the plane, in world units.
  float positionX = 0.0f;
  float positionY = 0.0f;

  /// **THE HEADING, PRECOMPUTED.** A `sincos` per vertex to turn one entity is work this
  /// arrangement exists to avoid, and the processor has the angle already.
  float headingCosine = 1.0f;
  float headingSine = 0.0f;

  float teamRed = 1.0f;
  float teamGreen = 1.0f;
  float teamBlue = 1.0f;
};

/// A ring of per-instance buffers, one for every frame the swap chain can have in flight.
///
/// **A SINGLE BUFFER WOULD BE WRITTEN WHILE THE GRAPHICS PROCESSOR WAS READING THE LAST FRAME'S
/// COPY**, which is the classic upload-heap defect: it does not crash, it draws one frame of ships
/// at the positions of another, intermittently, at whatever rate the two ends happen to alias.
class InstanceRing
{
public:
  /// The swap chain's, and it is not restated -- see the .cpp.
  static constexpr std::uint32_t FRAME_COUNT = 3;

  InstanceRing() noexcept;
  ~InstanceRing() noexcept;

  InstanceRing(const InstanceRing&) = delete;
  InstanceRing& operator=(const InstanceRing&) = delete;
  InstanceRing(InstanceRing&&) = delete;
  InstanceRing& operator=(InstanceRing&&) = delete;

  /// _capacity is instances per frame. ADR-003's peak is 110 entities and 220 after M2.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, std::uint32_t _capacity) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;
  [[nodiscard]] std::uint32_t Capacity() const noexcept;

  /// Writes into the buffer for _frameIndex and returns how many were written -- which is fewer than
  /// asked for when the capacity is smaller, rather than writing past the end.
  [[nodiscard]] std::uint32_t Write(std::uint32_t _frameIndex, std::span<const MeshInstance> _instances) noexcept;

  [[nodiscard]] std::uint64_t BufferAddress(std::uint32_t _frameIndex) const noexcept;
  [[nodiscard]] std::uint32_t BufferBytes() const noexcept;

private:
  std::shared_ptr<struct InstanceRingBinding> m_binding;
};

} // namespace Neuron

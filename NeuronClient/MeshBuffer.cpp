#include "pch.h"

#include "MeshBuffer.h"

#include <d3d12.h>

#include <winrt/base.h>

#include <cstring>

namespace Neuron
{

struct MeshBufferBinding
{
  winrt::com_ptr<ID3D12Resource> vertexBuffer;
  winrt::com_ptr<ID3D12Resource> indexBuffer;

  std::uint32_t vertexCount = 0;
  std::uint32_t indexCount = 0;
  std::uint32_t vertexStride = 0;
  std::uint32_t vertexBytes = 0;
  std::uint32_t indexBytes = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

struct InstanceRingBinding
{
  winrt::com_ptr<ID3D12Resource> buffers[InstanceRing::FRAME_COUNT];

  /// **MAPPED ONCE AND NEVER UNMAPPED.** An upload heap may stay mapped for the resource's life, and
  /// mapping per frame is a driver call per frame for nothing.
  void* mapped[InstanceRing::FRAME_COUNT] = {};

  std::uint32_t capacity = 0;
  std::uint32_t bytes = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
[[nodiscard]] bool OpenDevice(const GraphicsDevice& _device, winrt::com_ptr<ID3D12Device>& _outDevice) noexcept
{
  return (_device.State() == DeviceState::Ready) && (_device.DeviceUnknown() != nullptr) &&
         SUCCEEDED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), _outDevice.put_void()));
}

/// One upload-heap buffer of _bytes, with the data copied in when there is any.
[[nodiscard]] HRESULT CreateUploadBuffer(ID3D12Device& _device, std::uint32_t _bytes, const void* _source,
                                         winrt::com_ptr<ID3D12Resource>& _outResource, void** _outMapped) noexcept
{
  const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD,
                                   .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                   .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                   .CreationNodeMask = 1,
                                   .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                        .Alignment = 0,
                                        .Width = _bytes,
                                        .Height = 1,
                                        .DepthOrArraySize = 1,
                                        .MipLevels = 1,
                                        .Format = DXGI_FORMAT_UNKNOWN,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                        .Flags = D3D12_RESOURCE_FLAG_NONE};

  const HRESULT created = _device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                          nullptr, winrt::guid_of<ID3D12Resource>(), _outResource.put_void());
  if (FAILED(created))
  {
    return created;
  }

  void* mapped = nullptr;

  // A READ RANGE OF NOTHING, because nothing here reads back. Passing null would tell the runtime
  // the whole resource may have been read, which on a discrete adapter costs a flush.
  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  const HRESULT opened = _outResource->Map(0, &noRead, &mapped);
  if (FAILED(opened))
  {
    return opened;
  }

  if (_source != nullptr)
  {
    std::memcpy(mapped, _source, _bytes);
  }

  if (_outMapped != nullptr)
  {
    *_outMapped = mapped;
  }
  else
  {
    // Written once and never again, so the mapping is closed. A written range of the whole buffer,
    // because all of it changed.
    const D3D12_RANGE wroteAll{.Begin = 0, .End = _bytes};
    _outResource->Unmap(0, &wroteAll);
  }
  return S_OK;
}
} // namespace

MeshBuffer::MeshBuffer() noexcept
  : m_binding(std::make_shared<MeshBufferBinding>())
{
}

MeshBuffer::~MeshBuffer() noexcept = default;

MeshBuffer::MeshBuffer(MeshBuffer&&) noexcept = default;

MeshBuffer& MeshBuffer::operator=(MeshBuffer&&) noexcept = default;

bool MeshBuffer::Create(const GraphicsDevice& _device, std::span<const std::byte> _vertices, std::size_t _vertexStride,
                        std::span<const std::uint16_t> _indices) noexcept
{
  MeshBufferBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if (_vertices.empty() || _indices.empty() || (_vertexStride == 0) || ((_vertices.size() % _vertexStride) != 0))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  binding.vertexBytes = static_cast<std::uint32_t>(_vertices.size());
  binding.indexBytes = static_cast<std::uint32_t>(_indices.size() * sizeof(std::uint16_t));
  binding.vertexStride = static_cast<std::uint32_t>(_vertexStride);
  binding.vertexCount = static_cast<std::uint32_t>(_vertices.size() / _vertexStride);
  binding.indexCount = static_cast<std::uint32_t>(_indices.size());

  binding.lastHresult = CreateUploadBuffer(*device, binding.vertexBytes, _vertices.data(), binding.vertexBuffer, nullptr);
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.lastHresult = CreateUploadBuffer(*device, binding.indexBytes, _indices.data(), binding.indexBuffer, nullptr);
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.ready = true;
  return true;
}

void MeshBuffer::Destroy() noexcept
{
  MeshBufferBinding& binding = *m_binding;
  binding.vertexBuffer = nullptr;
  binding.indexBuffer = nullptr;
  binding.ready = false;
}

bool MeshBuffer::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t MeshBuffer::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t MeshBuffer::IndexCount() const noexcept
{
  return m_binding->indexCount;
}

std::uint32_t MeshBuffer::VertexCount() const noexcept
{
  return m_binding->vertexCount;
}

std::uint64_t MeshBuffer::VertexBufferAddress() const noexcept
{
  return m_binding->vertexBuffer ? m_binding->vertexBuffer->GetGPUVirtualAddress() : 0;
}

std::uint64_t MeshBuffer::IndexBufferAddress() const noexcept
{
  return m_binding->indexBuffer ? m_binding->indexBuffer->GetGPUVirtualAddress() : 0;
}

std::uint32_t MeshBuffer::VertexBufferBytes() const noexcept
{
  return m_binding->vertexBytes;
}

std::uint32_t MeshBuffer::IndexBufferBytes() const noexcept
{
  return m_binding->indexBytes;
}

std::uint32_t MeshBuffer::VertexStride() const noexcept
{
  return m_binding->vertexStride;
}

InstanceRing::InstanceRing() noexcept
  : m_binding(std::make_shared<InstanceRingBinding>())
{
}

InstanceRing::~InstanceRing() noexcept = default;

bool InstanceRing::Create(const GraphicsDevice& _device, std::uint32_t _capacity) noexcept
{
  InstanceRingBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if (_capacity == 0)
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  binding.capacity = _capacity;
  binding.bytes = _capacity * static_cast<std::uint32_t>(sizeof(MeshInstance));

  for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame)
  {
    binding.lastHresult = CreateUploadBuffer(*device, binding.bytes, nullptr, binding.buffers[frame], &binding.mapped[frame]);
    if (FAILED(binding.lastHresult))
    {
      return false;
    }
  }

  binding.ready = true;
  return true;
}

void InstanceRing::Destroy() noexcept
{
  InstanceRingBinding& binding = *m_binding;
  for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame)
  {
    binding.mapped[frame] = nullptr;
    binding.buffers[frame] = nullptr;
  }
  binding.ready = false;
}

bool InstanceRing::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t InstanceRing::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t InstanceRing::Capacity() const noexcept
{
  return m_binding->capacity;
}

std::uint32_t InstanceRing::Write(std::uint32_t _frameIndex, std::span<const MeshInstance> _instances) noexcept
{
  InstanceRingBinding& binding = *m_binding;
  if (!binding.ready || (_frameIndex >= FRAME_COUNT) || (binding.mapped[_frameIndex] == nullptr))
  {
    return 0;
  }

  // FEWER THAN ASKED FOR RATHER THAN PAST THE END. The count comes from however many entities a
  // snapshot carried, which is not this class's to bound.
  const auto count = static_cast<std::uint32_t>((_instances.size() < binding.capacity) ? _instances.size() : binding.capacity);
  if (count > 0)
  {
    std::memcpy(binding.mapped[_frameIndex], _instances.data(), static_cast<std::size_t>(count) * sizeof(MeshInstance));
  }
  return count;
}

std::uint64_t InstanceRing::BufferAddress(std::uint32_t _frameIndex) const noexcept
{
  const InstanceRingBinding& binding = *m_binding;
  if ((_frameIndex >= FRAME_COUNT) || !binding.buffers[_frameIndex])
  {
    return 0;
  }
  return binding.buffers[_frameIndex]->GetGPUVirtualAddress();
}

std::uint32_t InstanceRing::BufferBytes() const noexcept
{
  return m_binding->bytes;
}

} // namespace Neuron

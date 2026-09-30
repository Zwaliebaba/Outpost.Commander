#include "pch.h"
#include "Mesh.h"

Neuron::Mesh::Mesh(Renderer& _renderer, const MeshData& _data)
  : m_indexCount(static_cast<UINT>(_data.indices.size())),
    m_boundsMin(_data.boundsMin),
    m_boundsMax(_data.boundsMax)
{
  const auto vertexBytes = std::as_bytes(std::span(_data.vertices));
  const auto indexBytes = std::as_bytes(std::span(_data.indices));
  m_vertexBuffer = _renderer.CreateStaticBuffer(vertexBytes);
  m_indexBuffer = _renderer.CreateStaticBuffer(indexBytes);

  m_vertexBufferView = {
    .BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(),
    .SizeInBytes = static_cast<UINT>(vertexBytes.size()),
    .StrideInBytes = sizeof(MeshVertex),
  };
  m_indexBufferView = {
    .BufferLocation = m_indexBuffer->GetGPUVirtualAddress(),
    .SizeInBytes = static_cast<UINT>(indexBytes.size()),
    .Format = DXGI_FORMAT_R32_UINT,
  };
}

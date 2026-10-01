#pragma once

namespace Neuron
{
class Renderer;
struct MeshData;

// A mesh in video memory: its vertices and indices, uploaded once, and its bounds (ADR-011). The mesh pipeline draws it.
class Mesh : NonCopyable
{
public:
  // Uploads _data. Throws winrt::hresult_error if the device cannot take it.
  Mesh(Renderer& _renderer, const MeshData& _data);

  [[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView() const noexcept
  {
    return m_vertexBufferView;
  }

  [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& IndexBufferView() const noexcept
  {
    return m_indexBufferView;
  }

  [[nodiscard]] UINT IndexCount() const noexcept
  {
    return m_indexCount;
  }

  [[nodiscard]] const DirectX::XMFLOAT3& BoundsMin() const noexcept
  {
    return m_boundsMin;
  }

  [[nodiscard]] const DirectX::XMFLOAT3& BoundsMax() const noexcept
  {
    return m_boundsMax;
  }

private:
  winrt::com_ptr<ID3D12Resource> m_vertexBuffer;
  winrt::com_ptr<ID3D12Resource> m_indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
  D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
  UINT m_indexCount = 0;
  DirectX::XMFLOAT3 m_boundsMin{};
  DirectX::XMFLOAT3 m_boundsMax{};
};
} // namespace Neuron
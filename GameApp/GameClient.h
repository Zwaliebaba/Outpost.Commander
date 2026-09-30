#pragma once

namespace Outpost
{
// The client's view of the game: every model loaded into video memory, the camera, and the scene drawn through it. Until
// task 2.5 draws from snapshots, the scene is milestone 1's: the Small, Medium and Large hulls of both sets side by side
// facing +x, an asteroid, and a grid on the ground that shows scale (design §14).
class GameClient : Neuron::NonCopyable
{
public:
  // Reads Models.json and Camera.json from the package's Assets folder and uploads every model's mesh. Throws
  // Neuron::Exception naming the file when a data file or a mesh is missing or cannot be read, so that no model is
  // silently left out.
  explicit GameClient(Neuron::Renderer& _renderer);

  void Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
              std::uint32_t _viewportHeightPixels) noexcept;

  // Draws the scene into the frame the renderer has begun.
  void Render(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList);

private:
  struct Placement
  {
    const Neuron::Mesh* mesh = nullptr;
    DirectX::XMFLOAT4X4 world{};
    DirectX::XMFLOAT4 color{};
  };

  [[nodiscard]] const Neuron::Mesh& ModelMesh(std::string_view _set, std::string_view _model) const;
  void PlaceLineup();

  ModelCatalog m_catalog;
  Camera m_camera;
  Neuron::MeshPipeline m_pipeline;
  // Keyed by "<set>/<model>".
  std::map<std::string, std::unique_ptr<Neuron::Mesh>, std::less<>> m_modelMeshes;
  std::unique_ptr<Neuron::Mesh> m_minorGrid;
  std::unique_ptr<Neuron::Mesh> m_majorGrid;
  std::vector<Placement> m_placements;
};
} // namespace Outpost

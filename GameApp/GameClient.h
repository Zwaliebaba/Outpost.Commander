#pragma once

namespace Outpost
{
// The client's view of the game: every model loaded into video memory, the camera, and the world drawn through it from
// interpolated snapshots (task 2.5, ADR-013). It never sees server state, only what the transport brings.
class GameClient : Neuron::NonCopyable
{
public:
  // Reads Models.json and Camera.json from the package's Assets folder and uploads every model's mesh. Throws
  // Neuron::Exception naming the file when a data file or a mesh is missing or cannot be read, so that no model is
  // silently left out. _ticksPerSecond is the server's rate, which the snapshots are interpolated at.
  GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond);

  // Takes the snapshots that arrived since the last frame. The first one centers the camera on the player's own ships.
  void Receive(std::vector<Snapshot> _snapshots);

  // Runs the view on by a frame, then the camera and the player's controls against what the view now shows.
  void Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
              std::uint32_t _viewportHeightPixels);

  // The orders the player gave since the last call, for the transport.
  [[nodiscard]] std::vector<Command> TakeCommands()
  {
    return m_controls.TakeCommands();
  }

  // Draws the world into the frame the renderer has begun.
  void Render(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList);

  // Task 2.7's order-to-response probe. When the frame just drawn is the first to show a ship of the last move order
  // visibly respond, its center or nose moved by a pixel or more, this returns when the order's input was read, once. The caller takes the time after presenting
  // the frame and has the latency. An order given to ships already moving is not measured: their motion would not be
  // its response.
  [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> TakeResponseShown()
  {
    return std::exchange(m_responseShown, std::nullopt);
  }

private:
  [[nodiscard]] const Neuron::Mesh& ModelMesh(std::string_view _set, std::string_view _model) const;
  void DrawEntity(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity);
  // A structure drawn to its footprint, darker while it is built (task 4.2).
  void DrawStructure(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity);
  // The structure being placed, at the cursor, green where it may stand and red where it may not.
  void DrawGhost(ID3D12GraphicsCommandList* _commandList);
  // Presses on the HUD's buttons and minimap, which the controls never see; and a drag on the minimap moves the camera.
  void HandleHudInput(const Neuron::InputState& _input);
  // The ground the camera shows, its corners in order, for the minimap; empty when a corner sees past the horizon.
  [[nodiscard]] std::vector<PlanePosition> ViewOnGround() const;
  void DrawSelection(ID3D12GraphicsCommandList* _commandList);
  // A bar over each damaged ship and structure, its length the share of hit points left (task 3.5), and one over each
  // structure under construction, its length the share built (task 4.2).
  void DrawHealthBars(ID3D12GraphicsCommandList* _commandList);
  void DrawEffects(ID3D12GraphicsCommandList* _commandList);
  void DrawHud(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex);
  // A level band from one point to another, _widthMeters wide and _heightMeters above the ground: a drag box's edge, a
  // health bar, a tracer or a beam.
  void DrawBand(ID3D12GraphicsCommandList* _commandList, PlanePosition _from, PlanePosition _to, float _widthMeters, float _heightMeters,
                const DirectX::XMFLOAT4& _color);

  ModelCatalog m_catalog;
  Camera m_camera;
  Neuron::MeshPipeline m_pipeline;
  Neuron::UiPipeline m_ui;
  // The HUD as this frame draws it; the next frame's clicks are tested against it.
  Hud::Layout m_hudLayout;
  SnapshotInterpolator m_view;
  PlayerControls m_controls;
  CombatEffects m_effects;
  // What the effects draw this frame, at the view's tick.
  std::vector<CombatEffects::Draw> m_effectDraws;
  // The view's entities this frame, which the controls pick from and the renderer draws.
  std::vector<EntityView> m_entities;
  Viewport m_viewport;
  // Keyed by "<set>/<model>".
  std::map<std::string, std::unique_ptr<Neuron::Mesh>, std::less<>> m_modelMeshes;
  std::unique_ptr<Neuron::Mesh> m_minorGrid;
  std::unique_ptr<Neuron::Mesh> m_majorGrid;
  // A ring and a disc of radius 1 and a strip 1 long and 1 wide, all flat on the ground, scaled where they are drawn.
  std::unique_ptr<Neuron::Mesh> m_ring;
  std::unique_ptr<Neuron::Mesh> m_disc;
  std::unique_ptr<Neuron::Mesh> m_strip;
  bool m_cameraPlaced = false;
  // The left button went down on the minimap and is still held.
  bool m_minimapDragging = false;
  // Where the cursor points on the ground this frame, for the ghost.
  std::optional<PlanePosition> m_cursorGround;

  // The move order being watched: its ships as the view showed them when it was given.
  struct ResponseProbe
  {
    std::vector<EntityView> ships;
    std::chrono::steady_clock::time_point inputRead;
  };
  void WatchForResponse();
  std::vector<EntityView> m_previousEntities;
  std::optional<ResponseProbe> m_probe;
  std::optional<std::chrono::steady_clock::time_point> m_responseShown;
};
} // namespace Outpost

#pragma once

namespace Outpost
{
// The client's view of the game: every model loaded into video memory, the camera, and the world drawn through it from
// interpolated snapshots (task 2.5, ADR-013). It never sees server state, only what the transport brings. It starts on the
// main menu; the shell starts a match on it, and takes it back to the menu, when the player asks for either (task 6.2).
class GameClient : Neuron::NonCopyable
{
public:
  // What the player asked the shell for, from the menu or the match's end.
  enum class Request : std::uint8_t
  {
    StartSkirmish,
    Quit,
    BackToMenu
  };

  // Reads Models.json and Camera.json from the package's Assets folder and uploads every model's mesh. Throws
  // Neuron::Exception naming the file when a data file or a mesh is missing or cannot be read, so that no model is
  // silently left out. _ticksPerSecond is the server's rate, which the snapshots are interpolated at.
  GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond);

  // Shows a new match from its first snapshot: whatever the last match left in the view, the selection, the designer and
  // the effects is gone, and the camera centers again on the player's fleet.
  void StartMatch();

  // Shows the main menu, and nothing of the match that was.
  void ShowMenu();

  // What the player asked for since the last call, if anything.
  [[nodiscard]] std::optional<Request> TakeRequest()
  {
    return std::exchange(m_request, std::nullopt);
  }

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
  // A ship's or a structure's model and where it stands, which drawing it and finding its hardpoints share.
  struct PlacedModel
  {
    const ModelSet* set = nullptr;
    const std::string* model = nullptr;
    ModelPose pose;
    // The share of the set's color a structure is drawn in (StructureModel::tint).
    float tint = 1.0f;
  };

  [[nodiscard]] const Neuron::Mesh& ModelMesh(std::string_view _set, std::string_view _model) const;
  [[nodiscard]] const std::vector<Neuron::MeshHardpoint>& ModelHardpoints(std::string_view _set, std::string_view _model) const;
  // A model's triangles on the CPU, for an explosion to break (ADR-026).
  [[nodiscard]] const Neuron::MeshData& ModelShape(std::string_view _set, std::string_view _model) const;
  // Nothing for what is not a ship or a structure, or what the data does not map to a model.
  [[nodiscard]] std::optional<PlacedModel> PlaceModel(const EntityView& _entity) const;
  // The shooter's gun nearest _target where the view draws it this frame, for the combat effects (ADR-018).
  [[nodiscard]] std::optional<PlanePosition> MuzzleOf(EntityId _shooter, PlanePosition _target) const;
  void DrawEntity(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity);
  // Starts the blast and the explosion of every ship and structure _snapshot reports destroyed (ADR-026), before the view
  // takes the snapshot, so that the view still holds what blew up.
  void Explode(const Snapshot& _snapshot);
  // The shards of every explosion, as ExplosionManager gives them for the view's tick.
  void DrawShards(ID3D12GraphicsCommandList* _commandList);
  // The model _set/_model placed by _world: its faces darker than _color now, and its creases over them as lines,
  // lighter, queued for the frame's one pass of lines (ADR-027).
  void DrawModel(ID3D12GraphicsCommandList* _commandList, std::string_view _set, std::string_view _model, const DirectX::XMFLOAT4X4& _world,
                 const DirectX::XMFLOAT4& _color);
  // A structure drawn to its footprint, darker while it is built (task 4.2).
  void DrawStructure(ID3D12GraphicsCommandList* _commandList, const EntityView& _entity);
  // The structure being placed, at the cursor, green where it may stand and red where it may not.
  void DrawGhost(ID3D12GraphicsCommandList* _commandList);
  // Presses on the HUD's buttons and minimap, which the controls never see; and a drag on the minimap moves the camera.
  void HandleHudInput(const Neuron::InputState& _input);
  // What a HUD button does: arms a placement, queues a job or a topic, or works the designer.
  void HandleHudAction(const Hud::Action& _action);
  // Forgets the match being shown.
  void ClearMatch();
  // While the designer's name takes typing, the keyboard is the designer's: _input loses its keys, so no order, control
  // group or camera key reads them.
  void HandleTyping(Neuron::InputState& _input);
  // The ground the camera shows, its corners in order, for the minimap; empty when a corner sees past the horizon.
  [[nodiscard]] std::vector<PlanePosition> ViewOnGround() const;
  void DrawSelection(ID3D12GraphicsCommandList* _commandList);
  // A bar over each damaged ship and structure, its length the share of hit points left (task 3.5), and one over each
  // structure under construction, its length the share built (task 4.2).
  void DrawHealthBars(ID3D12GraphicsCommandList* _commandList);
  void DrawEffects(ID3D12GraphicsCommandList* _commandList);
  // Every ship's exhaust, in its drive's color, brighter and longer the faster the ship goes (ADR-019), and the particles
  // as diamonds of their sprite (ADR-026).
  void DrawGlows(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList);
  // Under fog of war, the ground the player does not see now, dimmed or dark, over everything but the HUD (ADR-024).
  void DrawFog(const Neuron::Renderer& _renderer, ID3D12GraphicsCommandList* _commandList);
  void DrawHud(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex);
  // A level band from one point to another, _widthMeters wide and _heightMeters above the ground: a drag box's edge, a
  // health bar, a tracer or a beam.
  void DrawBand(ID3D12GraphicsCommandList* _commandList, PlanePosition _from, PlanePosition _to, float _widthMeters, float _heightMeters,
                const DirectX::XMFLOAT4& _color);

  enum class Screen : std::uint8_t
  {
    Menu,
    Match
  };

  Screen m_screen = Screen::Menu;
  std::optional<Request> m_request;
  std::uint32_t m_ticksPerSecond = 0;
  ModelCatalog m_catalog;
  Camera m_camera;
  Neuron::MeshPipeline m_pipeline;
  Neuron::GlowPipeline m_glows;
  Neuron::GroundMaskPipeline m_groundMask;
  // The particles, drawn with DeepSpaceOutpost's particle texture rather than as soft spots (ADR-026).
  std::unique_ptr<Neuron::GlowPipeline> m_particleSprites;
  // The sky behind everything (ADR-022): its stars as points, and its brightest as crosses (ADR-028).
  std::unique_ptr<Neuron::StarPipeline> m_sky;
  std::unique_ptr<Neuron::StarPipeline> m_bursts;
  // This frame's glows, kept so that their storage is not allocated every frame.
  std::vector<Neuron::GlowPipeline::Glow> m_frameGlows;
  Neuron::UiPipeline m_ui;
  // The HUD as this frame draws it; the next frame's clicks are tested against it.
  Hud::Layout m_hudLayout;
  SnapshotInterpolator m_view;
  PlayerControls m_controls;
  Designer m_designer;
  // What the player has seen of the map, when the match is played under fog of war (ADR-024).
  FogOfWar m_fog;
  CombatEffects m_effects;
  // What blew up: its blast, as particles, and its shards (ADR-026).
  ParticleSystem m_particles;
  ExplosionManager m_explosions;
  // What the effects draw this frame, at the view's tick.
  std::vector<CombatEffects::Draw> m_effectDraws;
  // The particles' glows, and the explosions' shards and their batches, this frame at the view's tick.
  std::vector<Neuron::GlowPipeline::Glow> m_particleGlows;
  std::vector<Neuron::MeshVertex> m_shardVertices;
  std::vector<ExplosionManager::Batch> m_shardBatches;
  // The view's entities this frame, which the controls pick from and the renderer draws.
  std::vector<EntityView> m_entities;
  Viewport m_viewport;
  // Keyed by "<set>/<model>".
  std::map<std::string, std::unique_ptr<Neuron::Mesh>, std::less<>> m_modelMeshes;
  std::map<std::string, std::vector<Neuron::MeshHardpoint>, std::less<>> m_modelHardpoints;
  std::map<std::string, Neuron::MeshData, std::less<>> m_modelShapes;
  // How long the frame being drawn took to come, which a ship's speed is measured over.
  float m_frameSeconds = 0.0f;
  // The ground's grid, as lines.
  std::unique_ptr<Neuron::Mesh> m_grid;
  // A ring and a disc of radius 1 and a strip 1 long and 1 wide, all flat on the ground, scaled where they are drawn.
  std::unique_ptr<Neuron::Mesh> m_ring;
  std::unique_ptr<Neuron::Mesh> m_disc;
  std::unique_ptr<Neuron::Mesh> m_strip;
  // Each model's creases as a line list, keyed as m_modelMeshes; none for a mesh with no creases (ADR-027).
  std::map<std::string, std::unique_ptr<Neuron::Mesh>, std::less<>> m_modelEdges;
  // The lines of the models drawn this frame, drawn together after them.
  std::vector<Neuron::MeshPipeline::LineDraw> m_lineDraws;
  // How high each of the rock meshes reaches over its center, at a radius of 1, for a Mining Rig to stand on.
  std::array<float, 3> m_rockTops{};
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
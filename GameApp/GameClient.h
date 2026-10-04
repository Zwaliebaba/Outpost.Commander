#pragma once

namespace Outpost
{
// What the client loads from the package and builds on the CPU before it can draw (ADR-049): the model catalog, the
// camera's settings, every model's meshes and their crease lines, the starfield and the particle sprite. Making it needs
// no device, so the shell makes it on a thread of its own while the window and the device are created.
struct ClientAssets
{
  // A piece of a model (ADR-045): its triangles, their creases as lines (ADR-027), empty when there are none, and how the
  // piece spins; nothing for the faces that stand still.
  struct Piece
  {
    Neuron::MeshData faces;
    Neuron::MeshData edges;
    std::optional<Neuron::MeshPart> spin;
  };

  // A model: its pieces, and its whole mesh with its hardpoints.
  struct Model
  {
    std::vector<Piece> pieces;
    Neuron::MeshData shape;
  };

  ModelCatalog catalog;
  CameraSettings camera;
  // In the catalog's order: each set's models after the set before, and a model that grows once for each of its levels,
  // first to last (ADR-045, ADR-064).
  std::vector<Model> models;
  Starfield sky;
  Neuron::TextureData particleSprite;
};

// Reads Models.json, Camera.json, every model's mesh and the particle sprite from the package's Assets folder, and builds
// what the client draws from them. Throws Neuron::Exception naming the file when a data file or a mesh is missing or
// cannot be read, so that no model is silently left out. It may run on any thread.
[[nodiscard]] ClientAssets LoadClientAssets();

// The interface's fonts and sprites rasterized for a back buffer of this size (ADR-030). It needs no device and may run on
// any thread. Throws Neuron::Exception when none of a font's families is installed.
[[nodiscard]] Neuron::UiAtlas RasterizeInterface(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

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

  // Builds the pipelines and uploads _assets and _interface, which LoadClientAssets and RasterizeInterface made, in the
  // renderer's batch of uploads when one is open. _ticksPerSecond is the server's rate, which the snapshots are
  // interpolated at. Throws winrt::hresult_error when the device fails.
  GameClient(Neuron::Renderer& _renderer, std::uint32_t _ticksPerSecond, ClientAssets _assets, Neuron::UiAtlas _interface);

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

  // Draws the HUD, the windows or the menu over the world, after Renderer::BeginInterface (ADR-050).
  void RenderInterface(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex);

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
    // The model's level that draws it: a structure's own, as far as its model grows (ADR-064).
    int level = FIRST_MODEL_LEVEL;
    ModelPose pose;
    // The share of the set's color a structure is drawn in (StructureModel::tint).
    float tint = 1.0f;
    // How it stands on its legs, for a Mining Rig on its rock (ADR-044).
    std::optional<Stance> stance;

    // The world matrix that draws it, stood when it has a stance.
    [[nodiscard]] DirectX::XMFLOAT4X4 World() const noexcept
    {
      return stance.has_value() ? StanceMatrix(pose, *stance) : PoseMatrix(pose);
    }

    // How high its origin stands over the ground.
    [[nodiscard]] float OriginLiftMeters() const noexcept
    {
      return pose.liftMeters + (stance.has_value() ? StandPoint(*stance, {}).y : 0.0f);
    }
  };

  // A model on the GPU in pieces: the faces that stand still and each spinning part's (ADR-045), each with its creases as
  // lines (ADR-027), which are null for a piece with no creases.
  struct ModelPiece
  {
    std::unique_ptr<Neuron::Mesh> faces;
    std::unique_ptr<Neuron::Mesh> edges;
    // How the piece spins; nothing for the faces that stand still.
    std::optional<Neuron::MeshPart> spin;
  };

  // A model as the client holds it: its pieces on the GPU, its hardpoints (ADR-018), its triangles on the CPU for an
  // explosion to break (ADR-026), and for a side's Mining Rig, the feet it stands on (ADR-044).
  struct LoadedModel
  {
    std::vector<ModelPiece> pieces;
    std::vector<Neuron::MeshHardpoint> hardpoints;
    Neuron::MeshData shape;
    std::optional<std::vector<DirectX::XMFLOAT3>> feet;
  };

  // Where _level of the model _set/_model is in m_models, found by name in the catalog with no key built, since it is asked
  // for every model drawn. Throws Neuron::Exception for a model that is not loaded.
  [[nodiscard]] std::size_t ModelIndex(std::string_view _set, std::string_view _model, int _level = FIRST_MODEL_LEVEL) const;
  [[nodiscard]] const LoadedModel& LoadedModelOf(std::string_view _set, std::string_view _model, int _level = FIRST_MODEL_LEVEL) const
  {
    return m_models[ModelIndex(_set, _model, _level)];
  }
  [[nodiscard]] const std::vector<ModelPiece>& ModelPieces(std::string_view _set, std::string_view _model,
                                                           int _level = FIRST_MODEL_LEVEL) const;
  // The world matrix that draws _piece of a model drawn by _world, turned as far as its spin has gone at the view's tick.
  [[nodiscard]] DirectX::XMFLOAT4X4 PieceWorld(const ModelPiece& _piece, const DirectX::XMFLOAT4X4& _world) const noexcept;
  [[nodiscard]] const std::vector<Neuron::MeshHardpoint>& ModelHardpoints(std::string_view _set, std::string_view _model,
                                                                          int _level = FIRST_MODEL_LEVEL) const;
  // A model's triangles on the CPU, for an explosion to break (ADR-026).
  [[nodiscard]] const Neuron::MeshData& ModelShape(std::string_view _set, std::string_view _model, int _level = FIRST_MODEL_LEVEL) const;
  // Nothing for what is not a ship or a structure, or what the data does not map to a model.
  [[nodiscard]] std::optional<PlacedModel> PlaceModel(const EntityView& _entity) const;
  // Leans each ship of the view into its turn, by its bank limits, for a frame of _elapsedSeconds (ADR-029).
  void UpdateBanking(float _elapsedSeconds);
  // How a Mining Rig, the model _set/_model drawn at _scale, stands on its rock: tilted and lifted so that its legs stand on
  // the rock under them, or without feet over the rock, its lowest point on the rock's top (ADR-044).
  [[nodiscard]] Stance RigStance(std::string_view _set, std::string_view _model, const EntityView& _rig, float _scale) const;
  // The color of the shooter's beams: its side's, made lighter (ADR-028). Nothing for a shooter the view does not hold.
  [[nodiscard]] std::optional<DirectX::XMFLOAT4> BeamColor(EntityId _shooter) const;
  // The shooter's gun nearest _target where the view draws it this frame, for the combat effects (ADR-018).
  [[nodiscard]] std::optional<PlanePosition> MuzzleOf(EntityId _shooter, PlanePosition _target) const;
  // Queues _entity's model for the frame's batches of faces and lines (ADR-053).
  void QueueEntity(const EntityView& _entity);
  // Starts the blast and the explosion of every ship and structure _snapshot reports destroyed (ADR-026), before the view
  // takes the snapshot, so that the view still holds what blew up.
  void Explode(const Snapshot& _snapshot);
  // The shards of every explosion, as ExplosionManager gives them for the view's tick.
  void DrawShards(ID3D12GraphicsCommandList* _commandList);
  // The model _set/_model placed by _world: its faces _color at _fillShade, queued for the frame's batch of faces
  // (ADR-053), and its creases over them as lines, lighter, queued for the frame's one pass of lines (ADR-027, ADR-040).
  void QueueModel(std::string_view _set, std::string_view _model, const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4& _color,
                  float _fillShade, int _level = FIRST_MODEL_LEVEL);
  // A structure queued at its footprint, grayer and darker than a ship (ADR-040), and darker still while it is built
  // (task 4.2).
  void QueueStructure(const EntityView& _entity);
  // The structure being placed, at the cursor, green where it may stand and red where it may not.
  void DrawGhost(ID3D12GraphicsCommandList* _commandList);
  // Presses on the HUD's buttons and minimap, which the controls never see; and a drag on the minimap moves the camera.
  // A press on a window brings it to the front: on its close box it closes it, and on its title bar it drags it.
  void HandleHudInput(const Neuron::InputState& _input);
  // A point on the back buffer in the HUD's reference units.
  [[nodiscard]] WindowManager::Point ToUnits(float _xPixels, float _yPixels) const noexcept;
  // What a HUD button does: arms a placement, queues a job or a topic, or works the designer.
  void HandleHudAction(const Hud::Action& _action);
  // Forgets the match being shown.
  void ClearMatch();
  // While the designer's name takes typing, the keyboard is the designer's: _input loses its keys, so no order, control
  // group or camera key reads them.
  void HandleTyping(Neuron::InputState& _input);
  // Opens a window, aimed at the structure selected on its own if it shows one, or closes it.
  void ToggleWindow(WindowKind _window);
  // The ground the camera shows, its corners in order, for the minimap; empty when a corner sees past the horizon.
  [[nodiscard]] std::vector<PlanePosition> ViewOnGround() const;
  // A faint ring, one pixel wide, in its side's color under every structure that is not selected, fading as the camera comes
  // in, and at full strength under the one the pointer is on and under all of them while a structure is placed; a Mining
  // Rig's only then (ADR-042, ADR-046).
  void DrawFootprints(ID3D12GraphicsCommandList* _commandList);
  // The ring of a Mining Rig's own footprint, laid over its asteroid's rock, into m_drapedRing as a line list in the world
  // (ADR-042); false when the rig's kind is not known.
  bool DrapeRigRing(const EntityView& _rig);
  void DrawSelection(ID3D12GraphicsCommandList* _commandList);
  // A bar over each damaged ship and structure, or every one while Alt is held, its length the share of hit points left
  // (task 3.5), and one over each structure under construction, its length the share built (task 4.2). Neither is smaller
  // on screen than a least size (ADR-047).
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
  // What the player has seen of the map, when the match is played under fog of war (ADR-024); the newest snapshot's tick it
  // was brought up to date at, and the revision of it the ground mask's texture holds (ADR-052).
  FogOfWar m_fog;
  // What the player is alerted to, from every snapshot (ADR-059).
  Alerts m_alerts;
  std::optional<std::uint64_t> m_fogTick;
  std::optional<std::uint64_t> m_fogRevisionShown;
  // The floating windows, which keep their places for as long as the game runs (ADR-031); and what was selected on its own
  // last frame, so that selecting a Shipyard aims the open designer at it once and the arrows can step on from there.
  WindowManager m_windows;
  EntityId m_soleSelected;
  // The producer the production window shows, and the first topic the research window shows (Phase 1 design §12).
  ProductionTarget m_production;
  std::size_t m_firstTopic = 0;
  // How far each ship leans into its turn as it is drawn, and this frame's targets, kept so that their storage is not
  // allocated every frame (ADR-029).
  ShipBanking m_banking;
  std::vector<ShipBanking::Target> m_bankTargets;
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
  // The view's entities this frame, which the renderer draws; and those the controls are given, less the asteroids in space
  // the player has never seen (ADR-046).
  std::vector<EntityView> m_entities;
  std::vector<EntityView> m_knownEntities;
  Viewport m_viewport;
  // Every set's models in the catalog's order, each set's after the one before.
  std::vector<LoadedModel> m_models;
  // How long the frame being drawn took to come, which a ship's speed is measured over.
  float m_frameSeconds = 0.0f;
  // The ground's grid, as lines.
  std::unique_ptr<Neuron::Mesh> m_grid;
  // A ring and a disc of radius 1 and a strip 1 long and 1 wide, all flat on the ground, scaled where they are drawn; and
  // the ring as a line, a structure's footprint (ADR-042).
  std::unique_ptr<Neuron::Mesh> m_ring;
  std::unique_ptr<Neuron::Mesh> m_ringLine;
  std::unique_ptr<Neuron::Mesh> m_disc;
  std::unique_ptr<Neuron::Mesh> m_strip;
  // The faces of the models drawn this frame, drawn together with each mesh's copies in one draw (ADR-053); their lines,
  // drawn together after them; and the structures' rings.
  std::vector<Neuron::MeshPipeline::MeshDraw> m_faceDraws;
  std::vector<Neuron::MeshPipeline::MeshDraw> m_lineDraws;
  std::vector<Neuron::MeshPipeline::MeshDraw> m_ringDraws;
  // A Mining Rig's ring over its rock, made afresh for each rig as it is drawn.
  std::vector<Neuron::MeshVertex> m_drapedRing;
  // How high each of the rock meshes reaches over its center, at a radius of 1, for a Mining Rig to stand on.
  std::array<float, 3> m_rockTops{};
  bool m_cameraPlaced = false;
  // Alt is held this frame, and every ship and structure shows its health bar.
  bool m_everyHealthBar = false;
  // The left button went down on the minimap and is still held.
  bool m_minimapDragging = false;
  // Where the cursor points on the ground this frame, for the ghost, and the structure it is on, whose ring shows at full
  // strength.
  std::optional<PlanePosition> m_cursorGround;
  std::optional<EntityId> m_hovered;

  // The move order being watched: its ships as the view showed them when it was given.
  struct ResponseProbe
  {
    std::vector<EntityView> ships;
    std::chrono::steady_clock::time_point inputRead;
  };

  void WatchForResponse();
  // The last frame's entities, in identifier order like m_entities. The two trade storage each frame, so that neither is
  // allocated again.
  std::vector<EntityView> m_previousEntities;
  std::optional<ResponseProbe> m_probe;
  std::optional<std::chrono::steady_clock::time_point> m_responseShown;
};
} // namespace Outpost
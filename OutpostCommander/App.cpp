#include "pch.h"

#include "WindowsHeader.h"

#include "App.h"

#include "Hud.h"
#include "Match.h"
#include "Operator.h"

#include "Camera.h"
#include "CameraController.h"
#include "CapturePath.h"
#include "ContentHash.h"
#include "ContentLoader.h"
#include "CursorPass.h"
#include "FogPass.h"
#include "FrameCapture.h"
#include "FrameInput.h"
#include "FrameTimer.h"
#include "GeometryPass.h"
#include "GraphicsDevice.h"
#include "HeightView.h"
#include "InputQueue.h"
#include "InputRouter.h"
#include "LandscapeDefinition.h"
#include "Lighting.h"
#include "Log.h"
#include "RenderView.h"
#include "MatchSettings.h"
#include "ModelBuffers.h"
#include "Movement.h"
#include "Paths.h"
#include "PlacementPreview.h"
#include "PresentPass.h"
#include "PointerMode.h"
#include "ScaleMode.h"
#include "SceneTarget.h"
#include "SwapChain.h"
#include "TerrainChunk.h"
#include "TerrainPass.h"
#include "TextureFile.h"
#include "UiDraw.h"
#include "UiPass.h"
#include "WaterPass.h"
#include "Window.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace Outpost
{

namespace
{

// The Species sky is the clear colour, black (SpeciesLook.md §6); the terrain draws over it.
constexpr std::array<float, 4> CLEAR_COLOR = {0.0f, 0.0f, 0.0f, 1.0f};
/// A FRAME EVERY HUNDRED TICKS and not every hundred frames (m1-vertical-slice/G2): the capture is
/// a tick count now, so five seconds of match time is what separates one BMP from the next however
/// long WARP took to draw them.
constexpr std::uint32_t CAPTURE_EVERY_TICKS = 100;
constexpr float PI = 3.14159265358979323846f;

/// The capture's script, in world units and ticks. The framing is the 60-degree vertical field of
/// view of Client/Camera.h: at a distance d the frame is 1.155 d high and 2.05 d wide at the aim
/// plane, so a base fifteen cells across (960 units) wants an eye about a thousand units off it and
/// a landscape 8,192 across wants four thousand.
constexpr float ORBIT_RADIUS_FRACTION = 0.45f;
constexpr float ORBIT_ELEVATION = 1800.0f;
constexpr float BASE_SETBACK = 900.0f;
constexpr float BASE_ELEVATION = 700.0f;
constexpr float FIGHT_SETBACK = 1100.0f;
constexpr float FIGHT_ELEVATION = 850.0f;
/// Where the fallback vantage for the fighting sits, along the line from this commander's start to
/// the other's. A quarter, measured: on the slice landscape the two armies met 461 world units from
/// that point, which the frame at this distance holds comfortably (m1-vertical-slice/S14).
constexpr float MEETING_FRACTION = 0.25f;
/// The tick ADR-005's pair of frames is drawn at: the end of the opening sweep, which is the one
/// vantage of the capture with a horizon far enough away for the two fog modes to differ. The
/// capture writes that frame twice, once under each, from one pose at one tick.
constexpr std::uint32_t COMPARISON_TICK = 800;
/// The seat the capture's client watches. Seat 0 because that is the commander the script's
/// vantages are written round - its base is the one held, and "the ground between the two" is
/// measured from its start.
constexpr std::uint8_t WATCHED_SEAT = 0;
/// How wide a frame's tick is written in its name, so that a listing sorts the ninety BMPs into
/// the order the match ran in.
constexpr std::size_t FRAME_NAME_DIGITS = 5;

/// Where the camera sits over the commander's base at the first frame, in world units: far enough
/// back that the command post and the ground around it are both in the frame, and high enough that
/// the look-down angle reads as an RTS camera rather than a chase one. Not a rule anywhere - the
/// design gives the camera's rates (SpeciesLook.md §7) and not its opening pose - so these are
/// this task's choice and G3's run is where the owner says whether they are right.
/// How long the WINDOW waits for the loopback join before it says so, and how many TICKS the
/// capture gives it. The capture's is a tick count rather than a duration because its host runs
/// when the loop says so: a wall-clock timeout there would be a different number of passes on a
/// fast machine and a slow one, and a run that did not reproduce.
constexpr std::chrono::seconds JOIN_WAIT{10};
constexpr std::uint32_t JOIN_WAIT_TICKS = 200;

/// F1, which "toggles the panel overlay off, for a clean look at the world" (Design/Interface.md
/// §7's hotkey table). Here rather than beside the Hud because it is the window's own view of the
/// interface and not something a panel knows about.
constexpr std::uint8_t KEY_PANEL_TOGGLE = 0x70;

constexpr float CAMERA_SETBACK = 420.0f;
constexpr float CAMERA_ELEVATION = 340.0f;

/// The landscape the slice is played on, from the content tree by its file stem
/// (m1-vertical-slice/G1a). Null when GameData carries no such file, which is a content fault and
/// not something to substitute a built-in for: M0's hard-coded recipe is gone, and a match played
/// on a landscape nobody authored is a match whose bases stand where nobody put them.
[[nodiscard]] const LandscapeDefinition* LandscapeNamed(const ContentTree& _content, std::string_view _id) noexcept
{
  for (std::size_t index = 0; index < _content.landscapeIds.size() && index < _content.landscapes.size(); ++index)
  {
    if (_content.landscapeIds[index] == _id)
    {
      return &_content.landscapes[index];
    }
  }
  return nullptr;
}

/// The tables a match is played by (OpenQuestions.md Q20), read once from the game-data directory
/// beside the executable. A tree that does not load is returned EMPTY and logged, and the caller
/// refuses the match: C2's tables are authored now, so an empty tree is GameData missing from
/// beside the executable rather than content nobody has written yet, and a match on no tables has
/// no command post to start either commander with.
[[nodiscard]] const Outpost::ContentTree& MatchContent()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree{};
    const std::filesystem::path directory = Neuron::Paths::GameDataDirectory();
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    if (Outpost::LoadContent(directory, tree, diagnostics))
    {
      Neuron::Log::Write(Neuron::LogLevel::Info, "content: " + std::to_string(tree.components.chassis.size()) + " chassis, " +
                                                   std::to_string(tree.structures.structures.size()) + " structures, " +
                                                   std::to_string(tree.research.size()) + " research items from " + directory.string());
      return tree;
    }
    const std::string reason = diagnostics.empty() ? std::string("no diagnostic") : diagnostics.front().message;
    Neuron::Log::Write(Neuron::LogLevel::Warning,
                       "content: no tables at " + directory.string() + " (" + reason + "); the match runs on none");
    return Outpost::ContentTree{};
  }();
  return TREE;
}

/// The fixed lobby of M1, until M2's lobby lets the owner choose: this commander in seat 0 and the
/// scripted AI of S12 in seat 1, at the defaults GameDesign.md §2 names.
///
/// _bothScripted is the capture's lobby: TWO AI SEATS, which the observer connection of G2 is what
/// makes drawable at all. Net/Host.cpp's FreeSeat hands a joining client only a seat whose kind is
/// Human, so an all-AI lobby refuses an ordinary join with NoSeat and leaves no replica to draw
/// from - measured at G1a, six failures and an empty view. The owner ruled on 2026-09-19 that a
/// client may join to WATCH a seat instead; the capture does that, sees exactly what the commander
/// it watches sees, and gives no orders.
[[nodiscard]] MatchSettings Lobby(bool _bothScripted = false)
{
  MatchSettings settings{};
  settings.seed = 1;
  settings.sizeClass = SizeClass::Small;
  settings.seatCount = 2;
  // "Nothing is a builder and a command post" (GameDesign.md §2). It is the only level
  // OutpostCommander/StartingBase.h places; Small and Established arrive with the lobby that can
  // ask for them.
  settings.baseLevel = BaseLevel::Nothing;
  settings.powerLevel = PowerLevel::Medium;
  settings.technologyTiers = 0;
  settings.victory = VictoryCondition::Annihilation;
  settings.survivalTicks = 0;
  settings.deviceCapLevel = DeviceCapLevel::Medium; // 200 devices, the figure GameDesign.md §4 names
  settings.rejoinGraceTicks = 400;
  for (SeatSettings& seat : settings.seats)
  {
    seat = {SeatKind::Empty, NO_ALLIANCE};
  }
  // A SCRIPTED SEAT IS SET UP WITH AUTO-RESEARCH ON, which is Sim/AiSeat.h's contract in as many
  // words: "Research is the seat's own autoResearch flag, so there is no research behaviour here
  // and an AI seat is set up with it on." This lobby did not do it, and the scripted commander was
  // therefore locked out of the whole research table for the entire match.
  //
  // IT MADE THE GAME UNWINNABLE BY THE AI AND NOBODY SAW IT, because the one configuration nothing
  // tested was this one: Tests/SimTests/AiTests.cpp's fixture sets the flag, so every suite ran a
  // commander that researches, and the executable ran one that does not. Measured on the slice
  // landscape at seed 1 (m1-vertical-slice/S15): without it the commander is stuck on the machine
  // gun - the only weapon unlocked from the first tick - and a machine gun deals EXACTLY NOTHING to
  // a command post, eight damage taken to two by the anti-light matrix's thirty percent against
  // Hard, then all of it by the post's twenty kinetic armour, with a floor of a third of two that
  // is nought in whole points. Fifty simulated minutes, the enemy field swept, fifteen hundred hit
  // points untouched. With the flag the same match is decided in 13,274 ticks.
  settings.seats[0] = {_bothScripted ? SeatKind::Ai : SeatKind::Human, 0, _bothScripted};
  settings.seats[1] = {SeatKind::Ai, 1, true};
  return settings;
}

/// Where the camera starts: over this commander's own base, high enough to see it and the ground
/// it has to expand into, looking down and inward toward the middle of the landscape so that the
/// first frame is the picture a player expects rather than a corner of sea.
void PoseOverBase(Neuron::Camera& _camera, const CellPosition& _start, float _extent, float _groundHeight) noexcept
{
  const float baseX = (static_cast<float>(_start.x) + 0.5f) * static_cast<float>(Neuron::WORLD_UNITS_PER_CELL);
  const float baseZ = (static_cast<float>(_start.y) + 0.5f) * static_cast<float>(Neuron::WORLD_UNITS_PER_CELL);
  const float center = _extent * 0.5f;
  // Back off along the line from the middle of the landscape to the base, so that "inward" is the
  // same direction whichever corner the seat starts in.
  const float awayX = baseX - center;
  const float awayZ = baseZ - center;
  const float distance = std::sqrt(awayX * awayX + awayZ * awayZ);
  const float unitX = distance > 1.0f ? awayX / distance : 0.0f;
  const float unitZ = distance > 1.0f ? awayZ / distance : -1.0f;
  _camera.SetPosition(baseX + unitX * CAMERA_SETBACK, _groundHeight + CAMERA_ELEVATION, baseZ + unitZ * CAMERA_SETBACK);
  _camera.LookAt(baseX, _groundHeight, baseZ);
}

/// The renderer's view over the simulation's landscape (TechnicalDesign.md §6.3): the one place
/// a Sim type meets a Client one.
[[nodiscard]] Neuron::HeightView ViewOf(const Landscape& _landscape) noexcept
{
  std::int32_t highest = 1;
  for (const std::int16_t sample : _landscape.Heights())
  {
    highest = std::max<std::int32_t>(highest, sample);
  }
  return {_landscape.Heights().data(), _landscape.SamplesPerSide(), SAMPLE_SPACING_WORLD_UNITS, 0, highest};
}

/// The landscape's palette, from GameData\Terrain, falling back to the built-in gradient when the
/// game data is not beside the executable or the file is not a 64 by 64 texture. Nothing in the
/// build puts it there (owner, 2026-09-18), so the fallback is the ordinary case until it does and
/// the log says which was used: a frame coloured by the gradient and a frame coloured by the
/// authored palette are different pictures and nobody should have to guess which they are reading.
///
/// The biome names its palette (GameData\Biomes.json, m1-vertical-slice/C2) and this reads the
/// default directly until the tables exist.
[[nodiscard]] Neuron::TerrainPalette LoadTerrainPalette()
{
  const std::filesystem::path file = Neuron::Paths::GameDataDirectory() / "Terrain" / "LandscapeDefault.dds";
  std::ifstream stream(file, std::ios::binary);
  if (stream)
  {
    const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Neuron::TextureFile texture;
    std::string error;
    Neuron::TerrainPalette palette;
    if (!Neuron::TextureFile::Read(std::as_bytes(std::span<const char>(bytes.data(), bytes.size())), texture, error))
    {
      Neuron::Log::Write(Neuron::LogLevel::Warning, "palette: " + file.string() + " was refused: " + error);
    }
    else if (!Neuron::TerrainPalette::FromTexture(texture, palette))
    {
      Neuron::Log::Write(Neuron::LogLevel::Warning, "palette: " + file.string() + " is not a 64 by 64 palette");
    }
    else
    {
      Neuron::Log::Write(Neuron::LogLevel::Info, "palette: " + file.string());
      return palette;
    }
  }
  else
  {
    Neuron::Log::Write(Neuron::LogLevel::Info, "palette: no " + file.string());
  }
  Neuron::Log::Write(Neuron::LogLevel::Info, "palette: the built-in gradient");
  return Neuron::TerrainPalette::BuiltIn();
}

/// The three passes that cannot exist until there is a landscape, held as ONE object because they
/// are born together and die together: each is built from the same heights and the same sample
/// count, and there is no state in which one of them is the right thing to draw without the others.
///
/// M0 BUILT THESE AT STARTUP AND M1 CANNOT. The landscape arrives with the join (Match.h), which is
/// a datagram answered by a thread, so there are frames - one over loopback, a second or more over
/// a network - in which the device, the swap chain and the scene target exist and the terrain does
/// not. The window therefore holds this in an optional and presents the cleared target, which is
/// the Species sky, until it is filled: a window that stays black until a join lands is
/// indistinguishable from one that hung. The capture holds it as a plain object, because it waits
/// for the join before it builds anything at all.
struct MatchPasses
{
  MatchPasses(Neuron::GraphicsDevice& _device, const Neuron::SceneTarget& _scene, const Neuron::HeightView& _heights,
              std::uint32_t _cellsPerSide)
    : terrain(_device, _heights, LoadTerrainPalette(), _scene.SampleCount()),
      water(_device, _heights, _scene.SampleCount()),
      fog(_device, _scene, _cellsPerSide)
  {
  }

  Neuron::TerrainPass terrain;
  Neuron::WaterPass water;
  Neuron::FogPass fog;
};

/// Pushes the far plane out for this landscape and puts the camera over the commander's base.
/// Reads the CLIENT'S landscape (Match::Terrain) and never the host's, which is the fog boundary of
/// TechnicalDesign.md §5.2.
void AimAtBase(Neuron::Camera& _camera, const Match& _match, const CellPosition& _start, float _extent)
{
  _camera.SetFarPlane(_extent * Neuron::FAR_PLANE_EXTENT_FACTOR);
  // The ground the base actually stands on, read off the commander's own landscape rather than
  // guessed from the highest sample: a camera parked at half the island's height is underground on
  // a peak and in orbit over a beach.
  const std::int32_t baseSubunitsX = static_cast<std::int32_t>(_start.x) * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2;
  const std::int32_t baseSubunitsZ = static_cast<std::int32_t>(_start.y) * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2;
  const float ground = Neuron::WorldUnitsOfSubunits(GroundHeightSubunits(_match.Terrain(), baseSubunitsX, baseSubunitsZ));
  PoseOverBase(_camera, _start, _extent, ground);
}

/// The fog range is the landscape's no longer (ADR-007): it is absolute, so a Frontier landscape's
/// horizon reads exactly as a Small one's.
[[nodiscard]] Neuron::TerrainPass::Frame FrameOf(Neuron::FogMode _fog) noexcept
{
  Neuron::TerrainPass::Frame frame{};
  frame.aspect = static_cast<float>(Neuron::AUTHORED_WIDTH_PIXELS) / static_cast<float>(Neuron::AUTHORED_HEIGHT_PIXELS);
  frame.lighting = Neuron::BUILT_IN_LIGHTING;
  frame.fogMode = _fog;
  frame.fogStart = Neuron::FOG_START_WORLD_UNITS;
  frame.fogEnd = Neuron::FOG_FULL_WORLD_UNITS;
  frame.fogMaxDesaturation = Neuron::FOG_MAX_DESATURATION;
  frame.fogColor = {0.0f, 0.0f, 0.0f};
  return frame;
}

/// How far above the ground the footprint ghost floats, in world units. It is not a depth bias:
/// the ghost is a flat quad over ground that is not flat, so a corner of it can be under a rise
/// that its middle clears, and a bias cannot answer that. One unit is under the eye at any camera
/// height this game has and is enough for the cells at the edges of a footprint on rough ground.
constexpr float GHOST_LIFT_WORLD_UNITS = 1.0f;

/// Whether a structure of this row may stand with its lowest cell here, AS THIS COMMANDER KNOWS
/// IT: his own structures, his own fog, his own landscape and his own deposits
/// (Replica/PlacementPreview.h). The host still decides; a green ghost means "nothing I know of
/// refuses this", and the refusal that comes back is what the warning line is for.
[[nodiscard]] bool MayStandHere(const Match& _match, const ContentTree& _content, std::uint32_t _row, std::int32_t _cellX,
                                std::int32_t _cellZ)
{
  if (_cellX < 0 || _cellZ < 0)
  {
    return false; // Off the landscape, which PreviewPlacement takes unsigned cells and cannot say.
  }
  PreviewQuery query{};
  query.structures = &_match.Commander().Structures();
  query.fog = _match.Commander().Fog();
  query.fogCellsPerSide = _match.Commander().FogCellsPerSide();
  query.landscape = &_match.Terrain();
  query.content = &_content;
  query.deposits = &_match.Deposits();
  return PreviewPlacement(_row, static_cast<std::uint32_t>(_cellX), static_cast<std::uint32_t>(_cellZ), query) == PlacementFault::Accepted;
}

/// The six meanings of Design/Interface.md §4's cursor table, as the tint the ring is drawn in
/// rather than as six bitmaps: "the six meanings above still apply - they choose the ring's TINT".
/// M1 needs three of them, which are the three a commander can be in without a panel.
constexpr std::array<float, 4> CURSOR_DEFAULT{1.0f, 1.0f, 1.0f, 1.0f};
constexpr std::array<float, 4> CURSOR_BUILD{0.45f, 1.0f, 0.55f, 1.0f};
constexpr std::array<float, 4> CURSOR_REFUSE{1.0f, 0.35f, 0.3f, 1.0f};
/// How the placement cursor pulses (§4's table): size x (1 + |sin 4t| x 0.6). Species animates
/// exactly the placement and move-here cursors, and a placement is the one thing in M1 that wants
/// the eye.
constexpr float PULSE_RADIANS_PER_SECOND = 4.0f;
constexpr float PULSE_DEPTH = 0.6f;

/// A DDS beside the executable, decoded to RGBA8, or an empty image with the reason logged. The
/// cursor draws a white square from an empty one, which says "the ring is missing" rather than
/// leaving a frame with no cursor at all.
[[nodiscard]] std::vector<std::uint8_t> LoadRing(const char* _file, std::uint32_t& _outWidth, std::uint32_t& _outHeight)
{
  _outWidth = 0;
  _outHeight = 0;
  const std::filesystem::path path = Neuron::Paths::GameDataDirectory() / "Textures" / _file;
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, std::string("cursor: no ") + path.string());
    return {};
  }
  const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  Neuron::TextureFile file;
  std::string error;
  if (!Neuron::TextureFile::Read(std::as_bytes(std::span<const char>(bytes.data(), bytes.size())), file, error))
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, "cursor: " + path.string() + " was refused: " + error);
    return {};
  }
  std::vector<std::uint8_t> pixels;
  if (!file.DecodeRgba8(0, pixels))
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, "cursor: " + path.string() + " could not be decoded to RGBA8");
    return {};
  }
  _outWidth = file.Width();
  _outHeight = file.Height();
  Neuron::Log::Write(Neuron::LogLevel::Info,
                     "cursor: " + path.string() + ", " + std::to_string(_outWidth) + "x" + std::to_string(_outHeight));
  return pixels;
}

/// The camera as Replica's picking reads it (Replica/Picking.h): the matrix that projects and its
/// inverse, ROW MAJOR, over the AUTHORED frame and not the window's - every rectangle of the
/// interface is authored and a click arrives converted, so picking at the window's size would land
/// a ray somewhere else on every display but one.
///
/// IT IS HERE AND NOT IN EITHER LIBRARY because it is exactly where the two meet: Client's camera
/// is DirectXMath and Replica may not include Client at all (ADR-001 makes them siblings), so the
/// sixteen floats are the seam and this is the one place they are filled.
[[nodiscard]] PickCamera PickCameraOf(const Neuron::Camera& _camera)
{
  const float aspect = static_cast<float>(Neuron::AUTHORED_WIDTH_PIXELS) / static_cast<float>(Neuron::AUTHORED_HEIGHT_PIXELS);
  const DirectX::XMMATRIX viewProjection = DirectX::XMMatrixMultiply(_camera.View(), _camera.Projection(aspect));
  DirectX::XMFLOAT4X4 stored{};
  DirectX::XMStoreFloat4x4(&stored, viewProjection);
  PickCamera camera{};
  camera.frameWidth = static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS);
  camera.frameHeight = static_cast<std::int32_t>(Neuron::AUTHORED_HEIGHT_PIXELS);
  std::memcpy(camera.viewProjection.m.data(), &stored, sizeof stored);
  DirectX::XMStoreFloat4x4(&stored, DirectX::XMMatrixInverse(nullptr, viewProjection));
  std::memcpy(camera.inverseViewProjection.m.data(), &stored, sizeof stored);
  return camera;
}

/// What the panels are shown, read off the commander and the match as they stand NOW. Written once
/// and called twice a frame - before the panels act on last frame's click, and again before they
/// are rebuilt for this one - because the two readings are the same six lines and a second copy of
/// them is a second place for the selection and what is armed to fall out of step.
void FillPanelFrame(Hud::Frame& _frame, const Operator& _commander, const Match& _match, const Neuron::Camera& _camera)
{
  _frame.selected = _commander.Abilities();
  _frame.tick = _match.Commander().NewestTick();
  _frame.camera = PickCameraOf(_camera);
  _frame.armed = _commander.Armed();
  _frame.armedStructure = _commander.ArmedStructure();
}

/// An atlas read from GameData, or an empty one with the reason logged. The interface draws nothing
/// without them and that is a picture rather than a hang: a window with no text is obviously wrong,
/// and a window that refused to open because a DDS was missing tells nobody anything.
template <typename Atlas> [[nodiscard]] Atlas LoadAtlas(const char* _file)
{
  Atlas atlas{};
  const std::filesystem::path path = Neuron::Paths::GameDataDirectory() / "Textures" / _file;
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, std::string("interface: no ") + path.string() + "; it draws nothing");
    return atlas;
  }
  const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  std::string error;
  if (!atlas.Load(std::as_bytes(std::span<const char>(bytes.data(), bytes.size())), error))
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, "interface: " + path.string() + " was refused: " + error);
    return Atlas{};
  }
  Neuron::Log::Write(Neuron::LogLevel::Info, "interface: " + path.string());
  return atlas;
}

/// A tick as a frame's name, zero-padded, so that a directory listing and a file browser both sort
/// the ninety BMPs into the order the match ran in. "frame-9000" beside "frame-800" does not.
[[nodiscard]] std::string FiveDigits(std::uint32_t _tick)
{
  std::string digits = std::to_string(_tick);
  return digits.size() >= FRAME_NAME_DIGITS ? digits : std::string(FRAME_NAME_DIGITS - digits.size(), '0') + digits;
}

/// How many shots are in the air in a view. The acceptance line G2 took from m1-vertical-slice/C8
/// is that at least one frame shows one, and this is what the log says so with: an agent reads the
/// job log and cannot open a BMP.
[[nodiscard]] std::uint32_t CountProjectiles(const Neuron::RenderView& _view) noexcept
{
  std::uint32_t count = 0;
  for (const Neuron::RenderInstance& instance : _view.instances)
  {
    count += instance.kind == Neuron::RenderInstanceKind::Projectile ? 1 : 0;
  }
  return count;
}

/// A world-unit coordinate as the simulation's subunits, for reading the ground under a vantage.
/// The landscape is the simulation's and is sampled in its own numbers; this is the one direction
/// Core/RenderView.h's WorldUnitsOfSubunits does not go.
[[nodiscard]] std::int32_t SubunitsOfWorldUnits(float _worldUnits) noexcept
{
  return static_cast<std::int32_t>(_worldUnits * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT));
}

/// Where the eye sits round an aim point, given the direction it should sit IN from it: the
/// bearing Client/CapturePath.h measures from due -Z toward +X. A vantage says "over the base,
/// from outside" or "from this commander's own ground", and this is what turns that into a
/// number. Due -Z of the aim is zero, due +X a quarter turn.
[[nodiscard]] float BearingOfEyeFrom(float _awayX, float _awayZ) noexcept
{
  return std::atan2(_awayX, -_awayZ);
}

/// THE CAPTURE'S SCRIPT (m1-vertical-slice/G2), built from the landscape rather than written in
/// world units, so that it says the same thing on any landscape with two starts.
///
/// WHAT EACH VANTAGE IS FOR, and why it is at the tick it is. Measured on the slice landscape at
/// seed 1 with both seats scripted (m1-vertical-slice/S14): the two commanders start at cells
/// (36, 92) and (108, 20), build until they have an army, acquire on tick 6,908 and fire first on
/// 6,980; the fighting sits around cell (58, 80) from tick 7,000 to 8,500 and then falls back onto
/// seat 0's own base from 8,700. So:
///
///   the island, in a half orbit from high up   ticks     0 - 800   ten frames of what the match is on
///   down onto the commander's own base         ticks   800 - 1,200 four frames of the descent
///   the base, held                             ticks 1,200 - 6,000 the base grows and sends devices out
///   out to the ground the two armies meet on   ticks 6,000 - 6,900 nine frames across the landscape
///   the meeting ground, held                   ticks 6,900 - 8,400 the fighting, and where it is
///   back over the commander's own base         ticks 8,400 - 9,000 where the fighting ends up
///
/// AND THE LAST TWO FOLLOW. The script cannot know where a fight is - the armies walk around
/// terrain rather than through it, and a quarter of the way along the line between the two starts
/// is 461 world units from where they actually met - so those vantages take their aim from the
/// shots this commander can see when there are any (Client/CapturePath.h's ActionCenter).
[[nodiscard]] std::vector<Neuron::CaptureVantage> ScriptFor(const LandscapeDefinition& _landscape, float _extent)
{
  const float center = _extent * 0.5f;
  const auto worldOf = [](const CellPosition& _cell, float& _outX, float& _outZ) noexcept
  {
    _outX = (static_cast<float>(_cell.x) + 0.5f) * static_cast<float>(Neuron::WORLD_UNITS_PER_CELL);
    _outZ = (static_cast<float>(_cell.y) + 0.5f) * static_cast<float>(Neuron::WORLD_UNITS_PER_CELL);
  };
  float homeX = center;
  float homeZ = center;
  float awayX = center;
  float awayZ = center;
  if (!_landscape.starts.empty())
  {
    worldOf(_landscape.starts.front(), homeX, homeZ);
  }
  if (_landscape.starts.size() > 1)
  {
    worldOf(_landscape.starts[1], awayX, awayZ);
  }
  // A quarter of the way to the enemy: the fallback the follow overrides when anything is in the
  // air, and on its own still a vantage that looks at the ground between the two commanders.
  const float meetX = homeX + (awayX - homeX) * MEETING_FRACTION;
  const float meetZ = homeZ + (awayZ - homeZ) * MEETING_FRACTION;
  const float outward = BearingOfEyeFrom(homeX - center, homeZ - center);
  const float fromHome = BearingOfEyeFrom(homeX - meetX, homeZ - meetZ);

  std::vector<Neuron::CaptureVantage> script;
  const auto add = [&script](std::uint32_t _tick, float _aimX, float _aimZ, float _bearing, float _setback, float _elevation, bool _follow,
                             Neuron::FogMode _fog)
  {
    Neuron::CaptureVantage vantage{};
    vantage.tick = _tick;
    vantage.aimX = _aimX;
    vantage.aimZ = _aimZ;
    vantage.bearingRadians = _bearing;
    vantage.setbackWorldUnits = _setback;
    vantage.elevationWorldUnits = _elevation;
    vantage.follow = _follow;
    vantage.fog = _fog;
    script.push_back(vantage);
  };
  // The opening sweep is under the Species fog and everything after it under the desaturation,
  // which is the pair ADR-005 rests on: the sweep is the only part of a capture with a horizon far
  // enough away for the two to look different, and the controlled comparison - one pose, one tick,
  // both modes - is the extra frame RunCapture writes at COMPARISON_TICK.
  add(0, center, center, 0.0f, _extent * ORBIT_RADIUS_FRACTION, ORBIT_ELEVATION, false, Neuron::FogMode::LinearToColor);
  add(COMPARISON_TICK, center, center, PI, _extent * ORBIT_RADIUS_FRACTION, ORBIT_ELEVATION, false, Neuron::FogMode::LinearToColor);
  add(1200, homeX, homeZ, outward, BASE_SETBACK, BASE_ELEVATION, false, Neuron::FogMode::Desaturation);
  add(6000, homeX, homeZ, outward, BASE_SETBACK, BASE_ELEVATION, false, Neuron::FogMode::Desaturation);
  add(6900, meetX, meetZ, fromHome, FIGHT_SETBACK, FIGHT_ELEVATION, true, Neuron::FogMode::Desaturation);
  add(8400, meetX, meetZ, fromHome, FIGHT_SETBACK, FIGHT_ELEVATION, true, Neuron::FogMode::Desaturation);
  add(8700, homeX, homeZ, outward, FIGHT_SETBACK, FIGHT_ELEVATION, true, Neuron::FogMode::Desaturation);
  return script;
}

/// One frame of the world, in the order TechnicalDesign.md §6.2 gives: the ground, the water over
/// it, the commanders' things on it, and the fog over everything. Shared by the window and the
/// capture so that the two cannot drift into showing different pictures of the same match - which
/// they would, because the capture is the only one anybody reviews.
void DrawMatch(ID3D12GraphicsCommandList* _list, const Neuron::SceneTarget& _scene, MatchPasses& _passes, Neuron::GeometryPass& _geometry,
               const Neuron::Camera& _camera, const Match& _match, Neuron::FogMode _fog)
{
  const Neuron::TerrainPass::Frame frame = FrameOf(_fog);
  _passes.terrain.Draw(_list, _camera, frame);
  // AFTER the draw and not before: TerrainPass::Draw is what writes this frame's constants and sets
  // the address to the slot it wrote, so an address read first is the previous frame's.
  const D3D12_GPU_VIRTUAL_ADDRESS constants = _passes.terrain.ConstantsAddress();
  _passes.water.Draw(_list, constants);
  _geometry.Draw(_list, constants, _match.View().instances);
  _passes.fog.Draw(_list, _scene, _camera, frame.aspect, _match.View().fog);
}

[[nodiscard]] std::string Describe(const winrt::hresult_error& _error)
{
  char code[16];
  std::snprintf(code, sizeof code, "0x%08X", static_cast<unsigned>(static_cast<std::int32_t>(_error.code())));
  return std::string(code) + " " + winrt::to_string(_error.message());
}

[[nodiscard]] int ExitCodeOf(const Neuron::GraphicsDevice& _device)
{
  return _device.DebugMessageCount() == 0 ? EXIT_CLEAN : EXIT_DEBUG_MESSAGES;
}

/// Microseconds as milliseconds to two places, for the title bar. Fixed to two places rather than
/// left to the default formatting, so that the three figures line up as the numbers move.
[[nodiscard]] std::wstring Micros(std::uint64_t _microseconds)
{
  const std::uint64_t hundredths = (_microseconds + 5) / 10;
  return std::to_wstring(hundredths / 100) + L"." + (hundredths % 100 < 10 ? L"0" : L"") + std::to_wstring(hundredths % 100);
}

} // namespace

bool ParseCommandLine(std::span<const std::wstring> _arguments, LaunchOptions& _options)
{
  for (std::size_t index = 1; index < _arguments.size(); ++index)
  {
    const std::wstring& argument = _arguments[index];
    if (argument == L"--warp")
    {
      _options.warp = true;
      continue;
    }
    if (argument == L"--novsync")
    {
      _options.noVerticalSync = true;
      continue;
    }
    if (argument == L"--capture" && index + 3 < _arguments.size())
    {
      // THE LANDSCAPE IS NAMED ON THE COMMAND LINE (m1-vertical-slice/G2), because a capture is run
      // to look at something and which landscape it is played on is the first thing a reader has to
      // know. It is the file stem the content tree carries, so a name nobody authored is a content
      // fault the run reports rather than a built-in recipe to fall back on.
      const std::wstring& name = _arguments[index + 1];
      wchar_t* end = nullptr;
      const unsigned long ticks = std::wcstoul(_arguments[index + 2].c_str(), &end, 10);
      if (name.empty() || end == nullptr || *end != L'\0' || ticks == 0)
      {
        return false;
      }
      // A content id is ASCII (Content/ContentTree.h), so a wide argument carrying anything else is
      // refused rather than squeezed into a char and silently matched against no landscape at all.
      std::string stem;
      stem.reserve(name.size());
      for (const wchar_t letter : name)
      {
        if (letter < L' ' || letter > L'~')
        {
          return false;
        }
        stem.push_back(static_cast<char>(letter));
      }
      _options.capture = true;
      _options.captureLandscape = std::move(stem);
      _options.captureTicks = static_cast<std::uint32_t>(ticks);
      _options.captureDirectory = _arguments[index + 3];
      index += 3;
      continue;
    }
    return false;
  }
  return true;
}

App::App(const LaunchOptions& _options)
  : m_options(_options)
{
}

int App::Run()
{
  int exitCode = EXIT_FAILED;
  try
  {
    exitCode = m_options.capture ? RunCapture() : RunWindowed();
  }
  catch (const winrt::hresult_error& error)
  {
    Neuron::Log::Write(Neuron::LogLevel::Error, "fatal: " + Describe(error));
  }
  catch (const std::exception& error)
  {
    Neuron::Log::Write(Neuron::LogLevel::Error, std::string("fatal: ") + error.what());
  }
  Neuron::Log::Close();
  return exitCode;
}

int App::RunWindowed()
{
  if (!Neuron::Log::Open(Neuron::Paths::UserDirectory() / "Logs" / "Client.log"))
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, "the log file could not be opened; logging to the debugger only");
  }
  const ContentTree& content = MatchContent();
  const LandscapeDefinition* landscape = LandscapeNamed(content, SLICE_LANDSCAPE);
  if (landscape == nullptr)
  {
    Neuron::Log::Write(Neuron::LogLevel::Error,
                       std::string("GameData carries no landscape called ") + SLICE_LANDSCAPE + "; there is nothing to play on");
    return EXIT_FAILED;
  }
  Neuron::Window window;
  Neuron::InputQueue inputQueue;
  Neuron::FrameInput inputState;
  Neuron::InputRouter inputRouter;
  window.AttachInput(&inputQueue);
  Neuron::GraphicsDevice device(m_options.warp);
  Neuron::SwapChain swapChain(device, window.Handle(), window.ClientWidth(), window.ClientHeight());
  Neuron::SceneTarget scene(device, CLEAR_COLOR);
  Neuron::PresentPass present(device, scene);

  // THE MODEL SET AND THE GEOMETRY PASS NEED NO LANDSCAPE, so they are built now rather than with
  // the rest: the models are content and the commander colours are content, and both are known
  // before a single datagram has been sent.
  Neuron::ModelBuffers models(device, content.models);
  Neuron::GeometryPass geometry(device, models, scene.SampleCount(), content.ui.commanders);
  // The interface's two atlases and the pass that draws them (m1-vertical-slice/K3). They are
  // content, so they are read now; the panels of K4 draw through the same pass.
  const Neuron::BitmapFont font = LoadAtlas<Neuron::BitmapFont>("SpectrumFont.dds");
  const Neuron::IconAtlas icons = LoadAtlas<Neuron::IconAtlas>("Icons.dds");
  Neuron::UiPass ui(device, scene, font, icons);
  std::vector<Neuron::UiQuad> overlay;
  // THE PANELS (Design/Interface.md §7 to §9; m1-vertical-slice/K4), and the sink that gives them
  // the input first. The sink is the ROUTER'S FIRST, so a click on a panel never also reaches
  // selection or the camera (Client/UiInputSink.h), which is the half of §5's rule that stops the
  // click; the other half is Hud::BlockedBy, handed to the Operator below, which stops the ray.
  Hud hud;
  Neuron::UiInputSink uiSink;
  hud.Register(uiSink);
  inputRouter.AddSink(&uiSink);
  std::vector<PickBox> panelBoxes;
  std::vector<Order> panelOrders;
  // F1 "toggles the panel overlay off, for a clean look at the world" (§7's hotkey table). The
  // panels stop consuming the pointer with it: an invisible panel that still ate clicks would be a
  // dead strip along the bottom of the screen with nothing on it to explain why.
  bool panelsShown = true;
  // The ground cursor (m1-vertical-slice/K7). Its two textures are Species's MouseHighlight and
  // the pre-blurred twin Tools/ImportTextures.py makes from it; the blurred one is drawn first.
  std::uint32_t sharpWidth = 0;
  std::uint32_t sharpHeight = 0;
  std::uint32_t blurWidth = 0;
  std::uint32_t blurHeight = 0;
  const std::vector<std::uint8_t> sharpRing = LoadRing("GroundRing.dds", sharpWidth, sharpHeight);
  const std::vector<std::uint8_t> blurredRing = LoadRing("GroundRingBlur.dds", blurWidth, blurHeight);
  Neuron::CursorPass cursor(device, {sharpRing, sharpWidth, sharpHeight}, {blurredRing, blurWidth, blurHeight}, scene.SampleCount());

  // The two loops (TechnicalDesign.md §3). Everything from here to the draw is Match's; this
  // function keeps steps 1, 6 and 7 - the window's messages, the input routing and the drawing -
  // because those are the three that need a window and a device.
  Match match(content, Lobby(), ContentHash(content), Neuron::CHUNK_CELLS);
  if (!match.Start(*landscape, "Commander"))
  {
    return EXIT_FAILED; // Match::Start has logged which of its faults it was.
  }

  // The passes and the height view they read arrive together, when the join does. The optional is
  // what says "not yet", and every use of it below is under a check the analyser can follow.
  Neuron::HeightView heights{};
  std::optional<MatchPasses> passes;
  Neuron::Camera camera;
  const CameraController controller;
  Operator commander;
  // Windows' own pointer is hidden in aim mode (§4). ShowCursor keeps a COUNTER rather than a
  // flag, so it is called on the change and never per frame - a per-frame call would drive the
  // count to a few thousand and the first Escape would not bring the pointer back.
  Neuron::PointerMode shownFor = Neuron::PointerMode::Point;
  const Neuron::FogMode fog = Neuron::DEFAULT_FOG_MODE;
  // The frame time is measured over the whole loop body, present included, which is what a player
  // waits for. With --novsync and a display that allows tearing it is the renderer's cost; without
  // either it is the refresh interval, and the title says which so that a figure read off it is
  // never mistaken for the other (m0-foundation/T22).
  Neuron::FrameTimer frameTimer;
  const std::uint32_t syncInterval = m_options.noVerticalSync ? 0u : 1u;
  const wchar_t* pacing = syncInterval != 0              ? L"vsync"
                          : swapChain.TearingSupported() ? L"unlocked"
                                                         : L"novsync (no tearing here: still paced by the display)";
  if (m_options.noVerticalSync && !swapChain.TearingSupported())
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning,
                       "--novsync: this output does not allow tearing, so frames are still paced by the display");
  }
  const auto matchStarted = std::chrono::steady_clock::now();
  bool joinWarned = false;
  auto lastFrame = matchStarted;
  while (window.Pump())
  {
    const auto frameStart = std::chrono::steady_clock::now();
    // Where the picture lands in the window, which is what turns a click into an authored pixel and
    // what the present pass scales into. ONE reading of the window's size a frame, because two
    // would be a click tested against a rectangle the picture was not drawn in (AGENTS.md §5).
    //
    // READ BEFORE THE EVENTS ARE OFFERED, because the first sink they are offered to is the
    // interface's and every rectangle in the interface is authored (Client/UiInputSink.h): a sink
    // with last frame's fit would test this frame's click against the rectangle the picture was
    // drawn in one resize ago.
    const Neuron::ScaledRectangle fit =
      Neuron::FitAuthored(window.ClientWidth(), window.ClientHeight(), Neuron::AUTHORED_WIDTH_PIXELS, Neuron::AUTHORED_HEIGHT_PIXELS);
    uiSink.SetFit(fit);
    // 1 and 6. The frame's input (TechnicalDesign.md §6.5): derive what the frame saw, offer it to
    // the sinks, mask what they took, fire the subscriptions, and only then read the view.
    const std::size_t consumed = Neuron::DeriveFrameInput(inputQueue.Events(), inputState);
    inputRouter.Dispatch(inputQueue.Events().first(consumed));
    // WHAT THE PANELS TOOK, taken before the mask: the panels of K4 were built from LAST frame's
    // replica and drawn in last frame's picture, which is the one the commander was looking at when
    // he pressed the button. Acting on it below is what makes a click on a button do its thing.
    const Neuron::UiEventResult panelEvent = uiSink.Take();
    Neuron::FrameInput inputView = inputState;
    inputRouter.Mask(inputView);
    inputRouter.FireSubscriptions(inputView);
    inputQueue.Erase(consumed);
    const auto now = std::chrono::steady_clock::now();
    const auto sinceLastFrame = now - lastFrame;
    lastFrame = now;
    const float seconds = std::min(std::chrono::duration<float>(sinceLastFrame).count(), 0.1f);

    // 2 to 5. Drain, apply, interpolate, build the render view. Given the WALL TIME and not the
    // clamped seconds: the clamp above is the camera's, so that a frame that took a second does not
    // fly it across the map, and the liveness clock must not be shortened by it - a timeout that
    // ran slow whenever the display hitched is a timeout that cannot be trusted.
    match.Advance(std::chrono::duration_cast<std::chrono::nanoseconds>(sinceLastFrame));
    // What the panels are shown, filled by the frame's logic below and read by its drawing. One
    // record over both, because the two must be the same frame: panels built from a replica the
    // logic had not yet read would be a frame ahead of the world drawn behind them.
    Hud::Frame panels{};
    panels.match = &match;
    panels.content = &content;
    if (!passes.has_value() && match.TerrainReady())
    {
      heights = ViewOf(match.Terrain());
      // emplace returns the reference, and that is what is used: there is no operator-> on the
      // optional here at all, so nothing has to reason about whether it is engaged.
      MatchPasses& built = passes.emplace(device, scene, heights, match.Terrain().CellsPerSide());
      AimAtBase(camera, match, landscape->starts.front(), built.terrain.ExtentWorldUnits());
      camera.ClampHeight(heights);
      Neuron::Log::Write(Neuron::LogLevel::Info, "match: the landscape arrived - " + std::to_string(heights.samplesPerSide) +
                                                   " samples a side, " + std::to_string(match.Terrain().CellsPerSide()) +
                                                   " cells, highest " + std::to_string(heights.highest));
    }
    if (passes.has_value())
    {
      // The ground this commander's structures levelled reaches the mesh here, BEFORE the frame is
      // recorded: Rebuild maps the terrain buffers and waits for the GPU first, which is not
      // something to do between BeginFrame and EndFrame (m1-vertical-slice/K6). Almost every frame
      // names no chunk and it costs nothing.
      passes->terrain.Rebuild(match.View().changedChunks);

      // 6. WHAT THE COMMANDER DID (Design/Interface.md §4 to §6; m1-vertical-slice/G1b). Before the
      //    camera moves, because picking is against the matrices the LAST frame was drawn with -
      //    which is the frame he was looking at when he clicked.
      //
      // WHAT THE PANELS DID COMES FIRST. A panel's click is an order or a change of what is armed,
      // and both have to be in before the Operator reads the frame - an arm request taken
      // afterwards would leave the cursor and the footprint ghost a frame behind the button that
      // lit. The record it is given is LAST frame's, deliberately: those are the panels the click
      // landed on, and reading the selection this frame has not yet made would answer a click
      // against a panel that did not exist when the button went down.
      FillPanelFrame(panels, commander, match, camera);
      panelOrders.clear();
      hud.OnEvent(panelEvent, panels, panelOrders);
      for (const Order& order : panelOrders)
      {
        if (!match.Submit(order))
        {
          break; // The unacknowledged window is full, as in Operator::Advance and for its reason.
        }
      }
      ArmedOrder wanted = ArmedOrder::None;
      std::uint32_t wantedRow = 0;
      if (hud.TakeArmRequest(wanted, wantedRow))
      {
        if (wanted == ArmedOrder::PlaceStructure)
        {
          commander.ArmStructure(wantedRow);
        }
        else
        {
          commander.Arm(wanted);
        }
      }
      std::uint32_t portrait = 0;
      if (hud.TakePortraitPick(portrait))
      {
        commander.SelectOnly(portrait);
      }
      // §7's hotkey table: "1 - 5, the command panel's tabs", and F1 for the whole strip.
      for (std::uint8_t key = '1'; key <= '5'; ++key)
      {
        if (inputView.keyEdges[key] > 0)
        {
          static_cast<void>(hud.TakeTabKey(key));
        }
      }
      if (inputView.keyEdges[KEY_PANEL_TOGGLE] > 0)
      {
        panelsShown = !panelsShown;
        // AN INVISIBLE PANEL CONSUMES NOTHING (Client/UiPanel.cpp's hit tests refuse one), so this
        // is the whole of turning the interface off: it stops drawing, it stops taking clicks, and
        // Hud::BlockedBy stops refusing rays through it. A strip that was merely not drawn would be
        // a dead band along the bottom of the screen with nothing on it to explain why.
        hud.SetVisible(panelsShown);
      }

      Operator::Frame seen{};
      seen.input = &inputView;
      seen.camera = PickCameraOf(camera);
      seen.terrain = &heights;
      seen.nowMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - matchStarted).count();
      Neuron::AuthoredPosition authored{};
      seen.pointerInside = Neuron::AuthoredFromClient(fit, inputView.mouseX, inputView.mouseY, Neuron::AUTHORED_WIDTH_PIXELS,
                                                      Neuron::AUTHORED_HEIGHT_PIXELS, authored);
      seen.pointerX = authored.x;
      seen.pointerY = authored.y;
      panelBoxes.clear();
      hud.BlockedBy(panelBoxes);
      seen.panels = panelBoxes;
      commander.Advance(seen, content, match);

      // §9.2'S MINIMAP CLICKS, off the UNMASKED input. The sink consumed the button and masked it
      // out of the view above - which is exactly right, because a click on the map must not also
      // select or order in the world - so the two edges are read from the frame's own input here,
      // where the rectangle they are tested against is the map's and nothing else's.
      Neuron::AuthoredPosition onMap{};
      if (panelsShown && Neuron::AuthoredFromClient(fit, inputState.mouseX, inputState.mouseY, Neuron::AUTHORED_WIDTH_PIXELS,
                                                    Neuron::AUTHORED_HEIGHT_PIXELS, onMap))
      {
        const Neuron::MinimapPoint point = hud.MinimapPointAt(onMap.x, onMap.y);
        // A LEFT DRAG SCRUBS AND A LEFT CLICK JUMPS, which is one rule: the button being HELD over
        // the map moves the camera every frame it is (§9.2), and a click is a frame of that.
        if (point.inside && inputState.buttonsHeld[Neuron::IndexOf(Neuron::MouseButton::Left)])
        {
          camera.SetPosition(point.x, camera.Position().y, point.z);
          camera.ClampHeight(heights);
        }
        if (point.inside && inputState.buttonEdges[Neuron::IndexOf(Neuron::MouseButton::Right)] > 0)
        {
          commander.MoveTo(point.x, point.z, match);
        }
      }
      if (commander.QuitRequested())
      {
        break;
      }
      controller.Advance(camera, inputView, commander.Mode(), heights, seconds, window.ClientWidth(), window.ClientHeight());
      if (commander.Mode() != shownFor)
      {
        ::ShowCursor(commander.Mode() == Neuron::PointerMode::Point ? TRUE : FALSE);
        shownFor = commander.Mode();
      }
    }
    else if (!joinWarned && now - matchStarted > JOIN_WAIT)
    {
      // Said once, because a window of nothing but sky is indistinguishable from a window that
      // hung, and the log is the only place that can tell them apart.
      Neuron::Log::Write(Neuron::LogLevel::Warning, "match: the host has not answered the join after " + std::to_string(JOIN_WAIT.count()) +
                                                      " seconds; the frame is the sky");
      joinWarned = true;
    }

    if (window.TakeResized())
    {
      swapChain.Resize(device, window.ClientWidth(), window.ClientHeight());
    }
    if (window.ClientWidth() == 0 || window.ClientHeight() == 0)
    {
      WaitMessage();
      continue;
    }

    // 7. Draw and present. A frame before the join has landed presents the cleared target, which is
    // the Species sky; it is a picture rather than a hang.
    ID3D12GraphicsCommandList* list = device.BeginFrame();
    scene.Begin(list);
    if (passes.has_value())
    {
      DrawMatch(list, scene, *passes, geometry, camera, match, fog);
      // THE GROUND CURSOR, on the world and under the interface (Design/Interface.md §4). It is
      // drawn in aim mode, where there is no pointer and the ring IS the cursor; point mode's six
      // 32x32 icons at the pointer are K4's, with the panels they belong beside.
      cursor.Begin();
      const D3D12_GPU_VIRTUAL_ADDRESS worldConstants = passes->terrain.ConstantsAddress();
      const ArmedOrder armed = commander.Armed();
      if (armed == ArmedOrder::PlaceStructure && commander.Ground().hit &&
          commander.ArmedStructure() < content.structures.structures.size())
      {
        // THE FOOTPRINT GHOST (m1-vertical-slice/G1b's last acceptance line). Its rule and its
        // legality are Replica/PlacementPreview.h's and are already tested; what was missing was a
        // quad lying on the ground to show it with, and this pass is what can put one there.
        const StructureDesc& row = content.structures.structures[commander.ArmedStructure()];
        const std::int32_t cellX = static_cast<std::int32_t>(commander.Ground().x) / Neuron::WORLD_UNITS_PER_CELL;
        const std::int32_t cellZ = static_cast<std::int32_t>(commander.Ground().z) / Neuron::WORLD_UNITS_PER_CELL;
        const float minX = static_cast<float>(cellX * Neuron::WORLD_UNITS_PER_CELL);
        const float minZ = static_cast<float>(cellZ * Neuron::WORLD_UNITS_PER_CELL);
        const float maxX = minX + static_cast<float>(row.footprintCellsX * Neuron::WORLD_UNITS_PER_CELL);
        const float maxZ = minZ + static_cast<float>(row.footprintCellsY * Neuron::WORLD_UNITS_PER_CELL);
        const bool legal = MayStandHere(match, content, commander.ArmedStructure(), cellX, cellZ);
        cursor.DrawFootprint(list, worldConstants, minX, minZ, maxX, maxZ, commander.Ground().y + GHOST_LIFT_WORLD_UNITS,
                             legal ? CURSOR_BUILD : CURSOR_REFUSE);
      }
      if (commander.Mode() == Neuron::PointerMode::Aim)
      {
        // The match's own age and not the frame's: the pulse is a wave over the whole run, and
        // the outer loop's `seconds` is how long the LAST frame took, which is a different number
        // with the same name - one MSVC refuses outright, and rightly.
        const float sinceStart = std::chrono::duration<float>(now - matchStarted).count();
        const float pulse =
          armed == ArmedOrder::PlaceStructure ? 1.0f + (std::fabs(std::sin(PULSE_RADIANS_PER_SECOND * sinceStart)) * PULSE_DEPTH) : 1.0f;
        const DirectX::XMFLOAT3 eye = camera.Position();
        cursor.Draw(list, worldConstants, commander.Ground(), eye.x, eye.y, eye.z,
                    armed == ArmedOrder::PlaceStructure ? CURSOR_BUILD : CURSOR_DEFAULT, pulse);
      }
      // AND WHAT THE COMMANDER IS TOLD, over it: the panels, then the drag rectangle, the refusal
      // line and whichever of §10's two overlays is open. The panels are built LAST of the frame's
      // logic and drawn first of its interface: last, so that they hold the replica this frame drew
      // and the next frame's click lands on what the commander is looking at now; first, so that
      // the refusal line and the modal overlays sit over them rather than under.
      //
      // THE RECORD IS RE-READ BEFORE THE REFRESH and not reused from above: Operator::Advance has
      // run since, so the selection, the abilities and what is armed have all moved - and
      // Abilities() is a span over a vector that the same Advance refilled, which a record held
      // across it would be pointing into from before it grew.
      FillPanelFrame(panels, commander, match, camera);
      hud.Refresh(panels);
      ui.SetMinimap(hud.MinimapPixels());
      overlay.clear();
      hud.Build(content.ui.chrome, icons, overlay);
      commander.BuildOverlay(content.ui.chrome, content.structures.structures, match,
                             std::chrono::duration_cast<std::chrono::milliseconds>(now - matchStarted).count(), overlay);
      ui.Draw(list, scene, overlay);
    }
    scene.Resolve(list);
    present.Draw(list, swapChain.CurrentBackBuffer(), swapChain.CurrentRenderTargetView(), fit);
    device.EndFrame();
    swapChain.Present(syncInterval);
    device.DrainDebugMessages();
    frameTimer.Add(static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frameStart).count()));
    // Once a quarter of the window rather than every frame: a title that changes 500 times a
    // second is unreadable, and SetWindowTextW is a message to the window's own thread.
    if (frameTimer.TotalFrames() % (Neuron::FrameTimer::WINDOW_FRAMES / 4) == 0)
    {
      std::wstring title = L"Outpost Commander - ";
      title += pacing;
      title += L" - mean " + Micros(frameTimer.MeanMicroseconds()) + L" ms, median " + Micros(frameTimer.MedianMicroseconds()) +
               L", 99th " + Micros(frameTimer.PercentileMicroseconds(99)) + L" - tick " + std::to_wstring(match.HostSide().Tick()) + L", " +
               std::to_wstring(geometry.LastInstanceCount()) + L" instances in " + std::to_wstring(geometry.LastDrawCount()) + L" draws";
      window.SetTitle(title.c_str());
    }
  }
  device.WaitForIdle();
  device.DrainDebugMessages();
  window.AttachInput(nullptr);
  // The host thread is stopped and joined before the device goes, because a host that was still
  // publishing into a transport whose client end had been destroyed is a race nobody would find.
  match.Stop();
  Neuron::Log::Write(Neuron::LogLevel::Info, "exit: " + std::to_string(device.FramesBegun()) + " frames, " +
                                               std::to_string(device.DebugMessageCount()) + " debug-layer messages, " +
                                               std::to_string(inputQueue.Dropped()) + " input events dropped, host tick " +
                                               std::to_string(match.HostSide().Tick()) + ", " +
                                               std::to_string(match.HostSide().SlowedTicks()) + " tick(s) of wall time given away");
  return ExitCodeOf(device);
}

int App::RunCapture()
{
  std::error_code ignored;
  std::filesystem::create_directories(m_options.captureDirectory, ignored);
  if (!Neuron::Log::Open(m_options.captureDirectory / "capture.log"))
  {
    return EXIT_FAILED;
  }
  Neuron::Log::SetMinimumLevel(Neuron::LogLevel::Debug);
  Neuron::Log::Write(Neuron::LogLevel::Info,
                     "capture: " + std::to_string(m_options.captureTicks) + " tick(s) of " + m_options.captureLandscape + " on " +
                       (m_options.warp ? "WARP" : "the first hardware adapter") + " into " + m_options.captureDirectory.string());
  const ContentTree& content = MatchContent();
  const LandscapeDefinition* landscape = LandscapeNamed(content, m_options.captureLandscape);
  if (landscape == nullptr)
  {
    Neuron::Log::Write(Neuron::LogLevel::Error,
                       "GameData carries no landscape called " + m_options.captureLandscape + "; there is nothing to capture");
    return EXIT_FAILED;
  }
  Neuron::GraphicsDevice device(m_options.warp);
  Neuron::SceneTarget scene(device, CLEAR_COLOR);
  Neuron::FrameCapture capture(device, Neuron::AUTHORED_WIDTH_PIXELS, Neuron::AUTHORED_HEIGHT_PIXELS, Neuron::SCENE_COLOR_FORMAT);
  Neuron::ModelBuffers models(device, content.models);
  Neuron::GeometryPass geometry(device, models, scene.SampleCount(), content.ui.commanders);
  // THE GROUND CURSOR IS DRAWN IN THE CAPTURE TOO, and that is not decoration. The capture is an
  // observer with no pointer and is never in aim mode, so without this the pass of K7 would be
  // compiled by CI and never RUN by it - and "a green build says nothing about whether the game
  // draws" (AGENTS.md §3). Here the ring is cast through the middle of the frame, which is exactly
  // what aim mode does, so every run puts the pipeline, its two blends and its two textures under
  // the debug layer, and every frame shows where the camera is pointed.
  std::uint32_t sharpWidth = 0;
  std::uint32_t sharpHeight = 0;
  std::uint32_t blurWidth = 0;
  std::uint32_t blurHeight = 0;
  const std::vector<std::uint8_t> sharpRing = LoadRing("GroundRing.dds", sharpWidth, sharpHeight);
  const std::vector<std::uint8_t> blurredRing = LoadRing("GroundRingBlur.dds", blurWidth, blurHeight);
  Neuron::CursorPass cursor(device, {sharpRing, sharpWidth, sharpHeight}, {blurredRing, blurWidth, blurHeight}, scene.SampleCount());
  // AND THE PANELS, for the same reason the ring is here (m1-vertical-slice/K4's second acceptance
  // line: "a capture with the panels on shows them legible at 1:1"). A capture that drew the world
  // alone would have CI compiling every panel in §7 to §9 and looking at none of them, and the
  // frames it writes are the only thing an agent ever sees of this game.
  const Neuron::BitmapFont font = LoadAtlas<Neuron::BitmapFont>("SpectrumFont.dds");
  const Neuron::IconAtlas icons = LoadAtlas<Neuron::IconAtlas>("Icons.dds");
  Neuron::UiPass ui(device, scene, font, icons);
  std::vector<Neuron::UiQuad> overlay;
  Hud hud;
  std::vector<std::uint32_t> watched;
  std::vector<SelectedObject> abilities;

  // TWO SCRIPTED COMMANDERS AND A CLIENT THAT WATCHES ONE (m1-vertical-slice/G2, the owner's ruling
  // of 2026-09-19). The lobby seats no human at all, so both sides play; the client joins as an
  // OBSERVER of seat 0 and is sent that commander's frames through the ordinary interest and fog
  // path, which is why the capture cannot show more of the match than seat 0 can see.
  Match match(content, Lobby(true), ContentHash(content), Neuron::CHUNK_CELLS);
  if (!match.Start(*landscape, "Capture", WATCHED_SEAT, HostPacing::Stepped))
  {
    return EXIT_FAILED;
  }
  Neuron::Camera camera;

  // THE JOIN IS WAITED FOR BEFORE THE FIRST FRAME IS WRITTEN, and the window deliberately is not.
  // A window shows the sky for the frame or two a loopback join takes and nobody minds; a capture
  // whose first frame is the cleared target has written a BLACK BMP as its first artefact, and the
  // one thing an agent ever sees of this game is those files.
  //
  // IT COSTS TICKS AND THEY ARE THE MATCH'S OWN. A stepped host runs when this loop says so, so
  // the wait is a bounded number of ticks rather than a wall-clock timeout: the same number on a
  // fast machine and a slow one, and a run that reproduces.
  const auto joined = [&match] { return match.TerrainReady() && match.Commander().NewestSequence() != NO_BASELINE; };
  for (std::uint32_t attempt = 0; attempt < JOIN_WAIT_TICKS && !joined(); ++attempt)
  {
    match.StepHost();
    match.Advance(TICK_DURATION);
  }
  if (!joined())
  {
    Neuron::Log::Write(Neuron::LogLevel::Error, "capture: the host did not answer the join in " + std::to_string(JOIN_WAIT_TICKS) +
                                                  " tick(s); there is nothing to capture");
    return EXIT_FAILED;
  }
  // Plain objects rather than an optional: the landscape is known by the time this line runs, so
  // there is no "not yet" for the capture to carry, and nothing below has to ask whether it has one.
  const Neuron::HeightView heights = ViewOf(match.Terrain());
  MatchPasses passes(device, scene, heights, match.Terrain().CellsPerSide());
  const float extent = passes.terrain.ExtentWorldUnits();
  camera.SetFarPlane(extent * Neuron::FAR_PLANE_EXTENT_FACTOR);
  const std::vector<Neuron::CaptureVantage> script = ScriptFor(*landscape, extent);
  Neuron::Log::Write(Neuron::LogLevel::Info, "capture: the landscape arrived - " + std::to_string(heights.samplesPerSide) +
                                               " samples a side, " + std::to_string(match.Terrain().CellsPerSide()) + " cells, highest " +
                                               std::to_string(heights.highest) + ", " + std::to_string(script.size()) +
                                               " vantage(s) over " + std::to_string(m_options.captureTicks) + " tick(s)");

  // THE MATCH IS RUN BY TICKS AND NOT BY WALL TIME (m1-vertical-slice/G2). One pass of the host per
  // pass of this loop, handed exactly a tick's worth, so the capture advances the simulation by the
  // number it was asked for as fast as the machine will run it - and the client's own clock is
  // handed the same tick, because a liveness clock reading wall time while the host runs at warp
  // would have the host hearing from its client once every few hundred ticks.
  const auto started = std::chrono::steady_clock::now();
  std::uint32_t written = 0;
  std::uint32_t framesWithAShotInTheAir = 0;
  std::uint32_t framesAimedAtTheFighting = 0;
  for (std::uint32_t tick = 0; tick <= m_options.captureTicks; ++tick)
  {
    // WHAT THE PANELS ARE SHOWN LOOKING AT, chosen BEFORE the tick is run. The capture's client is
    // an observer and clicks nothing, so a selection has to be made for it or §8's portrait grid
    // and the whole of §9.1 would be drawn empty in every frame CI ever looks at; it takes the
    // watched commander's own devices, which is what a player who dragged a box round his army
    // would have. Before rather than after, because Match::Select is a store and the render view is
    // built by Advance: a selection made after it would light the devices in the NEXT frame. Only
    // on the ticks a frame is written from, because that is the only Advance whose view is drawn.
    if (tick % CAPTURE_EVERY_TICKS == 0)
    {
      watched.clear();
      // `seen` and not `device`: this function already has a Neuron::GraphicsDevice called `device`
      // at its own scope, and MSVC reports the inner name as C4456, which /warnaserror makes a
      // build failure - the same trap UiPass's `description` fell into one commit ago.
      for (const auto& seen : match.Commander().Devices())
      {
        if (seen.second.state.seat == WATCHED_SEAT && watched.size() < PORTRAITS_SHOWN)
        {
          watched.push_back(seen.first);
        }
      }
      match.Select(watched);
    }
    if (tick > 0)
    {
      match.StepHost();
      match.Advance(TICK_DURATION);
      passes.terrain.Rebuild(match.View().changedChunks); // As in the window's loop, and before the frame
    }
    if (tick % CAPTURE_EVERY_TICKS != 0)
    {
      continue;
    }
    // WHERE THE CAMERA LOOKS: the script's vantage, and its aim replaced by the fighting when this
    // vantage follows and this commander can see any. The ground height is the CLIENT'S landscape,
    // as the window's opening pose is, so a vantage over a ridge is over the ridge.
    Neuron::CaptureVantage vantage = Neuron::VantageAt(script, tick);
    float actionX = 0.0f;
    float actionZ = 0.0f;
    const bool following = vantage.follow && Neuron::ActionCenter(match.View().instances, actionX, actionZ);
    if (following)
    {
      vantage.aimX = actionX;
      vantage.aimZ = actionZ;
    }
    const std::uint32_t inTheAir = CountProjectiles(match.View());
    framesAimedAtTheFighting += following ? 1 : 0;
    framesWithAShotInTheAir += inTheAir > 0 ? 1 : 0;
    const std::int32_t aimSubunitsX = SubunitsOfWorldUnits(vantage.aimX);
    const std::int32_t aimSubunitsZ = SubunitsOfWorldUnits(vantage.aimZ);
    const float ground = Neuron::WorldUnitsOfSubunits(GroundHeightSubunits(match.Terrain(), aimSubunitsX, aimSubunitsZ));
    const Neuron::CapturePose pose = Neuron::PoseOf(vantage, ground);
    camera.SetPosition(pose.eyeX, pose.eyeY, pose.eyeZ);
    camera.LookAt(pose.atX, pose.atY, pose.atZ);
    camera.ClampHeight(heights);
    // Where the ring goes: the ray from the eye to what the camera is looking at - the middle of
    // the frame - cast against the heightfield exactly as aim mode casts it. AFTER the clamp, so
    // the eye this reads is the one the frame is drawn from and not the one the script asked for.
    const DirectX::XMFLOAT3 eye = camera.Position();
    const Neuron::GroundHit aimed =
      Neuron::RayAgainstGround(heights, {eye.x, eye.y, eye.z}, {pose.atX - eye.x, pose.atY - eye.y, pose.atZ - eye.z});

    // The panels themselves, from the replica this frame is drawn from. The tab is stepped frame by
    // frame so that all five of §7's are drawn over a run rather than the first one over and over.
    AbilitiesOf(match.Commander().Devices(), match.Commander().Structures(), match.Commander().Designs(), content, watched, abilities);
    static_cast<void>(hud.TakeTabKey(static_cast<std::uint8_t>('1' + (written % HUD_TAB_COUNT))));
    Hud::Frame panels{};
    panels.match = &match;
    panels.content = &content;
    panels.selected = abilities;
    panels.tick = match.Commander().NewestTick();
    panels.camera = PickCameraOf(camera);
    hud.Refresh(panels);
    ui.SetMinimap(hud.MinimapPixels());
    overlay.clear();
    hud.Build(content.ui.chrome, icons, overlay);

    // ADR-005's PAIR IS ONE POSE, ONE TICK AND THE TWO FOG MODES. The ADR rests on a comparison of
    // the Species fog against the desaturation and says the capture re-makes it on every run; two
    // frames five seconds apart would compare two matches as well as two fogs, so the comparison
    // tick is drawn twice and nothing else about it moves.
    const bool comparison = tick == COMPARISON_TICK;
    for (std::uint32_t pass = 0; pass < (comparison ? 2u : 1u); ++pass)
    {
      const Neuron::FogMode fog = pass == 0 ? vantage.fog : Neuron::DEFAULT_FOG_MODE;
      ID3D12GraphicsCommandList* list = device.BeginFrame();
      scene.Begin(list);
      DrawMatch(list, scene, passes, geometry, camera, match, fog);
      cursor.Begin();
      cursor.Draw(list, passes.terrain.ConstantsAddress(), aimed, eye.x, eye.y, eye.z, CURSOR_DEFAULT, 1.0f);
      ui.Draw(list, scene, overlay);
      scene.Resolve(list);
      capture.Record(list, scene.Resolved());
      device.EndFrame();
      device.WaitForIdle();
      const std::filesystem::path file =
        m_options.captureDirectory / ("frame-" + FiveDigits(tick) + (pass == 0 ? "" : "-desaturated") + ".bmp");
      if (!capture.Write(file))
      {
        return EXIT_FAILED;
      }
      ++written;
      Neuron::Log::Write(Neuron::LogLevel::Info,
                         "capture: wrote " + file.filename().string() + " from (" + std::to_string(static_cast<int>(pose.eyeX)) + ", " +
                           std::to_string(static_cast<int>(pose.eyeY)) + ", " + std::to_string(static_cast<int>(pose.eyeZ)) + ") at (" +
                           std::to_string(static_cast<int>(pose.atX)) + ", " + std::to_string(static_cast<int>(pose.atZ)) +
                           "), host tick " + std::to_string(match.HostSide().Tick()) + ", " + std::to_string(geometry.LastInstanceCount()) +
                           " instances in " + std::to_string(geometry.LastDrawCount()) + " draws, " + std::to_string(inTheAir) +
                           " shot(s) in the air" + (following ? " (followed)" : "") +
                           (aimed.hit ? (aimed.water ? ", ring on the sea" : ", ring on the ground") : ", ring hidden") + ", fog " +
                           (fog == Neuron::FogMode::LinearToColor ? "linear to black" : "desaturation") + ", " +
                           std::to_string(ui.LastQuadsDrawn()) + " interface quad(s) over " + std::to_string(watched.size()) + " selected");
      device.DrainDebugMessages();
    }
  }
  device.WaitForIdle();
  device.DrainDebugMessages();
  match.Stop();
  const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
  // WHAT THE RUN COST AND WHAT IT SHOWED, in one line, because the agent reads this log and cannot
  // fetch the artefact. "Shots in the air" is G2's own acceptance: a capture whose frames show none
  // is a capture pointed at the wrong part of the landscape, and the script is what changes.
  Neuron::Log::Write(Neuron::LogLevel::Info,
                     "capture: " + std::to_string(written) + " frame(s) over " + std::to_string(m_options.captureTicks) + " tick(s) in " +
                       std::to_string(took.count()) + " ms, " + std::to_string(framesWithAShotInTheAir) +
                       " frame(s) with a shot in the air, " + std::to_string(framesAimedAtTheFighting) + " aimed at the fighting, " +
                       // WHAT THE WATCHED COMMANDER RESEARCHED, because this is the one run in the tree that plays the
                       // lobby the GAME ships rather than a fixture's (m1-vertical-slice/S15). A scripted commander with
                       // auto-research off is locked to the machine gun, which cannot destroy a command post at all, so a
                       // match it has won can never be finished - and every unit suite passed throughout, because their
                       // fixture sets the flag and the executable's lobby did not. The count is the observed seat's own
                       // mask from the replica, so it is read the way a client reads anything.
                       std::to_string(std::popcount(match.Commander().Own().researchComplete)) +
                       " item(s) researched by the "
                       "watched commander, " +
                       std::to_string(device.DebugMessageCount()) + " debug-layer messages, debug layer " +
                       (device.DebugLayerActive() ? "on" : "off") + ", host tick " + std::to_string(match.HostSide().Tick()));
  return ExitCodeOf(device);
}

} // namespace Outpost

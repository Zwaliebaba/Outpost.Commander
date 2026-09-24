#include "pch.h"

// NOT in pch.h, and it has to stay out of it: the Storage projection is large enough that
// adding it to this project's precompiled header fails the build outright with C3859 and
// C1076, the compiler running out of room for the PCH itself. It is needed by exactly one
// function here -- ADR-008's LocalState folder -- so it is included by exactly one file.
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>

// Fullscreen lives in its own translation unit rather than here, and ViewMode.h says why: one more
// projection in this file is what tips it over C3859.
#include "ViewMode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// The packaged client, and the whole of it: an IFrameworkView over a CoreWindow that activates,
// runs M0.5's temporary probe and exits. There is no XAML anywhere in this tree and no
// SwapChainPanel.
//
// This project holds Windows Runtime glue and nothing else (AGENTS.md R20). Anything a suite could
// pin belongs below it in NeuronClient or GameClient -- a thing an Application holds is a thing no
// suite can reach.

using winrt::Windows::ApplicationModel::Core::CoreApplication;
using winrt::Windows::ApplicationModel::Core::CoreApplicationView;
using winrt::Windows::ApplicationModel::Core::IFrameworkView;
using winrt::Windows::ApplicationModel::Core::IFrameworkViewSource;
using winrt::Windows::Graphics::Display::DisplayInformation;
using winrt::Windows::Storage::ApplicationData;
using winrt::Windows::UI::Core::CoreDispatcher;
using winrt::Windows::UI::Core::CoreProcessEventsOption;
using winrt::Windows::UI::Core::CoreWindow;

namespace
{
// ---------------------------------------------------------------------------------------------
// M0.5 SCAFFOLDING, AND IT IS MEANT TO BE DELETED.
//
// The gate needs a client that logs what arrives (Design/Plan/M0-the-wire.md, M0.5). Everything
// between this banner and the next one goes once ADR-008 and TechnicalDesign.md section 9 have the
// four answers.
//
// IT COMPUTES NOTHING. Every line it writes is an observation -- a sequence, the host's stamp, its
// own stamp -- and Scripts/ProbeReport.py turns those into loss and jitter afterwards. That is
// what keeps R20 honest here: there is no arithmetic in this executable for a suite to be missing.
// ---------------------------------------------------------------------------------------------

/// How much jitter the frame will absorb, and how large a datagram may be. Generous on both
/// counts because the probe would rather record a late burst than drop it -- a queue overflow
/// here would read as packet loss the network never caused. ADR-003's real figures are M0.9's.
inline constexpr std::size_t QUEUE_SLOTS = 512;
inline constexpr std::size_t QUEUE_SLOT_BYTES = 2048;

/// M1.9's per-frame instance capacity. ADR-003's peak is **110 entities** in the reduced MVP and 220
/// at four players; this is the second, so the third and fourth slots cost no allocation change.
inline constexpr std::uint32_t MAXIMUM_INSTANCES = 220;

/// M2.4: the one identity instance every asteroid variant's draw shares. The rocks are already placed in
/// their vertices (`GameClient/AsteroidMesh.h`), so the instance puts them at the origin, unturned.
inline constexpr std::uint32_t FIELD_INSTANCES = 1;

/// Q81: the most beams a frame draws -- every held tracer, and one mining or unloading beam an entity.
inline constexpr std::uint32_t BEAM_CAPACITY =
  static_cast<std::uint32_t>(Outpost::MAX_TRACERS) + MAXIMUM_INSTANCES + (8 * Outpost::MAX_WRECKS);

/// The host learns where to reply from the first datagram it hears, so the hello is repeated --
/// a single one could be the packet that gets lost, and the run would then measure silence.
/// **THE HELLO IS GONE AND THE JOIN REPLACED IT** (ADR-013). An empty command packet used to be
/// what registered a client, because the host learned an endpoint from any packet that decoded.
/// It no longer does: a command from an endpoint with no session is refused, and the cadence now
/// lives in `Outpost::JoinState` where a suite can reach it.

/// M0.16's measurement window. The first frames are thrown away because they are not frames the
/// game would ever run: the pipeline state is created on the first of them, the driver is still
/// compiling, and the fullscreen transition may not have finished settling. Sixty seconds of
/// vsynced frames after that is a long enough sample that the mean stops moving.
///
/// **IT WRITES ITS FIGURES AND KEEPS RUNNING, WHICH IS A CHANGE M1.9 MADE.** It used to exit here,
/// because a gate run has to terminate by itself or it writes no figures at all -- and that was
/// right while there was nothing to look at but a coloured rectangle. There are hulls now, and a
/// client that closes itself after sixty seconds is hostile to the only way M1.9's remaining exit
/// criteria can be met: somebody looking at the silhouettes.
///
/// **The figures are still written without anybody closing the window**, which is the property that
/// mattered -- they are reported the moment the window completes rather than at teardown, and the
/// sample stops there so a long look cannot change them.
inline constexpr std::uint64_t WARMUP_FRAMES = 120;
inline constexpr std::uint64_t MEASURE_FRAMES = 3600;

/// M0.17'S RECTANGLE, in AUTHORED coordinates (`Interface.md` section 1): the 48 x 48 touch floor,
/// inset by the 16 pixels of clear space that section requires, in the bottom left corner. It is
/// the smallest thing the interface is ever allowed to contain, so a pair of eyes checking that it
/// lands "at the physically correct place" is checking the number the whole interface rests on --
/// 96 physical pixels and 9.15 mm on the target panel. The physical rectangle it maps to is written
/// into the log below rather than inferred from the picture.
inline constexpr Neuron::AuthoredRect INTERFACE_PROBE_RECT{.left = 16, .top = 896, .right = 64, .bottom = 944};

/// How long the fullscreen transition is given, and what counts as having finished. A quarter of a
/// second of an unchanging width is several refreshes at any rate this panel runs at, and the
/// timeout is generous because overshooting it costs a second of a sixty-second run while
/// undershooting it measures the wrong panel.
inline constexpr std::chrono::milliseconds SETTLE_TIMEOUT{3000};
inline constexpr std::chrono::milliseconds SETTLE_STABLE_FOR{250};
inline constexpr std::chrono::milliseconds SETTLE_POLL_INTERVAL{8};

[[nodiscard]] std::uint64_t MillisecondsSince(std::chrono::steady_clock::time_point _start) noexcept
{
  const auto elapsed = std::chrono::steady_clock::now() - _start;
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

/// Both at once and on purpose: the debugger's output window is what a developer watches live,
/// and the file is what comes off the Surface Pro afterwards and feeds the report script. A
/// tablet has no console, and DebugView on a device is a bad way to keep a run of some minutes.
void Report(std::ofstream& _log, const std::string& _line)
{
  const std::string terminated = _line + "\n";
  OutputDebugStringA(terminated.c_str());
  _log << terminated;
  _log.flush();
}

/// **ONE MESH OUT OF THE PACKAGE, BY ITS CATALOG NAME, OR A REPORT SAYING WHY NOT.** The hulls and the
/// asteroid variants take the same route, so the route is written once: every way this can fail names the
/// mesh, because the failure ADR-021 expects is a clean install whose package did not carry the file.
[[nodiscard]] bool ReadShippedMesh(std::string_view _name, std::ofstream& _log, Outpost::HullMesh& _outMesh)
{
  const Outpost::MeshEntry* entry = Outpost::FindMesh(_name);
  if (entry == nullptr)
  {
    Report(_log, "MESH " + std::string{_name} + " is not in the catalog");
    return false;
  }

  // The catalog states the `ms-appx:///` form, because that is what the manifest says and what a
  // `StorageFile` route would take. An `ifstream` wants what is after the scheme, with backslashes.
  constexpr std::wstring_view SCHEME = L"ms-appx:///";
  std::wstring relative{entry->packageUri.substr(SCHEME.size())};
  for (wchar_t& character : relative)
  {
    if (character == L'/')
    {
      character = L'\\';
    }
  }

  std::vector<std::byte> bytes;
  if (!Neuron::ReadPackageFile(relative, bytes))
  {
    Report(_log, "MESH " + std::string{_name} + " did not load -- the package may not carry it");
    return false;
  }

  Neuron::CmoMesh read;
  const Neuron::CmoFault fault = Neuron::ReadCmo(bytes, read);
  if (fault != Neuron::CmoFault::None)
  {
    Report(_log, "MESH " + std::string{_name} + " did not decode, fault " + std::to_string(static_cast<int>(fault)));
    return false;
  }

  if (!Outpost::LoadHullMesh(read, entry->longestUnits, _outMesh))
  {
    Report(_log, "MESH " + std::string{_name} + " decoded but would not load");
    return false;
  }
  return true;
}

/// ADR-008: a one-line text file in LocalState, falling back to the compiled-in default when it is
/// absent or unreadable. M0.22 owns the real thing and it belongs in a library with a suite; this
/// is the same mechanism in the shell so that the gate's two-machine run can point the Surface Pro
/// at a LAN address without a rebuild, which is the whole reason ADR-008 made it a file.
/// **M0.22 MOVED THE BODY OF THIS DOWN INTO `NeuronClient` (R20, ADR-008).** It used to read the
/// file here with an `ifstream`, which worked and which no suite could reach -- so the fallback
/// that decides whether the client starts at all was the one piece of it nobody could pin.
/// `Neuron::ReadHostAddress` is that same decision with `Neuron::HostAddressFromFileContents`
/// split out in front of it and a suite over the split.
///
/// What is left here is the comment, because the note above it is about the shell.

/// Log formatting and nothing else, which is why it is here rather than in a library (R20). M0.16
/// is a human reading this file beside the screen, and "filter 1" is a worse thing to read at that
/// moment than "point".
[[nodiscard]] const char* FilterName(Neuron::FitFilter _filter) noexcept
{
  switch (_filter)
  {
  case Neuron::FitFilter::None:
    return "none";
  case Neuron::FitFilter::Point:
    return "point";
  case Neuron::FitFilter::Bilinear:
    return "bilinear";
  }
  return "?";
}

/// Log formatting, like `FilterName` above: how many entities the frame drew between two samples, how
/// many it held at their newest, and how many had one sample. **Under ADR-024 holding is ordinary** for a
/// distant idle entity the accumulator refreshes rarely; at the MVP, where every entity is refreshed every
/// tick, a rising hold count is the playout buffer not doing its job and would make every latency figure
/// optimistic.
[[nodiscard]] std::string DrawnText(const Outpost::DrawnSummary& _summary)
{
  return "interp=" + std::to_string(_summary.interpolating) + " hold=" + std::to_string(_summary.holding) +
         " starved=" + std::to_string(_summary.starved);
}

/// Before there is a log file to write to, and therefore the only way to see how far this got on a
/// machine where it did not get far. It stays after the gate is over if the probe outlives it.
void Trace(const char* _marker) noexcept
{
  OutputDebugStringA(_marker);
  OutputDebugStringA("\n");
}

void RunProbe(const CoreWindow& _window)
{
  Trace("probe: entered");
  const std::filesystem::path localState{std::wstring{ApplicationData::Current().LocalFolder().Path()}};
  Trace("probe: have LocalState");

  // M1.15: a second client on this machine shares this folder, so the log and the session token are
  // named by the slot this instance claims. Slot zero is the names they always had.
  const std::uint32_t instanceSlot = Neuron::ClaimInstanceSlot();
  std::ofstream log{localState / Neuron::InstanceFileName(L"probe-log.txt", instanceSlot), std::ios::trunc};
  Trace(log ? "probe: log open" : "probe: log NOT open");

  const std::string host = Neuron::ReadHostAddress();
  Report(log, "probe: LocalState is " + localState.string() + ", instance slot " + std::to_string(instanceSlot));
  Report(log, "probe: host " + host + " port " + std::to_string(Neuron::ProbePacket::PORT));

  Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
  Neuron::DatagramTransport transport{queue};
  Neuron::TransportRecovery transportRecovery;
  if (!transport.Open(host, Neuron::ProbePacket::PORT))
  {
    Report(log, "probe: the socket could not be created at all");
    return;
  }

  // M0.16 MEASURES THE PANEL, and a windowed client measures something else: the swap chain is
  // created from the window's size exactly once, so at the default 1024 x 768 device-independent
  // pixels the gate takes its figures at 2048 x 1536 and writes them down as the Surface Pro's.
  // That is a wrong answer that looks like a right one, which is why the state is REPORTED below
  // rather than assumed. The request itself was made in `SetWindow`, because it had to precede
  // activation.
  //
  // THE TRANSITION IS NOT INSTANT. `Bounds()` keeps reporting the old size until the resize has
  // been dispatched, so the loop below pumps until the width stops changing rather than trusting a
  // fixed delay.
  {
    // THE REQUEST THAT MATTERS WAS ALREADY MADE, in `SetWindow`, before the view was activated --
    // `PreferFullScreenLaunch`. This one is the fallback, and on this shell it has never once
    // succeeded: asked before activation it is refused, asked after activation it is refused, and
    // waiting for `CoreWindow::Activated` in between does not help because THAT EVENT NEVER FIRES
    // HERE. A loop that waited for it burned 77 seconds and still reported "NOT activated".
    //
    // WHAT THOSE 77 SECONDS ACTUALLY WERE is worth writing down, because it looked like a hang in
    // every tool that was pointed at it: a packaged application that is not in the foreground is
    // SUSPENDED, and a suspended application blocks inside `ProcessEvents` rather than returning
    // from it. A five-second deadline around a call that does not return is not a deadline. The
    // probe therefore does not wait on the dispatcher for anything before it has a frame to draw.
    const CoreDispatcher settleDispatcher = _window.Dispatcher();
    static_cast<void>(Outpost::TryEnterFullScreen());

    // SETTLED IS A DURATION AND NOT AN ITERATION COUNT, and that distinction is the whole
    // correctness of this loop. `ProcessAllIfPresent` returns immediately when the queue is empty
    // and `yield` does not sleep, so counting iterations spins through sixty of them in well under
    // a millisecond -- before the resize this is waiting for has even been posted. It would then
    // report the pre-transition 1024 x 768 as settled and the gate would measure the wrong panel,
    // which is precisely the wrong answer that looks like a right one this block exists to prevent.
    const auto begun = std::chrono::steady_clock::now();
    auto stableSince = begun;
    float previousWidth = -1.0f;
    while (std::chrono::steady_clock::now() < (begun + SETTLE_TIMEOUT))
    {
      settleDispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

      const float width = _window.Bounds().Width;
      if (width != previousWidth)
      {
        previousWidth = width;
        stableSince = std::chrono::steady_clock::now();
      }
      else if ((std::chrono::steady_clock::now() - stableSince) >= SETTLE_STABLE_FOR)
      {
        break;
      }

      std::this_thread::sleep_for(SETTLE_POLL_INTERVAL);
    }
    Report(log, "probe: window settled at " + std::to_string(static_cast<int>(previousWidth)) + " dips wide after " +
                  std::to_string(MillisecondsSince(begun)) + " ms");

    // THE STATE, ASKED ONCE, AFTER THE TRANSITION -- not the request's return value, which says
    // only that the ask was accepted. This is the line that decides whether the figures below are
    // M0.16's or a window's.
    const bool fullScreen = Outpost::IsFullScreenMode();
    Report(log, std::string{"probe: fullscreen "} + (fullScreen ? "yes" : "NO"));
    if (!fullScreen)
    {
      // SAID PLAINLY, BECAUSE THE FAILURE IS SILENT OTHERWISE. Everything below still runs and
      // still produces well-formed figures; they are simply a windowed measurement, and a number in
      // ADR-016 that came from one is worse than no number at all.
      Report(log, "probe: NOT FULLSCREEN -- the frame times below are a window's and are NOT M0.16's figures");
    }
  }

  // M0.13: the device, the swap chain, and the one thing this client draws.
  //
  // THE SWAP CHAIN IS CREATED AT PHYSICAL PIXELS AND THAT IS THE WHOLE POINT (ADR-007). The
  // CoreWindow reports DIPs; the Surface Pro ships at 200%; a swap chain made from the reported
  // size is 1440 x 960 against a 2880 x 1920 panel, nothing fails, and the game is half resolution
  // on the one device it is for. WindowMetrics is that conversion and R18 requires a suite over it.
  const Neuron::WindowMetrics metrics{.widthDips = _window.Bounds().Width,
                                      .heightDips = _window.Bounds().Height,
                                      .rawPixelsPerViewPixel =
                                        static_cast<float>(DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel())};

  Neuron::GraphicsDevice device;
  Neuron::SwapChain swapChain;
  Neuron::SceneTarget sceneTarget;
  Neuron::PresentStep presentStep;
  Neuron::InterfacePass interfacePass;

  // M0.21b's world pass, and the client state it draws from. `ClientFrame` is `GameClient`'s and it
  // is where R20 draws its line: the drain, the decode, the interpolation clock and the marker
  // clearing are all behind it with a suite over them, and what is left up here is the order they
  // happen in and a Direct3D call.
  Neuron::WorldPass worldPass;

  // M1.9: the hulls. One `MeshBuffer` for each mesh that ships -- three hulls and, since M2.10b, one per
  // module level -- one instanced draw apiece, and a ring of per-instance buffers so a frame is never written while the graphics
  // processor is still reading the last one.
  Neuron::MeshPass meshPass;
  Neuron::InstanceRing instanceRing;
  std::array<Neuron::MeshBuffer, Outpost::SHIPPED_MESH_COUNT> meshBuffers;
  std::array<bool, Outpost::SHIPPED_MESH_COUNT> meshReady{};
  std::uint32_t instanceFrame = 0;

  // M2.4: THE ASTEROID FIELD. The five variants as read, kept on the processor, and one static buffer
  // per variant holding that variant's rocks already placed -- baked when the join names a field and
  // drawn through `meshPass` with one identity instance. The pair it was baked for is what says when
  // to bake again; a seed of zero with a count of zero is "never".
  std::array<Outpost::HullMesh, Outpost::ASTEROID_VARIANT_COUNT> asteroidVariants;
  std::array<bool, Outpost::ASTEROID_VARIANT_COUNT> asteroidLoaded{};
  std::array<Neuron::MeshBuffer, Outpost::ASTEROID_VARIANT_COUNT> fieldBuffers;
  std::array<bool, Outpost::ASTEROID_VARIANT_COUNT> fieldReady{};
  std::uint64_t bakedFieldSeed = 0;
  std::size_t bakedFieldPlayers = 0;

  // M2.8: where each rock is drawn, for the tap to aim at -- set with the bake, from the same looks.
  std::vector<Outpost::RockPickPoint> rockPicks;

  // M1.9b: THE SKY (ADR-019). Stars and nothing else: they upload once and are then drawn LAST with
  // the depth test on so they shade no pixel the fleet already covers.
  //
  // **IT CANNOT HAPPEN UNTIL THE JOIN REPLY LANDS**, because the field is seeded from the match (R23).
  Neuron::PointSprites pointSprites;

  // Q81: SHOTS, MINING AND UNLOADING, drawn as beams after the hulls. Its own ring, indexed like the meshes'.
  Neuron::BeamPass beamPass;
  std::uint32_t beamFrame = 0;
  std::vector<Neuron::BeamInstance> beams;
  bool meshesLoaded = false;
  Outpost::ClientFrame clientFrame;

  // R21's one path in. The seam turns `CoreWindow` pointer events into plain records on the frame's
  // own thread (R18), and everything downstream of that -- the pick, the order, the marker -- is
  // `GameClient`'s with a suite over it.
  Neuron::GestureSeam seam;

  // M1.8: the two halves of the camera. The gate applies `Interface.md` section 5's deadzones, its
  // latch and the tap slop; `CameraGesture` turns what comes out into ADR-018's single anchor solve.
  // Both are in libraries with suites over them, and what is left up here is which one to call.
  Neuron::ManipulationGate gate;
  Outpost::CameraGesture cameraGesture;

  // M1.10 and M1.11: what is selected, and what the last tap anchored -- which is all a double tap
  // needs, because the expansion matches on IDENTITY rather than on screen distance (ADR-017).
  Outpost::Selection selection;
  Outpost::WireIdentity lastTapIdentity = Outpost::NO_WIRE_IDENTITY;
  // Where the selection is when a hold recenters on it; kept across frames so a hold allocates nothing.
  std::vector<float> recenterX;
  std::vector<float> recenterY;

  // M1.12 to M1.14: THE INTERFACE. The atlas is rasterized once, on the first frame, because its upload
  // is recorded on the frame's command list; the renderer draws every plate and glyph as one instanced
  // call. `hud` is the frame the player last SAW, and it is what a tap is tested against -- a tap answers
  // to what was on the glass, not to what this frame is about to draw.
  Neuron::GlyphAtlas glyphAtlas;
  Neuron::TextRenderer textRenderer;
  bool atlasAttempted = false;
  Outpost::HudFrame hud;

  // M3.3b: the alerts as last drawn, which is what a tap on one is resolved against.
  std::vector<Outpost::AlertPlacement> shownAlerts;
  std::vector<Neuron::GlyphQuad> hudQuads;
  Outpost::QuitConfirm quitConfirm;

  // M2.7: the credits panel's change flash (Q36), fed the balance each frame the own block is held.
  Outpost::CreditFlash creditFlash;
  bool quitConfirmed = false;

  // `Interface.md` section 4: a tap on your own station opens the build panel and leaves the selection
  // alone, so "the station is selected" is its own client-local fact rather than a selection entry.
  bool buildPanelOpen = false;

  // M2.11: the armed module, if one is. It lives only while the build panel is open -- the station being
  // selected is the arming's precondition (`Interface.md` section 6) -- and is dropped the moment it closes.
  Outpost::ModuleArming moduleArming;

  // OpenQuestions.md Q33: right-handed is the default. This is the one value that swaps the two bottom
  // panels, and until there is a settings surface it is a constant here.
  constexpr bool LEFT_HANDED = false;

  // THE TWO FITS (ADR-016), and the one place this shell asks for either. They are computed once
  // because nothing here resizes: the scene target is created from the swap chain's size and both
  // are fixed for the life of the probe. A resize path belongs with the frame loop at M0.22.
  //
  // THEY ARE TWO VALUES AND NOT ONE, which is the whole of ADR-016 section 2. The world fit maps the
  // scene target into the back buffer; the interface fit maps authored layout space into it. At the
  // 1:1 world default the first is identity, so an interface drawn through it would land at half
  // size in one corner.
  Neuron::FitTransform worldFit{};
  Neuron::FitTransform interfaceFit{};

  if (!device.Create())
  {
    Report(log, "probe: no Direct3D 12 device, hresult " + std::to_string(device.LastHresult()));
  }
  else if (!swapChain.Create(device, winrt::get_unknown(_window), Neuron::PhysicalWidth(metrics), Neuron::PhysicalHeight(metrics)))
  {
    Report(log, "probe: no swap chain, hresult " + std::to_string(swapChain.LastHresult()));
  }
  else if (!sceneTarget.Create(device, {.widthPixels = Neuron::WorldTargetWidthPixels(swapChain.WidthPixels()),
                                        .heightPixels = Neuron::WorldTargetHeightPixels(swapChain.HeightPixels())}))
  {
    Report(log, "probe: no scene target, hresult " + std::to_string(sceneTarget.LastHresult()));
  }
  else if (!presentStep.Create(device, swapChain))
  {
    // ADR-012's owed measurement fails here or nowhere: this is the first time a driver is asked to
    // accept the Shader Model 6.7 blob the build compiled.
    Report(log, "probe: no present step, hresult " + std::to_string(presentStep.LastHresult()));
  }
  else if (!interfacePass.Create(device, swapChain))
  {
    Report(log, "probe: no interface pass, hresult " + std::to_string(interfacePass.LastHresult()));
  }
  else if (!meshPass.Create(device, sceneTarget))
  {
    Report(log, "probe: no mesh pass, hresult " + std::to_string(meshPass.LastHresult()));
  }
  else if (!instanceRing.Create(device, MAXIMUM_INSTANCES + FIELD_INSTANCES + static_cast<std::uint32_t>(Outpost::MAX_WRECKS)))
  {
    Report(log, "probe: no instance ring, hresult " + std::to_string(instanceRing.LastHresult()));
  }
  else if (!worldPass.Create(device, sceneTarget))
  {
    // M0.21b. It is created from the SCENE TARGET rather than the swap chain, because that is what
    // it renders into and a pipeline state has to agree with its target's formats and sample count.
    Report(log, "probe: no world pass, hresult " + std::to_string(worldPass.LastHresult()));
  }
  else if (!pointSprites.Create(device, sceneTarget))
  {
    Report(log, "probe: no point sprites, hresult " + std::to_string(pointSprites.LastHresult()));
  }
  else if (!textRenderer.Create(device, swapChain))
  {
    Report(log, "probe: no text renderer, hresult " + std::to_string(textRenderer.LastHresult()));
  }
  else
  {
    worldFit = Neuron::ComputeFit(sceneTarget.WidthPixels(), sceneTarget.HeightPixels(), swapChain.WidthPixels(), swapChain.HeightPixels());
    interfaceFit = Neuron::ComputeInterfaceFit(swapChain.WidthPixels(), swapChain.HeightPixels());

    // NOT IN THE CHAIN ABOVE: a game without beams is still a game, and a failure here must not take the
    // text renderer and the gesture seam down with it.
    if (!beamPass.Create(device, sceneTarget, BEAM_CAPACITY))
    {
      Report(log, "probe: no beam pass, hresult " + std::to_string(beamPass.LastHresult()));
    }

    // THE INTERFACE FIT AND NOT THE WORLD'S (ADR-016), which is the defect that ADR worries about:
    // at the 1:1 world default the world fit is identity and every tap would land in the top left
    // quarter of the frame.
    const Neuron::AuthoredSpace authoredSpace{.interfaceFit = interfaceFit, .rawPixelsPerViewPixel = metrics.rawPixelsPerViewPixel};
    if (!seam.Attach(winrt::get_unknown(_window), authoredSpace))
    {
      Report(log, "probe: the gesture seam did not attach -- there is no way in");
    }

    // Major and minor out of the packed nibbles, because "0x" in front of a decimal is a lie.
    const std::uint32_t shaderModel = device.HighestShaderModel();
    Report(log, "probe: highest shader model " + std::to_string((shaderModel >> 4) & 0xF) + "." + std::to_string(shaderModel & 0xF));
    Report(log, "probe: swap chain " + std::to_string(swapChain.WidthPixels()) + "x" + std::to_string(swapChain.HeightPixels()) +
                  " physical, from " + std::to_string(static_cast<int>(metrics.widthDips)) + "x" +
                  std::to_string(static_cast<int>(metrics.heightDips)) + " dips at " + std::to_string(metrics.rawPixelsPerViewPixel) + "x");

    // The figures M0.16 looks at, written down rather than inferred from the picture.
    Report(log, "probe: scene target " + std::to_string(sceneTarget.WidthPixels()) + "x" + std::to_string(sceneTarget.HeightPixels()) +
                  " at " + std::to_string(Neuron::WORLD_SCALE_NUMERATOR) + "/" + std::to_string(Neuron::WORLD_SCALE_DENOMINATOR) +
                  " scale, " + std::to_string(Neuron::WORLD_SAMPLE_COUNT) + " sample");
    Report(log, "probe: world fit scale " + std::to_string(worldFit.scale) + " filter " + FilterName(worldFit.filter) + " rect " +
                  std::to_string(worldFit.width) + "x" + std::to_string(worldFit.height) + " at " + std::to_string(worldFit.offsetX) + "," +
                  std::to_string(worldFit.offsetY));
    Report(log, "probe: interface fit scale " + std::to_string(interfaceFit.scale) + " rect " + std::to_string(interfaceFit.width) + "x" +
                  std::to_string(interfaceFit.height) + " at " + std::to_string(interfaceFit.offsetX) + "," +
                  std::to_string(interfaceFit.offsetY));

    // M0.17 IS CONFIRMED AGAINST THIS LINE AND NOT AGAINST THE PICTURE ALONE. On the panel the
    // authored 16,896 48 x 48 target has to read 32,1792 96 x 96 here, and the square on the glass
    // has to be where those numbers say it is.
    const Neuron::PhysicalRect probeRect = Neuron::MapAuthoredRect(interfaceFit, INTERFACE_PROBE_RECT);
    Report(log, "probe: interface rect authored " + std::to_string(INTERFACE_PROBE_RECT.WidthPixels()) + "x" +
                  std::to_string(INTERFACE_PROBE_RECT.HeightPixels()) + " at " + std::to_string(INTERFACE_PROBE_RECT.left) + "," +
                  std::to_string(INTERFACE_PROBE_RECT.top) + " reaches " + std::to_string(probeRect.WidthPixels()) + "x" +
                  std::to_string(probeRect.HeightPixels()) + " at " + std::to_string(probeRect.left) + "," + std::to_string(probeRect.top));
  }

  bool running = true;
  const auto closed = _window.Closed(winrt::auto_revoke, [&running](const auto&, const auto&) { running = false; });
  std::uint64_t presentedFrames = 0;

  // M0.16's four figures come from here. The arithmetic is in NeuronClient with a suite over it
  // (R20); this shell only feeds it what the device reports.
  Neuron::FrameStatistics gpuTime;

  // `TechnicalDesign.md` section 9.6: the same window of frames, split at the two marks.
  Neuron::FrameStatistics worldGpuTime;
  Neuron::FrameStatistics presentGpuTime;
  Neuron::FrameStatistics interfaceGpuTime;

  const CoreDispatcher dispatcher = _window.Dispatcher();
  const auto start = std::chrono::steady_clock::now();
  // The token from `LocalState`, or zero on a first run. It is read once: the file is only
  // written again when the host issues a different one.
  clientFrame.MutableJoin().Begin(Neuron::ReadSessionToken(instanceSlot));
  bool reportedReady = false;
  bool reportedSeat = false;
  std::uint64_t receivedCount = 0;

  // M0.23'S MEASUREMENT, AND IT IS AN OBSERVATION RATHER THAN AN ARITHMETIC. The rule -- what arms
  // it, what counts as visible -- is `GameClient/TapLatencyProbe.h`'s, with a suite over it (R20);
  // this file feeds it the drawn pose each frame and the tap when it lands, and logs what comes back.
  // `Scripts/ProbeReport.py` turns a run of those into a distribution; nothing here averages anything.
  //
  // WHY IT IS THE DRAWN POSE AND NOT THE SNAPSHOT'S. The gate's question is what the PLAYER sees, and
  // the client renders 75 ms behind the newest update -- so an update that already carries the move
  // is still not the moment the ship appears to move.
  Outpost::TapLatencyProbe tapProbe;

  // THE FRAME'S ENTITIES, INTERPOLATED, kept across frames so steady state allocates nothing. Filled by
  // `ClientFrame::Advance` (ADR-024: per entity, between its own two samples).
  std::vector<Outpost::EntityRecord> drawnRecords;

  while (running)
  {
    dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

    const std::uint64_t nowMs = MillisecondsSince(start);

    // **A FAILED TRANSPORT IS REOPENED, NOT FATAL.** This loop used to break here, which turned a socket
    // closed under a suspended client into a client that walked out of its match on resume. The cadence
    // is `TransportRecovery`'s; the rejoin that follows needs nothing from here, because a new socket is
    // a new endpoint the host never hears from, and `ClientFrame`'s silence detector rejoins with the
    // token it holds.
    if (transportRecovery.ShouldReopen(transport.State(), nowMs))
    {
      Report(log, "probe: the transport failed, reopen " + std::to_string(transportRecovery.ReopenCount()) +
                    " -- if it never comes up, that is usually the capability or the exemption");
      transport.Close();
      static_cast<void>(transport.Open(host, Neuron::ProbePacket::PORT));
      reportedReady = false;
    }

    const Neuron::TransportState state = transport.State();
    if (state == Neuron::TransportState::Ready && !reportedReady)
    {
      Report(log, "probe: transport ready");
      reportedReady = true;
    }

    // ADR-013's join, and the arithmetic that decides when is `JoinState`'s rather than this
    // loop's -- which is the same split the gesture seam and the interpolation clock take.
    if ((state == Neuron::TransportState::Ready) && clientFrame.MutableJoin().ShouldSend(nowMs))
    {
      std::array<std::byte, QUEUE_SLOT_BYTES> outgoingJoin{};
      Neuron::ByteWriter joinWriter{outgoingJoin};
      if (Outpost::Encode(clientFrame.CurrentJoin().Outgoing(), joinWriter))
      {
        const bool sent = transport.Send(std::span<const std::byte>{outgoingJoin.data(), joinWriter.WrittenBytes()});
        Report(log, "TX join attempt=" + std::to_string(clientFrame.CurrentJoin().SentCount()) +
                      " token=" + std::to_string(clientFrame.CurrentJoin().Token()) + (sent ? " sent" : " refused"));
      }
    }

    // **THE VIEW, WHEN THERE IS NOTHING TO ORDER** (ADR-024). An empty command packet carries it, on a
    // cadence `ClientFrame` decides; the host's accumulator scores relevance against it. Not logged: it
    // goes four times a second and says nothing a reader of the log needs.
    if ((state == Neuron::TransportState::Ready) && clientFrame.ShouldReportView(nowMs))
    {
      Outpost::CommandPacket report{.sequence = 0, .player = clientFrame.Player(), .commands = {}};
      clientFrame.StampView(report);
      // **AND EVERY COMMAND STILL UNACKNOWLEDGED** (ADR-003, the 2026-09-23 review's M5), so an order lost on
      // the way goes again within a quarter of a second whether or not the player taps.
      static_cast<void>(clientFrame.FillOutstanding(report));
      std::array<std::byte, QUEUE_SLOT_BYTES> outgoingView{};
      Neuron::ByteWriter viewWriter{outgoingView};
      if (Outpost::Encode(report, viewWriter))
      {
        static_cast<void>(transport.Send(std::span<const std::byte>{outgoingView.data(), viewWriter.WrittenBytes()}));
      }
    }

    // ONE DRAIN PER FRAME, AND IT IS `ClientFrame`'S (R20). Everything that used to be written out
    // here -- read a datagram, decode it, decide what to do with it -- is behind that class with a
    // suite over it, and this is the call plus a line in the log.
    const Outpost::ClientFrame::DrainResult drained = clientFrame.DrainPackets(queue, nowMs);

    // M3.4: A SHIP THAT DIED LEAVES THE SELECTION ON THE FRAME ITS REMOVAL ARRIVES, not on the next tap, so the
    // panel never shows the dead.
    static_cast<void>(selection.RetainLiving(clientFrame.Replicas().Entities()));
    clientFrame.Wrecks().Expire(nowMs);
    clientFrame.Alerts().Expire(nowMs);

    // M3.8: A MATCH ENDED. The frame has cleared what it derived and is joining the next; what the app holds goes
    // too, and the seat is reported again so the camera opens on the new station and the sky is the new seed's.
    if (drained.matchEnded)
    {
      selection.Clear();
      moduleArming.Disarm();
      buildPanelOpen = false;
      reportedSeat = false;
      if (const Outpost::MatchEnded* ended = clientFrame.ShownResult(nowMs); ended != nullptr)
      {
        Report(log, "MATCH " + std::to_string(ended->matchNumber) + " ended, " +
                      ((ended->winner == Outpost::NO_PLAYER) ? std::string{"a draw"}
                                                             : "won by player " + std::to_string(static_cast<unsigned>(ended->winner))) +
                      (ended->onClock ? " on the clock" : "") + "; joining the next");
      }
    }

    // `Interface.md` section 7's reconnect, decided in `ClientFrame` and only reported here. The rejoin it
    // started goes out through the same `ShouldSend` above on the next frame.
    if (drained.linkLost)
    {
      // **A FRESH SOCKET WITH EVERY REJOIN.** After a suspension the old one may be closed, or open and
      // deaf, and nothing distinguishes a deaf socket from a quiet host -- so it is not trusted. The new
      // one is a new endpoint, which is exactly what ADR-013's token exists to carry a player across.
      transport.Close();
      static_cast<void>(transport.Open(host, Neuron::ProbePacket::PORT));
      reportedReady = false;
      Report(log, "LINK lost after " + std::to_string(Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS) +
                    " ms of silence, reopening the socket and rejoining");
    }
    if (drained.commandsExpired > 0)
    {
      Report(log, "TX gave up on " + std::to_string(drained.commandsExpired) + " unacknowledged command(s) after " +
                    std::to_string(Outpost::ClientFrame::COMMAND_RESEND_WINDOW_MILLISECONDS) + " ms");
    }
    if (drained.linkRestored)
    {
      Report(log, std::string{"LINK restored, "} + (clientFrame.CurrentJoin().Resumed() ? "rejoined" : "accepted") +
                    " player=" + std::to_string(static_cast<unsigned>(clientFrame.Player())));
    }

    if (drained.tokenChanged)
    {
      // **A FAILED WRITE IS LOGGED AND NOT ACTED ON.** ADR-013 names what it costs -- a client
      // that takes a second slot after a relaunch -- and there is nothing better to do about it
      // here than say so.
      const bool written = Neuron::WriteSessionToken(clientFrame.CurrentJoin().Token(), instanceSlot);
      Report(log, std::string{"SESSION token "} + (written ? "stored" : "COULD NOT BE STORED"));
    }
    if (!reportedSeat && (clientFrame.CurrentJoin().Phase() != Outpost::JoinPhase::Joining))
    {
      reportedSeat = true;
      const Outpost::JoinState& join = clientFrame.CurrentJoin();

      // **THE CAMERA OPENS ON YOUR OWN STATION**, which it could not do before: the opening pose was
      // M0's convenience because nothing knew which player this was until the join answered. It is
      // the same recenter a hold performs (ADR-018), done once, at the moment there is an answer.
      if (join.IsJoined())
      {
        // THE HOST'S COUNT, FROM THE REPLY (M2.3). This read 2 until the join carried one, which put a
        // four-player match's second player on the far side of the map from their station.
        const Neuron::Vec2 home = Outpost::StartAnchor(join.PlayerCount(), join.Player());
        clientFrame.Camera() = Outpost::Recenter(
          clientFrame.Camera(), Outpost::RecenterRequest{.stationX = static_cast<float>(home.x) / static_cast<float>(Neuron::FIXED_ONE),
                                                         .stationY = static_cast<float>(home.y) / static_cast<float>(Neuron::FIXED_ONE)});
      }
      Report(log, "JOIN " +
                    std::string{join.IsJoined() ? (join.Resumed() ? "rejoined" : "accepted")
                                                : (join.FieldMismatch() ? "REFUSED: the host's map differs" : "REFUSED: match full")} +
                    " player=" + std::to_string(static_cast<unsigned>(join.Player())) + " of " + std::to_string(join.PlayerCount()) +
                    " seed=" + std::to_string(join.MatchSeed()) + " field=" + std::to_string(clientFrame.Field().Rocks().size()) +
                    " rocks");

      // **THE SEED HAS JUST ARRIVED, SO THIS IS THE FIRST MOMENT THE SKY EXISTS** (R23, ADR-019).
      // Generated once and never updated: the field takes no time input at all, so the upload below
      // is read unchanged for the rest of the match and there is no ring behind it.
      if (join.IsJoined() && pointSprites.IsReady())
      {
        const std::vector<Neuron::StarInstance> stars = Outpost::ShippedStarInstances(join.MatchSeed());
        if (pointSprites.Upload(device, stars))
        {
          Report(log, "SKY " + std::to_string(pointSprites.StarCount()) + " stars uploaded");
        }
        else
        {
          Report(log, "SKY FAILED to upload stars, hresult " + std::to_string(pointSprites.LastHresult()));
        }
      }
    }

    if (drained.datagrams > 0)
    {
      receivedCount += drained.accepted;
      // The same query the draw makes a few lines later, into the same vector, so it allocates nothing
      // once the vector has grown; asking twice is cheaper than carrying the answer across half a frame.
      const Outpost::DrawnSummary shown = clientFrame.Advance(nowMs, drawnRecords);
      Report(log, "RX datagrams=" + std::to_string(drained.datagrams) + " accepted=" + std::to_string(drained.accepted) +
                    " refused=" + std::to_string(drained.refused) + " faulted=" + std::to_string(drained.faulted) + " tick=" +
                    std::to_string(clientFrame.Replicas().NewestTick()) + " lost=" + std::to_string(clientFrame.Replicas().LostCount()) +
                    " entities=" + std::to_string(clientFrame.Replicas().HeldCount()) +
                    // THE DRAWN POSITION, because the last run could not answer "did it move?" from
                    // the log alone. Working that out took reading the host's uptime against the
                    // entity's journey time, to find that the ship had already arrived before the
                    // client started and that nothing moving was correct rather than a defect.
                    " drawn=" + std::to_string(tapProbe.Current().xUnits) + "," + std::to_string(tapProbe.Current().yUnits) +
                    " awaiting=" + std::to_string(tapProbe.Armed() ? 1 : 0) +
                    // WHAT THE PLAYOUT CLOCK IS ACTUALLY DOING, per entity. At the MVP every entity is
                    // refreshed every tick, so anything but interpolating in a steady run is the buffer
                    // not covering the delay, and every latency figure taken from it would be optimistic.
                    " " + DrawnText(shown) + " local_ms=" + std::to_string(nowMs));
    }

    // === M0.21's VERB, WIRED (M0.22). ==========================================================
    //
    // One drain of the seam per frame, on the frame's thread. A tap on empty space with something
    // selected is a move order: it is SENT at once and the client draws a marker for it
    // immediately (Q20), because a tap is not visible for 152 ms at best and on a touchscreen the
    // tap is the only feedback there is.
    //
    // NOTHING IS PREDICTED. The order goes to the host and the entity keeps the position the last
    // snapshot gave it until a later snapshot moves it. R19 forbids the client simulating, not the
    // client drawing what it asked for.
    std::array<Neuron::InputEvent, Neuron::GestureSeam::EVENT_CAPACITY> events{};
    const std::size_t eventCount = seam.Drain(events);
    for (std::size_t index = 0; index < eventCount; ++index)
    {
      const Neuron::InputEvent& event = events[index];

      // **THE CAMERA, M1.8.** A manipulation is fed through the gate and then through one anchor
      // solve; a hold recenters. Neither is a tap, so both leave before the tap handling below.
      if (event.kind != Neuron::InputEventKind::Tapped)
      {
        const Neuron::GatedManipulation gated = Neuron::ApplyGate(gate, event);
        const float cameraAspect = (sceneTarget.HeightPixels() > 0)
                                     ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                     : 1.5f;

        if (event.kind == Neuron::InputEventKind::ManipulationStarted)
        {
          static_cast<void>(cameraGesture.Begin(clientFrame.Camera(), cameraAspect, event.xAuthoredPixels, event.yAuthoredPixels));
        }
        else if (event.kind == Neuron::InputEventKind::ManipulationUpdated)
        {
          clientFrame.Camera() = cameraGesture.Update(gated, cameraAspect);
        }
        else if (event.kind == Neuron::InputEventKind::ManipulationCompleted)
        {
          clientFrame.Camera() = Outpost::CameraGesture::Complete(cameraGesture.Update(gated, cameraAspect));
          cameraGesture.End();
          Report(log, "CAMERA focus=" + std::to_string(clientFrame.Camera().focusX) + "," + std::to_string(clientFrame.Camera().focusY) +
                        " distance=" + std::to_string(clientFrame.Camera().distance) +
                        " heading=" + std::to_string(clientFrame.Camera().headingRadians));
        }
        else if (event.kind == Neuron::InputEventKind::Holding)
        {
          // **A HOLD ON EMPTY SPACE RECENTERS** (ADR-018 decision 7): on the selection when there is one, on
          // this player's station when there is not. It went to the station unconditionally until the
          // 2026-09-23 review (m9) -- written before selection existed and never revisited -- which left
          // panning as the only way back to your own fleet in the game's normal state.
          const Neuron::Vec2 station = Outpost::StartAnchor(clientFrame.CurrentJoin().PlayerCount(), clientFrame.Player());
          selection.PositionsOf(clientFrame.Replicas().Entities(), recenterX, recenterY);
          clientFrame.Camera() =
            Outpost::Recenter(clientFrame.Camera(),
                              Outpost::RecenterRequest{.selectionX = recenterX,
                                                       .selectionY = recenterY,
                                                       .stationX = static_cast<float>(station.x) / static_cast<float>(Neuron::FIXED_ONE),
                                                       .stationY = static_cast<float>(station.y) / static_cast<float>(Neuron::FIXED_ONE)});
          Report(log, recenterX.empty() ? std::string{"RECENTER on the station"}
                                        : "RECENTER on " + std::to_string(recenterX.size()) + " selected");
        }
        continue;
      }

      // THE TRUTH AS LAST TOLD, NOT THE INTERPOLATED PICTURE: the newest record of every entity held.
      const std::span<const Outpost::EntityRecord> known = clientFrame.Replicas().Entities();

      // === THE INTERFACE FIRST (`design_handoff_hud` *Pick order*). ============================
      //
      // A tap that lands on a panel is the panel's, target or not, and never reaches the world: a tap
      // on the credit balance that fell through would be a move order to wherever the balance is drawn.
      const Outpost::HudHit hudHit = hud.hits.Test(event.xAuthoredPixels, event.yAuthoredPixels);
      if (hudHit.consumed)
      {
        const auto design = static_cast<Outpost::DesignId>(hudHit.argument);
        switch (hudHit.action)
        {
        case Outpost::HudAction::SelectGroup:
          Report(log, "HUD narrowed to " + std::to_string(Outpost::NarrowToDesign(selection, known, design)) + " of design " +
                        std::to_string(hudHit.argument));
          break;

        case Outpost::HudAction::ClearSelection:
          // **THE ONLY WAY TO DESELECT** (`Interface.md` section 4), and it takes the build panel with it:
          // "everything" includes the station.
          selection.Clear();
          buildPanelOpen = false;
          Report(log, "HUD cleared the selection");
          break;

        case Outpost::HudAction::Build:
        case Outpost::HudAction::CancelBuild:
          if (clientFrame.CurrentJoin().IsJoined() && (state == Neuron::TransportState::Ready))
          {
            const std::uint16_t sequence = clientFrame.TakeCommandSequence();
            const Outpost::CommandType type =
              (hudHit.action == Outpost::HudAction::Build) ? Outpost::CommandType::Build : Outpost::CommandType::CancelBuild;
            Outpost::CommandPacket packet{.sequence = sequence, .player = clientFrame.Player(), .commands = {}};
            clientFrame.StampView(packet);
            clientFrame.IssueCommand(Outpost::BuildStationCommand(sequence, type, design), nowMs);
            static_cast<void>(clientFrame.FillOutstanding(packet));

            std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
            Neuron::ByteWriter commandWriter{outgoing};
            const bool sent = Outpost::Encode(packet, commandWriter) &&
                              transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});
            Report(log,
                   std::string{"HUD "} +
                     ((type == Outpost::CommandType::Build) ? "build design " + std::to_string(hudHit.argument) : std::string{"cancel"}) +
                     " seq=" + std::to_string(sequence) + (sent ? " sent" : " NOT SENT"));
          }
          break;

        case Outpost::HudAction::ArmModule:
          // **ARMING SENDS NOTHING.** The tap on the plane that follows is the order (M2.11).
          moduleArming.Toggle(design);
          Report(log, std::string{"HUD module "} + std::to_string(hudHit.argument) + (moduleArming.IsArmed() ? " armed" : " disarmed"));
          break;

        case Outpost::HudAction::ArmQuit:
          quitConfirm.Arm(nowMs);
          Report(log, "HUD quit armed");
          break;

        case Outpost::HudAction::StayInMatch:
          quitConfirm.Disarm();
          Report(log, "HUD quit disarmed");
          break;

        case Outpost::HudAction::ConfirmQuit:
          // Checked again rather than trusted: the confirm is drawn from last frame's state, and four
          // seconds can have run out between the draw and the tap.
          if (quitConfirm.IsArmed(nowMs))
          {
            Report(log, "HUD quit confirmed");
            quitConfirmed = true;
            running = false;
          }
          break;

        case Outpost::HudAction::RecenterOnAlert:
          // THE HANDOFF'S RULE 7: THE CAMERA GOES WHERE THE ALERT POINTS, through the same recenter a hold performs.
          if (hudHit.argument < shownAlerts.size())
          {
            const Outpost::AlertPlacement& alert = shownAlerts[hudHit.argument];
            clientFrame.Camera() =
              Outpost::Recenter(clientFrame.Camera(), Outpost::RecenterRequest{.stationX = alert.worldX, .stationY = alert.worldY});
            Report(log, "ALERT tapped, recentering on " + std::to_string(alert.worldX) + "," + std::to_string(alert.worldY));
          }
          break;

        case Outpost::HudAction::None:
          break;
        }
        continue;
      }

      if (known.empty())
      {
        Report(log, "TAP at " + std::to_string(event.xAuthoredPixels) + "," + std::to_string(event.yAuthoredPixels) +
                      " -- nothing is replicated yet, so nothing is selected");
        continue;
      }

      // THE ASPECT IS THE SCENE TARGET'S, NOT THE AUTHORED FRAME'S, and they answer different
      // questions. The authored width and height convert the tap's position into the normalized
      // range; the ASPECT builds the ray, and it has to be the one the world was DRAWN with or a
      // tap resolves to a different world point than the pixel the player touched.
      //
      // On the target device the two coincide -- a 3:2 panel, an authored 3:2 frame, fullscreen at
      // launch (Q23) -- which is exactly why passing the wrong one would go unnoticed here and
      // surface on the first window that is not 3:2.
      const float tapAspect = (sceneTarget.HeightPixels() > 0)
                                ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                : 1.0f;

      // **THE SELECTION DROPS WHAT THE NEWEST SNAPSHOT NO LONGER CARRIES** before anything is
      // resolved against it: a selected ship that died is a selection the player cannot act on.
      static_cast<void>(selection.RetainLiving(known));

      const Outpost::HitTestRequest request{.authoredX = event.xAuthoredPixels,
                                            .authoredY = event.yAuthoredPixels,
                                            .authoredWidth = static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH),
                                            .authoredHeight = static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT),
                                            .aspectRatio = tapAspect,
                                            .player = clientFrame.Player()};

      // === M2.11: AN ARMED TAP IS THE PLACEMENT'S. ================================================
      //
      // Resolved in `GameClient` against the host's own rule (`ResolvePlacementTap`, R20); what is left here is
      // sending what it decided. Only a tap on one of your own ships falls through to the selection below, and
      // that selection closes the build panel and the arming with it.
      if (!buildPanelOpen)
      {
        moduleArming.Disarm();
      }
      if (moduleArming.IsArmed())
      {
        const Outpost::PlacementOutcome placement =
          Outpost::ResolvePlacementTap(clientFrame.Camera(), request, known, rockPicks, moduleArming.Armed());
        if ((placement.action == Outpost::PlacementAction::Place) || (placement.action == Outpost::PlacementAction::Upgrade))
        {
          if (!clientFrame.CurrentJoin().IsJoined())
          {
            Report(log, "TAP ignored -- not seated yet");
            continue;
          }
          const std::uint16_t sequence = clientFrame.TakeCommandSequence();
          Outpost::CommandPacket packet{.sequence = sequence, .player = clientFrame.Player(), .commands = {}};
          clientFrame.StampView(packet);
          clientFrame.IssueCommand((placement.action == Outpost::PlacementAction::Place)
                                     ? Outpost::BuildPlaceModuleCommand(sequence, placement.site, moduleArming.Armed())
                                     : Outpost::BuildUpgradeModuleCommand(sequence, placement.module, moduleArming.Armed()),
                                   nowMs);
          static_cast<void>(clientFrame.FillOutstanding(packet));

          std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
          Neuron::ByteWriter commandWriter{outgoing};
          const bool sent = Outpost::Encode(packet, commandWriter) &&
                            transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});
          Report(log, std::string{(placement.action == Outpost::PlacementAction::Place) ? "PLACE module " : "UPGRADE module "} +
                        std::to_string(static_cast<int>(moduleArming.Armed())) + " seq=" + std::to_string(sequence) +
                        (sent ? " sent" : " NOT SENT"));
          // ONE ORDER PER ARMING: the next module is a fresh choice in the panel.
          moduleArming.Disarm();
          continue;
        }
        if (placement.action == Outpost::PlacementAction::Nothing)
        {
          Report(log, "PLACE refused by the preview, fault " + std::to_string(static_cast<int>(placement.fault)));
          continue;
        }
        moduleArming.Disarm();
      }

      const Outpost::SelectionOutcome picked = selection.Tap(clientFrame.Camera(), request, known, rockPicks);

      // **THE FIRST TAP HAS ALREADY ACTED; THE SECOND ONLY UPGRADES IT** (ADR-017). Nothing is
      // deferred waiting to see whether a second tap arrives, so no tap in this game got slower.
      if ((event.tapCount >= 2) && (picked.verb == Outpost::OrderVerb::Select))
      {
        const Outpost::ExpansionOutcome expanded =
          Outpost::ExpandSelection(selection, clientFrame.Camera(), request, known, lastTapIdentity, picked.target);
        if (expanded.expanded)
        {
          Report(log, "SELECT expanded to " + std::to_string(expanded.selected) + " of one design");
        }
      }
      if (picked.verb == Outpost::OrderVerb::OpenBuildPanel)
      {
        // **THE SELECTION IS UNCHANGED** (`Interface.md` section 4): this row of the table is about the
        // interface rather than the world. A second tap on the station closes the panel again.
        buildPanelOpen = !buildPanelOpen;
        Report(log, std::string{"BUILD panel "} + (buildPanelOpen ? "opened" : "closed"));
        continue;
      }
      if (picked.verb == Outpost::OrderVerb::Select)
      {
        // A ship replaced the selection, so the station is no longer what is selected.
        buildPanelOpen = false;
        lastTapIdentity = picked.target;
        Report(log, "SELECT " + std::to_string(picked.target) + ", " + std::to_string(selection.Count()) + " selected");
        continue;
      }

      // === M2.8: A TAP ON AN ASTEROID. ============================================================
      //
      // **THE ONE ROW OF THE TABLE THAT SPLITS A SELECTION** (`Interface.md` section 4): the miners take a
      // standing mine order and the rest move to the rock, from one gesture, as two commands in one packet.
      // Which is which is `SplitForMine`'s, in `GameClient`, where a suite pins it (R20).
      if (picked.verb == Outpost::OrderVerb::Mine)
      {
        if (!clientFrame.CurrentJoin().IsJoined())
        {
          Report(log, "TAP ignored -- not seated yet");
          continue;
        }

        const Outpost::MineSplit split = Outpost::SplitForMine(selection.Identities(), known);
        std::vector<Outpost::Command> issued;
        if (!split.miners.empty())
        {
          issued.push_back(Outpost::BuildMineCommand(clientFrame.TakeCommandSequence(), picked.rock, split.miners));
        }
        if (!split.others.empty())
        {
          issued.push_back(Outpost::BuildMoveCommand(clientFrame.TakeCommandSequence(), picked.worldX, picked.worldY, split.others));
        }
        if (issued.empty())
        {
          continue;
        }
        for (const Outpost::Command& command : issued)
        {
          clientFrame.IssueCommand(command, nowMs);
        }
        Outpost::CommandPacket packet{.sequence = issued.front().sequence, .player = clientFrame.Player(), .commands = {}};
        clientFrame.StampView(packet);
        static_cast<void>(clientFrame.FillOutstanding(packet));

        std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
        Neuron::ByteWriter commandWriter{outgoing};
        const bool sent = Outpost::Encode(packet, commandWriter) &&
                          transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});

        // ONE MARKER PER COMMAND, at the rock's place on the plane, each cleared by its own acknowledgment.
        for (const Outpost::Command& command : issued)
        {
          Outpost::OrderMarker marker;
          marker.targetX = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(picked.worldX * static_cast<float>(Neuron::FIXED_ONE)));
          marker.targetY = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(picked.worldY * static_cast<float>(Neuron::FIXED_ONE)));
          marker.commandSequence = command.sequence;
          marker.selection = command.selection;
          clientFrame.Markers().Add(marker);
        }
        Report(log, "MINE rock " + std::to_string(picked.rock) + ": " + std::to_string(split.miners.size()) + " mining, " +
                      std::to_string(split.others.size()) + " moving" + (sent ? "" : " NOT SENT"));
        continue;
      }

      if (picked.verb == Outpost::OrderVerb::Attack)
      {
        // **A TAP ON A HOSTILE WITH SHIPS SELECTED** (M3.2, `Interface.md` section 4): the host sends what can
        // fight to a standoff arc around it (Q67). One command, the whole selection.
        if (!clientFrame.CurrentJoin().IsJoined())
        {
          Report(log, "TAP ignored -- not seated yet");
          continue;
        }
        const Outpost::Command attack =
          Outpost::BuildAttackCommand(clientFrame.TakeCommandSequence(), picked.target, selection.Identities());
        clientFrame.IssueCommand(attack, nowMs);
        Outpost::CommandPacket packet{.sequence = attack.sequence, .player = clientFrame.Player(), .commands = {}};
        clientFrame.StampView(packet);
        static_cast<void>(clientFrame.FillOutstanding(packet));

        std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
        Neuron::ByteWriter commandWriter{outgoing};
        const bool sent = Outpost::Encode(packet, commandWriter) &&
                          transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});
        Report(log, "ATTACK " + std::to_string(picked.target) + " with " + std::to_string(attack.selection.size()) + " selected" +
                      (sent ? "" : " NOT SENT"));
        continue;
      }

      if (picked.verb != Outpost::OrderVerb::MoveTo)
      {
        // The build panel and anything else this tap table grows. The verb is resolved and named rather than
        // silently dropped, so the log says which row of `Interface.md` section 4's table a tap landed on.
        Report(log, "TAP resolved to verb " + std::to_string(static_cast<int>(picked.verb)) + ", which M1.10 does not act on yet");
        continue;
      }

      const Outpost::SelectionOutcome& outcome = picked;

      if (!clientFrame.CurrentJoin().IsJoined())
      {
        // Before ADR-013 this could not happen, because the client assumed it was player one. Now
        // the host refuses a command from an endpoint it has not seated, so sending one would be
        // a marker on screen for an order nobody applied.
        Report(log, "TAP ignored -- not seated yet");
        continue;
      }

      const std::uint16_t sequence = clientFrame.TakeCommandSequence();
      std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
      Neuron::ByteWriter commandWriter{outgoing};
      Outpost::CommandPacket packet{.sequence = sequence, .player = clientFrame.Player(), .commands = {}};
      clientFrame.StampView(packet);
      const Outpost::Command moveCommand = Outpost::BuildMoveCommand(sequence, outcome.worldX, outcome.worldY, selection.Identities());
      clientFrame.IssueCommand(moveCommand, nowMs);
      static_cast<void>(clientFrame.FillOutstanding(packet));

      bool sent = false;
      if (Outpost::Encode(packet, commandWriter))
      {
        sent = transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});
      }

      Outpost::OrderMarker marker;
      marker.targetX = moveCommand.targetX;
      marker.targetY = moveCommand.targetY;
      marker.commandSequence = sequence;
      // The whole selection, because the marker's job is to say what was ordered (Q20) and M1.10
      // made that more than one ship.
      marker.selection.assign(selection.Identities().begin(), selection.Identities().end());
      clientFrame.Markers().Add(marker);

      // === M0.23'S MEASUREMENT ARMS ONLY ON A PARKED SHIP. ====================================
      //
      // It used to arm on every tap and fire on the first frame where the drawn position differed
      // at all -- which, if the ship was ALREADY travelling from an earlier order, is the next
      // frame. Eight of twelve taps in the first run on the device read zero milliseconds that
      // way: the measurement was timing the old motion continuing rather than the new order
      // arriving, and it produced a plausible-looking number for it.
      //
      // A latency test taps a ship that is standing still. When it is not, the tap is recorded as
      // unmeasurable rather than measured badly -- a missing sample costs nothing and a wrong one
      // is worse than none, because it goes into an average.
      //
      // **SINCE M1.17 "STANDING STILL" ALSO MEANS NOT TURNING**, because a ship can now turn in place.
      if (!tapProbe.Arm(nowMs))
      {
        Report(log, "TAPUNMEASURED the ship was already " +
                      std::string{tapProbe.TurnedLastFrame() ? "turning"
                                                             : "moving, " + std::to_string(tapProbe.MovedLastFrameUnits()) + " units"} +
                      " last frame");
      }

      Report(log, "TAP seq=" + std::to_string(sequence) + " authored=" + std::to_string(event.xAuthoredPixels) + "," +
                    std::to_string(event.yAuthoredPixels) + " world=" + std::to_string(outcome.worldX) + "," +
                    std::to_string(outcome.worldY) + (sent ? " sent" : " NOT SENT") +
                    " markers=" + std::to_string(clientFrame.Markers().Count()) + " local_ms=" + std::to_string(nowMs));
    }

    // === THE MESHES, READ ONCE BEFORE THE FRAME LOOP EVER DRAWS ONE. ========================
    //
    // ADR-021 names the failure this has to survive and it is not a read error: **the package not
    // carrying the file on a clean install**, which does not appear on the machine that built it.
    // So a mesh that does not load is reported BY NAME and the client keeps running -- M0.21b's
    // generated arrow is what a design with no mesh gets, which is a visible, diagnosable outcome
    // rather than a window that never appears.
    //
    // It is blocking I/O and it is done once, here. Four hundred kilobytes at launch is not a
    // frame's worth of work to hide; the moment something has to load DURING a match this needs a
    // worker and a handoff (`NeuronClient/PackageFile.h`).
    if (!meshesLoaded)
    {
      meshesLoaded = true;
      for (std::size_t slot = 0; slot < meshBuffers.size(); ++slot)
      {
        const std::string_view name = Outpost::ShippedMeshes()[slot];
        Outpost::HullMesh hull;
        if (!ReadShippedMesh(name, log, hull))
        {
          continue;
        }

        const std::span<const std::byte> vertexBytes{reinterpret_cast<const std::byte*>(hull.vertices.data()),
                                                     hull.vertices.size() * sizeof(Outpost::HullVertex)};
        meshReady[slot] = meshBuffers[slot].Create(device, vertexBytes, sizeof(Outpost::HullVertex), hull.indices);
        Report(log, "MESH " + std::string{name} + (meshReady[slot] ? " uploaded " : " FAILED to upload ") +
                      std::to_string(hull.vertices.size()) + " vertices, " + std::to_string(hull.indices.size() / 3) + " triangles");
      }

      // M2.4: THE FIVE ASTEROID VARIANTS, READ AND KEPT ON THE PROCESSOR. Nothing is uploaded until a
      // join says which field to draw, because what goes to the device is each variant's rocks already
      // placed (`GameClient/AsteroidMesh.h`), not the variant alone.
      for (std::size_t variant = 0; variant < asteroidVariants.size(); ++variant)
      {
        asteroidLoaded[variant] = ReadShippedMesh(Outpost::AsteroidVariantNames()[variant], log, asteroidVariants[variant]);
      }
    }

    // === M2.4: THE FIELD, BAKED ONCE A MATCH AND BEFORE THE FRAME RECORDS. ==================
    //
    // The field never moves, so each variant's rocks are placed on the processor into one static
    // mesh and drawn with one identity instance -- five draws for every rock on the map. It is redone
    // only when the join names a different pair, and then after `WaitForGpu`: the old buffers may
    // still be read by a frame in flight, and a destroyed buffer under a recorded draw is a removed
    // device rather than a wrong picture.
    const Outpost::FieldView& field = clientFrame.Field();
    if (meshesLoaded && field.IsDerived() && ((field.MatchSeed() != bakedFieldSeed) || (field.PlayerCount() != bakedFieldPlayers)))
    {
      device.WaitForGpu();
      bakedFieldSeed = field.MatchSeed();
      bakedFieldPlayers = field.PlayerCount();

      // The exact sphere of each loaded variant, and the catalog's looser one for a variant that did not
      // load -- which draws nothing, but still has to be kept clear of by its neighbors' clamp.
      std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii{};
      for (std::size_t variant = 0; variant < radii.size(); ++variant)
      {
        const Outpost::MeshEntry* entry = Outpost::FindMesh(Outpost::AsteroidVariantNames()[variant]);
        radii[variant] = asteroidLoaded[variant] ? Outpost::BoundingRadiusUnits(asteroidVariants[variant])
                         : (entry != nullptr)    ? Outpost::BoundingRadiusUnits(*entry)
                                                 : 0.0f;
      }
      const std::vector<Outpost::RockLook> looks = Outpost::RockLooks(field.MatchSeed(), field.Rocks(), radii);
      rockPicks = Outpost::RockPickPoints(field.Rocks(), looks);

      std::size_t rocksDrawn = 0;
      for (std::size_t variant = 0; variant < fieldBuffers.size(); ++variant)
      {
        fieldBuffers[variant].Destroy();
        fieldReady[variant] = false;

        Outpost::HullMesh placed;
        if (!asteroidLoaded[variant] ||
            !Outpost::BuildVariantField(asteroidVariants[variant], static_cast<std::uint8_t>(variant), field.Rocks(), looks, placed) ||
            placed.vertices.empty())
        {
          continue;
        }

        const std::span<const std::byte> vertexBytes{reinterpret_cast<const std::byte*>(placed.vertices.data()),
                                                     placed.vertices.size() * sizeof(Outpost::HullVertex)};
        fieldReady[variant] = fieldBuffers[variant].Create(device, vertexBytes, sizeof(Outpost::HullVertex), placed.indices);
        if (fieldReady[variant])
        {
          rocksDrawn += placed.vertices.size() / asteroidVariants[variant].vertices.size();
        }
      }
      Report(log, "FIELD " + std::to_string(rocksDrawn) + " of " + std::to_string(field.Rocks().size()) + " rocks baked for seed " +
                    std::to_string(bakedFieldSeed) + " at " + std::to_string(bakedFieldPlayers) + " players");
    }

    // M0.15's frame and M0.17's, and R13's arrangement in six lines: the world clears the SCENE
    // TARGET, the back buffer is cleared to black -- which is what the letterbox bars are -- the
    // present step fits one into the other, and the interface pass draws over that, straight into
    // the back buffer at physical resolution (ADR-011).
    //
    // **BOTH CLEARS ARE BLACK.** The scene target used to clear to a dark blue so that a present
    // blit which drew nothing would show as a black screen -- instrumentation from before there was
    // a world to draw. The fleet and the stars now say the same thing, and a blue clear is exactly
    // what ADR-019 does not want: the scene target's clear IS the sky between the stars.
    if (swapChain.IsReady() && presentStep.IsReady() && interfacePass.IsReady() && device.BeginFrame())
    {
      // === M1.12: THE GLYPH ATLAS, ONCE, ON THE FIRST FRAME. ====================================
      //
      // Here rather than with the other passes because its upload is a copy recorded on the frame's
      // command list. **A MISSING SEGOE UI IS A FAILURE AND NOT A SUBSTITUTION** (ADR-009): the client
      // keeps running with no interface, and says why, rather than moving every string.
      if (!atlasAttempted)
      {
        atlasAttempted = true;
        const auto rasterizing = std::chrono::steady_clock::now();
        if (glyphAtlas.Create(device, interfaceFit))
        {
          // ADR-009's second owed measurement: what the atlas costs at startup.
          Report(log, "ATLAS rasterized in " + std::to_string(MillisecondsSince(rasterizing)) + " ms, body " +
                        std::to_string(glyphAtlas.EmSizePixels(Neuron::TextSize::Body)) + " px, display " +
                        std::to_string(glyphAtlas.EmSizePixels(Neuron::TextSize::Display)) + " px, " +
                        std::to_string(glyphAtlas.UsedHeightPixels()) + " of " + std::to_string(glyphAtlas.HeightPixels()) + " rows used");
        }
        else
        {
          Report(log, "ATLAS FAILED, hresult " + std::to_string(glyphAtlas.LastHresult()) + " -- the interface is not drawn");
        }
      }

      static_cast<void>(sceneTarget.RecordClear(device));

      // M0.21b: THE WORLD, AND THE FIRST THING IN THIS TREE TO DRAW ONE. Into the scene target,
      // which `RecordClear` has just bound, before the present step fits it into the back buffer.
      //
      // The clock hands back every held entity, each interpolated between its own two samples or held
      // at its newest (`TechnicalDesign.md` section 6, ADR-024). Nothing here simulates: R19's line is
      // that the host said where these are and the client only says where they are BETWEEN the two
      // things the host said.
      if (worldPass.IsReady())
      {
        const Outpost::DrawnSummary drawn = clientFrame.Advance(nowMs, drawnRecords);
        // THE FIELD DRAWS WITH NO SHIPS IN VIEW, so an empty store no longer skips the pass -- it skips
        // only the loop over entities, which has nothing to do.
        const bool fieldBaked = std::any_of(fieldReady.begin(), fieldReady.end(), [](bool _ready) { return _ready; });
        if (!drawnRecords.empty() || fieldBaked)
        {
          const float aspect = (sceneTarget.HeightPixels() > 0)
                                 ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                 : 1.0f;
          const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(clientFrame.Camera(), aspect);

          // One list per shipped mesh. Cleared and refilled every frame rather than kept: the store
          // already carries what is worth carrying across frames, and this is only the drawing of it.
          std::array<std::vector<Neuron::MeshInstance>, Outpost::SHIPPED_MESH_COUNT> instances;

          // THE ENTITY M0.23'S INSTRUMENT WATCHES: the first selected, which is the first ship a tap on
          // the ground orders. The first drawn record was right while M0 had one ship; in M1 it is a
          // station, which never moves, so no tap could ever be timed against it.
          const Outpost::WireIdentity watched =
            selection.Identities().empty() ? drawnRecords.front().identity : selection.Identities().front();

          for (const Outpost::EntityRecord& shown : drawnRecords)
          {
            const float entityX = static_cast<float>(Outpost::DequantizePosition(shown.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
            const float entityY = static_cast<float>(Outpost::DequantizePosition(shown.positionY)) / static_cast<float>(Neuron::FIXED_ONE);
            const Neuron::Angle entityHeading = Outpost::DequantizeWireHeading(shown.heading);
            const auto entityDesign = static_cast<Outpost::DesignId>(shown.designIdentity);
            const Outpost::PlayerId entityOwner = shown.owner;

            // **BUCKETED BY MESH, NOT DRAWN HERE.** One instanced call per shape is the whole point
            // of the arrangement; drawing inside this loop would be one call per entity, which is
            // what M0.21b did with a generated arrow and what M1.9 exists to stop.
            const std::string_view meshName = Outpost::MeshNameForDesign(entityDesign);
            std::size_t bucket = meshBuffers.size();
            for (std::size_t slot = 0; slot < meshBuffers.size(); ++slot)
            {
              if (Outpost::ShippedMeshes()[slot] == meshName)
              {
                bucket = slot;
                break;
              }
            }

            if ((bucket < meshBuffers.size()) && meshReady[bucket])
            {
              instances[bucket].push_back(Outpost::InstanceFor(entityX, entityY, entityHeading, entityOwner));
            }
            else
            {
              // **NO MESH, SO M0.21b's ARROW.** The `Cruiser` today, anything M4 adds before its
              // geometry does, and -- the case that matters -- every hull on an install where the
              // package did not carry the files. A visible, diagnosable outcome rather than a gap.
              const Outpost::Matrix4 world = Outpost::EntityTransform(entityX, entityY, entityHeading);

              // world * viewProjection, row-vector convention, multiplied out here because this is
              // the only place in the tree that composes two of these.
              Outpost::Matrix4 combined;
              for (int row = 0; row < 4; ++row)
              {
                for (int column = 0; column < 4; ++column)
                {
                  float sum = 0.0f;
                  for (int k = 0; k < 4; ++k)
                  {
                    sum += world.m[(row * 4) + k] * viewProjection.m[(k * 4) + column];
                  }
                  combined.m[(row * 4) + column] = sum;
                }
              }

              static_cast<void>(worldPass.Record(device, sceneTarget, combined.m, 0.80f, 0.86f, 0.95f, 1.0f));
            }

            if (shown.identity == watched)
            {
              const Outpost::DrawnPose pose{
                .xUnits = static_cast<float>(Outpost::DequantizePosition(shown.positionX)) / static_cast<float>(Neuron::FIXED_ONE),
                .yUnits = static_cast<float>(Outpost::DequantizePosition(shown.positionY)) / static_cast<float>(Neuron::FIXED_ONE),
                .wireHeading = shown.heading};
              if (const std::optional<Outpost::TapVisible> seen = tapProbe.Observe(pose, nowMs); seen.has_value())
              {
                Report(log, "TAPVISIBLE ms=" + std::to_string(seen->milliseconds) +
                              " by=" + ((seen->change == Outpost::VisibleChange::Heading) ? "heading" : "position") +
                              " from=" + std::to_string(tapProbe.From().xUnits) + "," + std::to_string(tapProbe.From().yUnits) + "," +
                              std::to_string(tapProbe.From().wireHeading) + " to=" + std::to_string(pose.xUnits) + "," +
                              std::to_string(pose.yUnits) + "," + std::to_string(pose.wireHeading) + " " + DrawnText(drawn));
              }
            }
          }

          // === M3.4: THE WRECKS, IN THEIR HULL'S BUCKET. =======================================
          //
          // Drawn with the living, one call per shape, dark. A wreck whose hull has no mesh on this install is
          // not drawn at all rather than as an arrow: the arrow is a diagnostic for the living.
          for (const Outpost::Wreck& wreck : clientFrame.Wrecks().Wrecks())
          {
            if (wreck.last.designIdentity >= Outpost::Designs().size())
            {
              continue;
            }
            const std::string_view meshName = Outpost::MeshNameForDesign(static_cast<Outpost::DesignId>(wreck.last.designIdentity));
            for (std::size_t slot = 0; slot < meshBuffers.size(); ++slot)
            {
              if ((Outpost::ShippedMeshes()[slot] == meshName) && meshReady[slot])
              {
                instances[slot].push_back(Outpost::WreckInstance(wreck, nowMs));
                break;
              }
            }
          }

          // === ONE INSTANCED CALL PER SHAPE. =================================================
          //
          // Team colour is per instance, which is what lets a single call cover every ship of a
          // shape regardless of owner. A mesh that wanted a second material for its team would cost
          // a draw call per owner, and that is a trade to argue rather than take.
          //
          // **THE RING IS INDEXED BY THE FRAME IN FLIGHT.** A single instance buffer would be
          // written while the graphics processor was still reading the last frame's copy -- which
          // does not crash, it draws one frame of ships at another frame's positions, intermittently.
          if (meshPass.IsReady() && instanceRing.IsReady() && meshPass.Begin(device, sceneTarget, viewProjection.m, Outpost::ShipLook()))
          {
            // **ONE UPLOAD, THREE RANGES.** The ring writes from the start of the frame's buffer,
            // so the three shapes are concatenated first and each draw points at its own offset --
            // three separate writes would each start at zero and the last would win.
            std::vector<Neuron::MeshInstance> packed;
            std::array<std::uint32_t, Outpost::SHIPPED_MESH_COUNT> firstInstance{};
            std::array<std::uint32_t, Outpost::SHIPPED_MESH_COUNT> instanceCount{};
            for (std::size_t slot = 0; slot < meshBuffers.size(); ++slot)
            {
              firstInstance[slot] = static_cast<std::uint32_t>(packed.size());
              if (meshReady[slot])
              {
                packed.insert(packed.end(), instances[slot].begin(), instances[slot].end());
              }
              instanceCount[slot] = static_cast<std::uint32_t>(packed.size()) - firstInstance[slot];
            }

            // M2.4's identity instance, last, in the same write -- a second write would start at zero and
            // overwrite the ships'. White, and unread: every rock vertex takes the hull palette.
            const std::uint32_t fieldInstance = static_cast<std::uint32_t>(packed.size());
            packed.push_back(Neuron::MeshInstance{});

            const std::uint32_t accepted = instanceRing.Write(instanceFrame, packed);
            for (std::size_t slot = 0; slot < meshBuffers.size(); ++slot)
            {
              // A capacity smaller than the snapshot truncates rather than writing past the end, so
              // a shape whose range fell off the end is skipped rather than drawn from rubbish.
              if ((instanceCount[slot] == 0) || ((firstInstance[slot] + instanceCount[slot]) > accepted))
              {
                continue;
              }

              const std::uint64_t address = instanceRing.BufferAddress(instanceFrame) +
                                            (static_cast<std::uint64_t>(firstInstance[slot]) * sizeof(Neuron::MeshInstance));
              static_cast<void>(meshPass.Draw(device, meshBuffers[slot], address,
                                              instanceCount[slot] * static_cast<std::uint32_t>(sizeof(Neuron::MeshInstance)),
                                              instanceCount[slot]));
            }

            // === M2.4: ONE DRAW PER ASTEROID VARIANT, FIVE FOR THE WHOLE FIELD. ============
            if (fieldInstance < accepted)
            {
              const std::uint64_t address =
                instanceRing.BufferAddress(instanceFrame) + (static_cast<std::uint64_t>(fieldInstance) * sizeof(Neuron::MeshInstance));
              for (std::size_t variant = 0; variant < fieldBuffers.size(); ++variant)
              {
                if (fieldReady[variant])
                {
                  static_cast<void>(
                    meshPass.Draw(device, fieldBuffers[variant], address, static_cast<std::uint32_t>(sizeof(Neuron::MeshInstance)), 1));
                }
              }
            }

            instanceFrame = (instanceFrame + 1) % Neuron::InstanceRing::FRAME_COUNT;
          }

          // === Q81: THE BEAMS, AFTER THE HULLS SO A HULL IN FRONT HIDES ONE. ========================
          clientFrame.Tracers().Expire(nowMs);
          if (beamPass.IsReady())
          {
            const float unitsPerPixel = Outpost::UnitsPerPixelAtFocus(clientFrame.Camera(), sceneTarget.HeightPixels());
            Outpost::BuildBeams(clientFrame.Tracers(), drawnRecords, rockPicks, nowMs, unitsPerPixel, beams);
            Outpost::AppendBlasts(clientFrame.Wrecks(), nowMs, unitsPerPixel, beams);
            if (beamPass.Draw(device, sceneTarget, beamFrame, viewProjection.m, beams))
            {
              beamFrame = (beamFrame + 1) % Neuron::BeamPass::FRAME_COUNT;
            }
          }
        }
      }

      // **M0.16's CALIBRATION PATTERN IS GONE, AND THAT GATE IS CLOSED.** It drew a one-pixel border
      // and a one-pixel cross through the middle of the scene target, so that the present step's
      // resampling had a hard edge to be judged on: at 1:1 one physical pixel and fully white, at a
      // 0.5 scale two pixels and still fully white, with gray the failure.
      //
      // The filter was confirmed by eye on the device and the scale that ships is 1:1
      // (`ADR-016`), so what is left is a white cross drawn over the world every frame. A closed
      // gate's instrumentation is debris, and this is the first milestone with something behind it
      // worth seeing.

      // === THE SKY, DRAWN LAST (ADR-019). ================================================
      //
      // **LAST IS THE WHOLE OPTIMIZATION.** The pass tests depth and does not write it, so every
      // pixel the fleet already covers is rejected before it shades -- the sprites' falloff is paid
      // only where there is sky to see. Drawn first it would shade and then be painted over.
      //
      // **THERE IS NO BACKDROP BEHIND THE STARS.** The baked galaxy band that used to sit here read
      // on the device as a painted wash rather than as sky, and ADR-019 withdrew it: the Milky Way is
      // carried by the stars crowding toward the plane, and between them the scene target's clear is
      // black.
      //
      // It is outside the `worldPass.IsReady()` block above on purpose: the sky is the backdrop and
      // does not depend on there being a snapshot to draw in front of it.
      if (pointSprites.StarCount() > 0)
      {
        const float skyAspect = (sceneTarget.HeightPixels() > 0)
                                  ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                  : 1.0f;

        // **THE TRANSLATION IS REMOVED BY ZEROING THE LAST ROW.** These matrices are row-major and
        // applied to row vectors, so the last row IS the image of the origin -- the camera's
        // position and nothing else. Zeroing it leaves the rotation and the projection, which is
        // exactly the sky's transform: it turns with heading and pitch and does NOT slide with the
        // pan (ADR-018, ADR-019). A sky that translated would read as a painted backdrop sliding
        // behind the fleet, which is the single clearest tell that a sky is not a sky.
        const Outpost::Matrix4 full = Outpost::ViewProjection(clientFrame.Camera(), skyAspect);
        float rotationProjection[16];
        for (std::size_t index = 0; index < 16; ++index)
        {
          rotationProjection[index] = (index >= 12) ? 0.0f : full.m[index];
        }

        static_cast<void>(pointSprites.Draw(device, sceneTarget, rotationProjection));
      }
      device.MarkWorldDrawn();

      static_cast<void>(swapChain.RecordBindAndClear(device, 0.0f, 0.0f, 0.0f));
      static_cast<void>(presentStep.Record(device, sceneTarget, worldFit));
      device.MarkPresentScaled();

      // **M0.17'S PROBE RECTANGLE IS GONE, AND THAT GATE IS CLOSED TOO.** It drew the 48 x 48 touch
      // floor inset by its 16 pixels of clear space, in the bottom left, so that a pair of eyes
      // could check the authored-to-physical transform lands where `Interface.md` section 1 says --
      // 96 physical pixels and 9.15 mm on the target panel. It did, and the figures are in the log
      // at startup rather than on the glass.
      //
      // **THE PASS STILL RUNS AND STILL DRAWS NOTHING**, which is deliberate: ADR-011's ordering is
      // the thing that would break silently, and a pass that is present and empty keeps the
      // constraint under test until M1.14 gives it panels to draw.
      //
      // ADR-011'S ORDERING CONSTRAINT lives here because it is the caller's: after the present
      // blit, before the back buffer is closed. Moving this line above the blit draws the interface
      // and then paints the world over it, which is a defect nothing in either class can catch.
      static_cast<void>(interfacePass.Record(device, swapChain, interfaceFit, {}));

      // === M1.14: THE FOUR PANELS, AS ONE INSTANCED DRAW. =========================================
      //
      // Everything the panels need is read here and nowhere else: the newest update's block for THIS
      // player (one-based identity, zero-based block -- `ClientFrame.cpp` says why that matters), the
      // selection, and the client-local facts. `BuildHud` is pure and has a suite over it; this is the
      // gathering and the call (R20).
      {
        Outpost::HudState hudState;
        hudState.leftHanded = LEFT_HANDED;
        hudState.player = clientFrame.Player();
        hudState.buildPanelOpen = buildPanelOpen;

        // M2.11: the armed button and the radius around the station, projected through the camera the world was
        // drawn with. The ring is world space and the panels are not, so it is recomputed every frame.
        // M2.11b: what this player has built decides which module buttons are available -- and an armed one that
        // stopped being available (the fourth module landed, say) is disarmed rather than left pointing at nothing.
        for (const Outpost::PlacedModule& owned : Outpost::OwnModules(clientFrame.Replicas().Entities(), clientFrame.Player()))
        {
          hudState.ownModules.push_back(owned.design);
        }
        if (!buildPanelOpen || (moduleArming.IsArmed() && !Outpost::ModuleAvailable(moduleArming.Armed(), hudState.ownModules)))
        {
          moduleArming.Disarm();
        }
        hudState.moduleArmed = moduleArming.IsArmed();
        hudState.armedModule = moduleArming.Armed();
        if (Outpost::EntityRecord station{};
            moduleArming.IsArmed() && Outpost::OwnStation(clientFrame.Replicas().Entities(), clientFrame.Player(), station))
        {
          const float ringAspect = (sceneTarget.HeightPixels() > 0)
                                     ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                     : 1.0f;
          hudState.placementRing = Outpost::PlacementRingSquares(
            clientFrame.Camera(), ringAspect, static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH),
            static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT),
            Neuron::Vec2{.x = Outpost::DequantizePosition(station.positionX), .y = Outpost::DequantizePosition(station.positionY)});
        }
        hudState.quitArmed = quitConfirm.IsArmed(nowMs);

        hudState.link = clientFrame.Link();
        hudState.fieldMismatch = clientFrame.CurrentJoin().FieldMismatch();

        // M3.8: THE RESULT, for as long as the frame says it is shown. The next match is already running under it.
        const Outpost::MatchEnded* result = clientFrame.ShownResult(nowMs);
        hudState.matchEnded = (result != nullptr);
        hudState.winner = (result != nullptr) ? result->winner : Outpost::NO_PLAYER;
        hudState.endedOnClock = (result != nullptr) && result->onClock;

        // **THIS CLIENT'S OWN BLOCK**, which is the only one an update carries (ADR-024).
        if (const Outpost::PlayerBlock* own = clientFrame.Replicas().Own(); own != nullptr)
        {
          hudState.credits = own->credits;
          hudState.buildingWire = own->buildingDesign;
          hudState.buildProgressPercent = own->buildProgressPercent;
          creditFlash.Observe(own->credits, nowMs);
        }

        // A LINK THAT IS NOT UP FORGETS THE BALANCE, so the first update after a rejoin records it rather than
        // flashing whatever it moved by while this client was away.
        if (hudState.link != Outpost::LinkState::Linked)
        {
          creditFlash.Reset();
        }
        hudState.creditFlash = creditFlash.Showing(nowMs);
        hudState.creditFlashAlpha = creditFlash.Alpha(nowMs);

        // **LIVE, EVERY FRAME, AND NOT ANIMATED** -- the count is what the player reads while their
        // hand covers the double tap's circle.
        hudState.groups = Outpost::SummarizeSelection(selection.Identities(), clientFrame.Replicas().Entities());

        // M3.3b: THE HULL BARS AND THE ALERTS, placed last, because an alert keeps clear of every panel above.
        const float hudAspect = (sceneTarget.HeightPixels() > 0)
                                  ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                  : 1.0f;
        hudState.hullBars = Outpost::PlaceHullBars(drawnRecords, clientFrame.Camera(), hudAspect);
        hudState.alerts = Outpost::PlaceAlerts(clientFrame.Alerts(), clientFrame.Camera(), hudAspect, Outpost::PanelBands(hudState), nowMs);
        shownAlerts = hudState.alerts;

        hud = Outpost::BuildHud(hudState);
        hudQuads.clear();
        if (glyphAtlas.IsReady() && textRenderer.IsReady())
        {
          Outpost::EmitQuads(hud, Outpost::TypefaceOf(glyphAtlas), interfaceFit, hudQuads);
          static_cast<void>(textRenderer.Record(device, swapChain, glyphAtlas, hudQuads));
        }
      }

      static_cast<void>(swapChain.RecordReadyToPresent(device));
      if (device.EndFrameAndSubmit() && swapChain.Present())
      {
        device.PresentedFrame();
        ++presentedFrames;

        // AFTER PresentedFrame, which is the wait that makes the slot's timestamps readable, and
        // only once the warm-up frames are behind us.
        // **THE SAMPLE IS EXACTLY THE WINDOW.** It used to run until the client exited, which was
        // the same thing while the client exited here; now that it keeps running, a long look would
        // otherwise be folded into a figure that is supposed to be sixty seconds of steady state.
        if ((presentedFrames > WARMUP_FRAMES) && (presentedFrames <= (WARMUP_FRAMES + MEASURE_FRAMES)))
        {
          gpuTime.Add(device.LastFrameGpuMicroseconds());

          // A frame with no honest split adds nothing to any of the three, so their counts say how
          // many frames the comparison is over.
          if (Neuron::GpuFrameSplit split{}; device.LastFrameGpuSplit(split))
          {
            worldGpuTime.Add(split.worldMicroseconds);
            presentGpuTime.Add(split.presentMicroseconds);
            interfaceGpuTime.Add(split.interfaceMicroseconds);
          }
        }
        if (presentedFrames == (WARMUP_FRAMES + MEASURE_FRAMES))
        {
          // ONCE, ON THE FRAME THE WINDOW CLOSES, and the loop carries on. `==` rather than `>=` is
          // what makes it once, and reporting here rather than at teardown is what keeps the gate's
          // property: the figures are written whether or not anybody closes the window.
          Report(log, "probe: measurement window complete, gpu us mean " + std::to_string(gpuTime.MeanMicroseconds()) + " min " +
                        std::to_string(gpuTime.MinimumMicroseconds()) + " max " + std::to_string(gpuTime.MaximumMicroseconds()) + " over " +
                        std::to_string(gpuTime.Count()) + " frames, " + std::to_string(gpuTime.DiscardedCount()) +
                        " discarded -- the client keeps running so the hulls can be looked at");
          Report(log,
                 "probe: gpu split us mean world " + std::to_string(worldGpuTime.MeanMicroseconds()) + " (max " +
                   std::to_string(worldGpuTime.MaximumMicroseconds()) + ") present " + std::to_string(presentGpuTime.MeanMicroseconds()) +
                   " (max " + std::to_string(presentGpuTime.MaximumMicroseconds()) + ") interface " +
                   std::to_string(interfaceGpuTime.MeanMicroseconds()) + " (max " + std::to_string(interfaceGpuTime.MaximumMicroseconds()) +
                   ") over " + std::to_string(worldGpuTime.Count()) + " split frames");
        }
        if (presentedFrames == 1)
        {
          // Once, on the first one. A frame counter every frame would bury the log; the question
          // this answers is only ever "did it present at all".
          Report(log, "probe: first frame presented, back buffer " + std::to_string(swapChain.CurrentBackBufferIndex()) + " of " +
                        std::to_string(Neuron::SwapChain::BUFFER_COUNT));
        }
      }
    }

    // NOT sleep_for. Windows rounds a short sleep up to the system timer granularity, which is
    // 15.6 ms by default, and a probe that polls that coarsely quantizes every arrival into a
    // 15 ms bucket -- reporting as jitter what is actually its own scheduler. A measurement whose
    // whole purpose is to find the real figure cannot have a noise floor three times the tick, so
    // this spins instead and spends a core for the length of a run. Deliberate, and temporary.
    std::this_thread::yield();
  }

  // The queue's own counters, so that a gap in the log can be told apart from the network. A drop
  // here is this client failing to keep up; a gap with none of these is the link losing packets.
  // M0.16 / TechnicalDesign.md section 9.5. GPU time, not the frame interval: Present(1, 0) waits
  // for a vertical blank, so the interval is the refresh period whatever the renderer costs.
  Report(log, "probe: gpu us mean " + std::to_string(gpuTime.MeanMicroseconds()) + " min " + std::to_string(gpuTime.MinimumMicroseconds()) +
                " max " + std::to_string(gpuTime.MaximumMicroseconds()) + " over " + std::to_string(gpuTime.Count()) + " frames, " +
                std::to_string(gpuTime.DiscardedCount()) + " discarded");

  Report(log, "probe: received " + std::to_string(receivedCount) + " dropped " + std::to_string(queue.DroppedCount()) + " rejected " +
                std::to_string(queue.RejectedCount()) + " oversized " + std::to_string(transport.OversizedCount()) + " skippedSends " +
                std::to_string(transport.SkippedSendCount()) + " presented " + std::to_string(presentedFrames));
  transport.Close();

  // THE WAIT COMES FIRST, and this is the one ordering in the teardown that matters. Direct3D 12
  // does not track whether the GPU is still reading a resource that is being released, so dropping
  // the scene target with the last frame in flight is a use-after-free with a driver in the middle
  // of it. The device's own Destroy waits too -- for the frames submitted after everything below
  // has already gone.
  device.WaitForGpu();

  beamPass.Destroy();
  textRenderer.Destroy();
  glyphAtlas.Destroy();
  interfacePass.Destroy();
  presentStep.Destroy();
  sceneTarget.Destroy();
  swapChain.Destroy();
  device.Destroy();

  // **THE SECOND TAP OF THE QUIT, AND ONLY THAT** (`Interface.md` section 6). After the teardown, so the
  // graphics processor is idle and the log is complete before the process goes.
  if (quitConfirmed)
  {
    CoreApplication::Exit();
  }
}

// ---------------------------------------------------------------------------------------------
// End of M0.5 scaffolding.
// ---------------------------------------------------------------------------------------------

struct App : winrt::implements<App, IFrameworkViewSource, IFrameworkView>
{
  IFrameworkView CreateView()
  {
    return *this;
  }

  void Initialize(const CoreApplicationView&)
  {
    // NOTHING HERE ANY MORE, AND THAT IS THE POINT. This named the four libraries it was linked
    // against, which was M0.5's evidence that the whole chain reached the package rather than
    // merely compiled. That gate is closed and the placeholder functions it read are gone with the
    // rest of them (`Leaving M0`). What proves the chain now is that the client draws.
  }

  void Load(const winrt::hstring&) {}

  void SetWindow(const CoreWindow&)
  {
    // M0.16 MEASURES THE PANEL, and a windowed client measures something else: the swap chain is
    // created from the window's size exactly once, so at the default 1024 x 768 device-independent
    // pixels the gate takes its figures at 2048 x 1536 and writes them down as the Surface Pro's.
    //
    // IT IS HERE AND NOT IN `Run` BECAUSE IT HAS TO PRECEDE ACTIVATION. This is a preference the
    // platform consults when the view comes up; `Run` has already activated by the time it could
    // ask, and asking afterwards -- `TryEnterFullScreen` -- is refused on this shell every time.
    // M0.22 owns fullscreen-at-launch as shipped behavior; this is it, made early.
    Outpost::PreferFullScreenLaunch();
  }

  void Run()
  {
    const CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();
    RunProbe(window);
  }

  void Uninitialize() {}
};
} // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
  CoreApplication::Run(winrt::make<App>());
  return 0;
}

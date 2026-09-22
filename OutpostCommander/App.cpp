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

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>

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
/// **The probe exits when this completes**, which it did not used to -- it ran until the window was
/// closed. A gate run has to terminate by itself or it writes no figures at all.
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

/// Log formatting, like `FilterName` above. **`Starved` in a steady run is the one to notice**: it
/// means no retained pair straddled the render time, so the frame fell back to the newest snapshot
/// and was NOT drawn 75 milliseconds behind -- which is ADR-003's buffer not doing its job, and it
/// would make every latency figure optimistic.
[[nodiscard]] const char* PlayoutName(Outpost::PlayoutState _state) noexcept
{
  switch (_state)
  {
  case Outpost::PlayoutState::Interpolating:
    return "interp";
  case Outpost::PlayoutState::Extrapolating:
    return "extrap";
  case Outpost::PlayoutState::Holding:
    return "hold";
  case Outpost::PlayoutState::Starved:
    return "STARVED";
  }
  return "?";
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
  std::ofstream log{localState / L"probe-log.txt", std::ios::trunc};
  Trace(log ? "probe: log open" : "probe: log NOT open");

  const std::string host = Neuron::ReadHostAddress();
  Report(log, "probe: LocalState is " + localState.string());
  Report(log, "probe: host " + host + " port " + std::to_string(Neuron::ProbePacket::PORT));

  Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
  Neuron::DatagramTransport transport{queue};
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
  std::uint16_t lastTapIdentity = 0;

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
                                        .heightPixels = Neuron::WorldTargetHeightPixels(swapChain.HeightPixels()),
                                        .clearRed = 0.02f,
                                        .clearGreen = 0.04f,
                                        .clearBlue = 0.09f}))
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
  else if (!worldPass.Create(device, sceneTarget))
  {
    // M0.21b. It is created from the SCENE TARGET rather than the swap chain, because that is what
    // it renders into and a pipeline state has to agree with its target's formats and sample count.
    Report(log, "probe: no world pass, hresult " + std::to_string(worldPass.LastHresult()));
  }
  else
  {
    worldFit = Neuron::ComputeFit(sceneTarget.WidthPixels(), sceneTarget.HeightPixels(), swapChain.WidthPixels(), swapChain.HeightPixels());
    interfaceFit = Neuron::ComputeInterfaceFit(swapChain.WidthPixels(), swapChain.HeightPixels());

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

  const CoreDispatcher dispatcher = _window.Dispatcher();
  const auto start = std::chrono::steady_clock::now();
  // The token from `LocalState`, or zero on a first run. It is read once: the file is only
  // written again when the host issues a different one.
  clientFrame.MutableJoin().Begin(Neuron::ReadSessionToken());
  bool reportedReady = false;
  bool reportedSeat = false;
  std::uint64_t receivedCount = 0;

  // M0.23'S MEASUREMENT, AND IT IS AN OBSERVATION RATHER THAN AN ARITHMETIC (R20). The client
  // records when the tap resolved, where the entity was drawn at that instant, and the first frame
  // in which the drawn position differs from it. `Scripts/ProbeReport.py` turns a run of those into
  // a distribution; nothing here averages anything.
  //
  // WHY IT IS THE DRAWN POSITION AND NOT THE SNAPSHOT'S. The gate's question is what the PLAYER
  // sees, and the client renders 75 ms behind the newest snapshot -- so a snapshot that already
  // carries the move is still not the moment the ship appears to move.
  std::uint64_t tapAtMs = 0;
  float tapFromX = 0.0f;
  float tapFromY = 0.0f;
  bool awaitingVisible = false;
  float drawnX = 0.0f;
  float drawnY = 0.0f;

  /// The frame before's, which is the only way to know whether the ship is moving RIGHT NOW. At
  /// 140 units a second a travelling ship moves about 2.3 units between frames and a parked one
  /// moves zero, so the two are not close.
  float previousDrawnX = 0.0f;
  float previousDrawnY = 0.0f;

  while (running)
  {
    dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

    const Neuron::TransportState state = transport.State();
    if (state == Neuron::TransportState::Failed)
    {
      Report(log, "probe: the transport failed -- on this side that is usually the capability or the exemption");
      break;
    }
    if (state == Neuron::TransportState::Ready && !reportedReady)
    {
      Report(log, "probe: transport ready");
      reportedReady = true;
    }

    const std::uint64_t nowMs = MillisecondsSince(start);

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

    // ONE DRAIN PER FRAME, AND IT IS `ClientFrame`'S (R20). Everything that used to be written out
    // here -- read a datagram, decode it, decide what to do with it -- is behind that class with a
    // suite over it, and this is the call plus a line in the log.
    const Outpost::ClientFrame::DrainResult drained = clientFrame.DrainPackets(queue, nowMs);

    if (drained.tokenChanged)
    {
      // **A FAILED WRITE IS LOGGED AND NOT ACTED ON.** ADR-013 names what it costs -- a client
      // that takes a second slot after a relaunch -- and there is nothing better to do about it
      // here than say so.
      const bool written = Neuron::WriteSessionToken(clientFrame.CurrentJoin().Token());
      Report(log, std::string{"SESSION token "} + (written ? "stored" : "COULD NOT BE STORED"));
    }
    if (!reportedSeat && (clientFrame.CurrentJoin().Phase() != Outpost::JoinPhase::Joining))
    {
      reportedSeat = true;
      const Outpost::JoinState& join = clientFrame.CurrentJoin();
      Report(log, "JOIN " + std::string{join.IsJoined() ? (join.Resumed() ? "rejoined" : "accepted") : "REFUSED: match full"} +
                    " player=" + std::to_string(static_cast<unsigned>(join.Player())) + " seed=" + std::to_string(join.MatchSeed()));
    }

    if (drained.datagrams > 0)
    {
      receivedCount += drained.accepted;
      const Outpost::Snapshot* newest = clientFrame.Replicas().Newest();
      // The same query the draw makes a few lines later. It walks a ring of three and allocates
      // nothing, and asking twice is cheaper than carrying the answer across half a frame.
      const Outpost::ReplicaStore::Frame shown = clientFrame.Advance(nowMs);
      Report(log, "RX datagrams=" + std::to_string(drained.datagrams) + " accepted=" + std::to_string(drained.accepted) +
                    " refused=" + std::to_string(drained.refused) + " faulted=" + std::to_string(drained.faulted) +
                    " seq=" + std::to_string(newest != nullptr ? newest->sequence : 0) +
                    " entities=" + std::to_string(newest != nullptr ? newest->entities.size() : 0) +
                    // THE DRAWN POSITION, because the last run could not answer "did it move?" from
                    // the log alone. Working that out took reading the host's uptime against the
                    // entity's journey time, to find that the ship had already arrived before the
                    // client started and that nothing moving was correct rather than a defect.
                    " drawn=" + std::to_string(drawnX) + "," + std::to_string(drawnY) +
                    " awaiting=" + std::to_string(awaitingVisible ? 1 : 0) +
                    // WHAT THE PLAYOUT CLOCK IS ACTUALLY DOING. A run that says STARVED is not
                    // drawing 75 milliseconds behind at all -- it is drawing the newest snapshot --
                    // and every latency figure taken from it would be optimistic by the whole
                    // buffer. The two sequences either side say which pair it is between.
                    " playout=" + PlayoutName(shown.playout.state) +
                    " pair=" + std::to_string(shown.older != nullptr ? shown.older->sequence : 0) + "-" +
                    std::to_string(shown.newer != nullptr ? shown.newer->sequence : 0) + " local_ms=" + std::to_string(nowMs));
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
          // **A HOLD ON EMPTY SPACE RECENTERS** (ADR-018). Nothing is selectable yet -- M1.10 is
          // selection -- so it goes to this player's station, which is the half of it
          // `GameDesign.md` section 7 actually names.
          const Neuron::Vec2 station = Outpost::StartAnchor(2, clientFrame.Player());
          clientFrame.Camera() =
            Outpost::Recenter(clientFrame.Camera(),
                              Outpost::RecenterRequest{.stationX = static_cast<float>(station.x) / static_cast<float>(Neuron::FIXED_ONE),
                                                       .stationY = static_cast<float>(station.y) / static_cast<float>(Neuron::FIXED_ONE)});
          Report(log, "RECENTER on the station");
        }
        continue;
      }

      const Outpost::Snapshot* newest = clientFrame.Replicas().Newest();
      if ((newest == nullptr) || newest->entities.empty())
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
      static_cast<void>(selection.RetainLiving(newest->entities));

      const Outpost::HitTestRequest request{.authoredX = event.xAuthoredPixels,
                                            .authoredY = event.yAuthoredPixels,
                                            .authoredWidth = static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH),
                                            .authoredHeight = static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT),
                                            .aspectRatio = tapAspect,
                                            .player = clientFrame.Player()};

      const Outpost::SelectionOutcome picked = selection.Tap(clientFrame.Camera(), request, newest->entities);

      // **THE FIRST TAP HAS ALREADY ACTED; THE SECOND ONLY UPGRADES IT** (ADR-017). Nothing is
      // deferred waiting to see whether a second tap arrives, so no tap in this game got slower.
      if ((event.tapCount >= 2) && (picked.verb == Outpost::OrderVerb::Select))
      {
        const Outpost::ExpansionOutcome expanded =
          Outpost::ExpandSelection(selection, clientFrame.Camera(), request, newest->entities, lastTapIdentity, picked.target);
        if (expanded.expanded)
        {
          Report(log, "SELECT expanded to " + std::to_string(expanded.selected) + " of one design");
        }
      }
      if (picked.verb == Outpost::OrderVerb::Select)
      {
        lastTapIdentity = picked.target;
        Report(log, "SELECT " + std::to_string(picked.target) + ", " + std::to_string(selection.Count()) + " selected");
        continue;
      }

      if (picked.verb != Outpost::OrderVerb::MoveTo)
      {
        // Attack, Mine and the build panel all need systems M1 has not finished. The verb is
        // resolved and named rather than silently dropped, so the log says which row of
        // `Interface.md` section 4's table a tap landed on.
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
      packet.commands.push_back(Outpost::BuildMoveCommand(sequence, outcome.worldX, outcome.worldY, selection.Identities()));

      bool sent = false;
      if (Outpost::Encode(packet, commandWriter))
      {
        sent = transport.Send(std::span<const std::byte>{outgoing.data(), commandWriter.WrittenBytes()});
      }

      Outpost::OrderMarker marker;
      marker.targetX = packet.commands.front().targetX;
      marker.targetY = packet.commands.front().targetY;
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
      const float movedLastFrame = std::fabs(drawnX - previousDrawnX) + std::fabs(drawnY - previousDrawnY);
      const bool parked = movedLastFrame < 0.1f;
      if (parked)
      {
        tapAtMs = nowMs;
        tapFromX = drawnX;
        tapFromY = drawnY;
        awaitingVisible = true;
      }
      else
      {
        Report(log, "TAPUNMEASURED the ship was already moving, " + std::to_string(movedLastFrame) + " units last frame");
      }

      Report(log, "TAP seq=" + std::to_string(sequence) + " authored=" + std::to_string(event.xAuthoredPixels) + "," +
                    std::to_string(event.yAuthoredPixels) + " world=" + std::to_string(outcome.worldX) + "," +
                    std::to_string(outcome.worldY) + (sent ? " sent" : " NOT SENT") +
                    " markers=" + std::to_string(clientFrame.Markers().Count()) + " local_ms=" + std::to_string(nowMs));
    }

    // M0.15's frame and M0.17's, and R13's arrangement in six lines: the world clears the SCENE
    // TARGET, the back buffer is cleared to black -- which is what the letterbox bars are -- the
    // present step fits one into the other, and the interface pass draws over that, straight into
    // the back buffer at physical resolution (ADR-011).
    //
    // THE TWO CLEARS ARE DIFFERENT COLORS ON PURPOSE. At the 1:1 default the present is a pure copy
    // and buys nothing visible, so the only way to see that it happened at all is that a blit which
    // drew nothing leaves a black screen rather than a dark blue one.
    if (swapChain.IsReady() && presentStep.IsReady() && interfacePass.IsReady() && device.BeginFrame())
    {
      static_cast<void>(sceneTarget.RecordClear(device));

      // M0.21b: THE WORLD, AND THE FIRST THING IN THIS TREE TO DRAW ONE. Into the scene target,
      // which `RecordClear` has just bound, before the present step fits it into the back buffer.
      //
      // The frame the clock hands back is a pair of snapshots and a fraction between them
      // (`TechnicalDesign.md` section 6), and every entity in the newer one is interpolated toward
      // it. Nothing here simulates: R19's line is that the host said where these are and the client
      // only says where they are BETWEEN the two things the host said.
      if (worldPass.IsReady())
      {
        const Outpost::ReplicaStore::Frame drawn = clientFrame.Advance(nowMs);
        if (drawn.newer != nullptr)
        {
          const float aspect = (sceneTarget.HeightPixels() > 0)
                                 ? (static_cast<float>(sceneTarget.WidthPixels()) / static_cast<float>(sceneTarget.HeightPixels()))
                                 : 1.0f;
          const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(clientFrame.Camera(), aspect);

          for (const Outpost::EntityRecord& newer : drawn.newer->entities)
          {
            // The older snapshot's record for this entity, matched on the WHOLE packed identity so
            // a reused slot is two ships rather than one that teleported (`Interpolation.h`).
            Outpost::EntityRecord shown = newer;
            if (drawn.older != nullptr)
            {
              for (const Outpost::EntityRecord& older : drawn.older->entities)
              {
                if (older.identity == newer.identity)
                {
                  static_cast<void>(Outpost::InterpolateRecord(older, newer, drawn.playout.fraction, shown));
                  break;
                }
              }
            }

            const Outpost::Matrix4 world = Outpost::EntityTransform(
              static_cast<float>(Outpost::DequantizePosition(shown.positionX)) / static_cast<float>(Neuron::FIXED_ONE),
              static_cast<float>(Outpost::DequantizePosition(shown.positionY)) / static_cast<float>(Neuron::FIXED_ONE),
              Outpost::DequantizeWireHeading(shown.heading));

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

            // The first entity's drawn position is what M0.23 watches. One entity is all M0 has,
            // and taking the first is honest for exactly as long as that is true.
            if (&newer == &drawn.newer->entities.front())
            {
              previousDrawnX = drawnX;
              previousDrawnY = drawnY;
              drawnX = static_cast<float>(Outpost::DequantizePosition(shown.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
              drawnY = static_cast<float>(Outpost::DequantizePosition(shown.positionY)) / static_cast<float>(Neuron::FIXED_ONE);

              // A QUARTER OF A WORLD UNIT IS THE WIRE'S OWN STEP (ADR-003), so anything at or below
              // it is the quantizer rather than movement. Half a unit is comfortably above that and
              // far below anything a player would call a move.
              if (awaitingVisible && ((std::fabs(drawnX - tapFromX) > 0.5f) || (std::fabs(drawnY - tapFromY) > 0.5f)))
              {
                Report(log, "TAPVISIBLE ms=" + std::to_string(nowMs - tapAtMs) + " from=" + std::to_string(tapFromX) + "," +
                              std::to_string(tapFromY) + " to=" + std::to_string(drawnX) + "," + std::to_string(drawnY) +
                              " playout=" + PlayoutName(drawn.playout.state));
                awaitingVisible = false;
              }
            }
          }
        }
      }

      // M0.16's hard one-pixel edge, drawn into the scene target so the present step has something
      // whose resampling is visible. At 1:1 it must reach the glass one physical pixel wide and
      // fully white; at a 0.5 scale, two pixels wide and still fully white. Gray is the failure.
      static_cast<void>(sceneTarget.RecordCalibrationPattern(device));

      static_cast<void>(swapChain.RecordBindAndClear(device, 0.0f, 0.0f, 0.0f));
      static_cast<void>(presentStep.Record(device, sceneTarget, worldFit));

      // ADR-011'S ORDERING CONSTRAINT, and it lives here because it is the caller's: after the
      // present blit, before the back buffer is closed. Moving this line above the blit draws the
      // interface and then paints the world over it, which is a defect nothing in either class can
      // catch.
      static_cast<void>(interfacePass.Record(device, swapChain, interfaceFit,
                                             {.rect = INTERFACE_PROBE_RECT, .red = 0.98f, .green = 0.73f, .blue = 0.18f, .alpha = 1.0f}));
      static_cast<void>(swapChain.RecordReadyToPresent(device));
      if (device.EndFrameAndSubmit() && swapChain.Present())
      {
        device.PresentedFrame();
        ++presentedFrames;

        // AFTER PresentedFrame, which is the wait that makes the slot's timestamps readable, and
        // only once the warm-up frames are behind us.
        if (presentedFrames > WARMUP_FRAMES)
        {
          gpuTime.Add(device.LastFrameGpuMicroseconds());
        }
        if (presentedFrames >= (WARMUP_FRAMES + MEASURE_FRAMES))
        {
          Report(log, "probe: measurement window complete");
          running = false;
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

  interfacePass.Destroy();
  presentStep.Destroy();
  sceneTarget.Destroy();
  swapChain.Destroy();
  device.Destroy();
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

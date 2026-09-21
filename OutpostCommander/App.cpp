#include "pch.h"

// NOT in pch.h, and it has to stay out of it: the Storage projection is large enough that
// adding it to this project's precompiled header fails the build outright with C3859 and
// C1076, the compiler running out of room for the PCH itself. It is needed by exactly one
// function here -- ADR-008's LocalState folder -- so it is included by exactly one file.
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>

#include <array>
#include <chrono>
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
void ReportLibrary(std::string_view _name)
{
  std::string line{_name};
  line.push_back('\n');
  OutputDebugStringA(line.c_str());
}

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
inline constexpr std::chrono::milliseconds HELLO_INTERVAL{1000};

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
[[nodiscard]] std::string ReadHostAddress(const std::filesystem::path& _path)
{
  std::ifstream file{_path};
  std::string line;
  if (file && std::getline(file, line))
  {
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    const std::size_t last = line.find_last_not_of(" \t\r\n");
    if (first != std::string::npos)
    {
      return line.substr(first, (last - first) + 1);
    }
  }
  return std::string{"127.0.0.1"};
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

  const std::string host = ReadHostAddress(localState / L"host.txt");
  Report(log, "probe: LocalState is " + localState.string());
  Report(log, "probe: host " + host + " port " + std::to_string(Neuron::ProbePacket::PORT));

  Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
  Neuron::DatagramTransport transport{queue};
  if (!transport.Open(host, Neuron::ProbePacket::PORT))
  {
    Report(log, "probe: the socket could not be created at all");
    return;
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
  if (!device.Create())
  {
    Report(log, "probe: no Direct3D 12 device, hresult " + std::to_string(device.LastHresult()));
  }
  else if (!swapChain.Create(device, winrt::get_unknown(_window), Neuron::PhysicalWidth(metrics), Neuron::PhysicalHeight(metrics)))
  {
    Report(log, "probe: no swap chain, hresult " + std::to_string(swapChain.LastHresult()));
  }
  else
  {
    Report(log, "probe: swap chain " + std::to_string(swapChain.WidthPixels()) + "x" + std::to_string(swapChain.HeightPixels()) +
                  " physical, from " + std::to_string(static_cast<int>(metrics.widthDips)) + "x" +
                  std::to_string(static_cast<int>(metrics.heightDips)) + " dips at " + std::to_string(metrics.rawPixelsPerViewPixel) + "x");
  }

  bool running = true;
  const auto closed = _window.Closed(winrt::auto_revoke, [&running](const auto&, const auto&) { running = false; });
  std::uint64_t presentedFrames = 0;

  const CoreDispatcher dispatcher = _window.Dispatcher();
  const auto start = std::chrono::steady_clock::now();
  auto nextHello = start;
  bool reportedReady = false;
  std::uint16_t helloSequence = 0;
  std::uint64_t receivedCount = 0;
  std::array<std::byte, QUEUE_SLOT_BYTES> datagram{};

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

    const auto now = std::chrono::steady_clock::now();
    if (state == Neuron::TransportState::Ready && now >= nextHello)
    {
      std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> hello{};
      Neuron::ByteWriter writer{hello};
      const Neuron::ProbePacket packet{.sequence = helloSequence, .sentAtMs = MillisecondsSince(start)};
      if (packet.Write(writer))
      {
        const bool sent = transport.Send(std::span<const std::byte>{hello.data(), writer.WrittenBytes()});
        Report(log, "TX hello seq=" + std::to_string(helloSequence) + (sent ? " sent" : " refused"));
      }
      ++helloSequence;
      nextHello = now + HELLO_INTERVAL;
    }

    std::size_t byteCount = 0;
    while (queue.Drain(datagram, byteCount))
    {
      const std::uint64_t arrivedAtMs = MillisecondsSince(start);
      Neuron::ByteReader reader{std::span<const std::byte>{datagram.data(), byteCount}};
      Neuron::ProbePacket packet{};
      const Neuron::PacketFault fault = Neuron::ProbePacket::Read(reader, packet);
      if (fault != Neuron::PacketFault::None)
      {
        Report(log, "RXBAD fault=" + std::to_string(static_cast<unsigned>(fault)));
        continue;
      }

      ++receivedCount;
      Report(log, "RX seq=" + std::to_string(packet.sequence) + " host_ms=" + std::to_string(packet.sentAtMs) +
                    " local_ms=" + std::to_string(arrivedAtMs));
    }

    // Clear and present. All of M0.13's drawing, and R13 puts the real passes in a scene target
    // rather than straight into this buffer -- that is M0.15's, behind ADR-012 and M0.16's gate.
    if (swapChain.IsReady() && device.BeginFrame())
    {
      static_cast<void>(swapChain.RecordClear(device, 0.02f, 0.04f, 0.09f));
      if (device.EndFrameAndSubmit() && swapChain.Present())
      {
        device.PresentedFrame();
        ++presentedFrames;
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
  Report(log, "probe: received " + std::to_string(receivedCount) + " dropped " + std::to_string(queue.DroppedCount()) + " rejected " +
                std::to_string(queue.RejectedCount()) + " oversized " + std::to_string(transport.OversizedCount()) + " skippedSends " +
                std::to_string(transport.SkippedSendCount()) + " presented " + std::to_string(presentedFrames));
  transport.Close();
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
    // The one thing this shell does today: name the libraries it was linked against, so that a
    // deploy proves the whole chain reached the package rather than merely compiled.
    ReportLibrary(Neuron::CoreLibraryName());
    ReportLibrary(Neuron::ClientLibraryName());
    ReportLibrary(Outpost::CoreLibraryName());
    ReportLibrary(Outpost::ClientLibraryName());
  }

  void Load(const winrt::hstring&) {}

  void SetWindow(const CoreWindow&) {}

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

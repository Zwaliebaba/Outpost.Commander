#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace Outpost
{

/// What the command line asked for: `--warp` for the software rasteriser, `--capture <landscape>
/// <ticks> <directory>` for the headless path (TechnicalDesign.md §6.1).
struct LaunchOptions
{
  bool warp = false;
  bool capture = false;
  /// Presents with a sync interval of zero, so that a frame finishes when the renderer is done
  /// rather than when the display refreshes. m0-foundation/T22 asked the owner for a frame time
  /// and got 8 to 16 ms, which was a 120 Hz and a 60 Hz monitor and not this renderer at all.
  bool noVerticalSync = false;
  /// SIMULATION TICKS AND NOT RENDERED FRAMES (m1-vertical-slice/G2). M0's capture ran for a number
  /// of frames and advanced the match by however long those frames took on WARP, which makes every
  /// run a different match and the frames of two runs incomparable. A capture is now a fixed number
  /// of ticks run as fast as the machine will run them, and a frame is written every hundredth.
  std::uint32_t captureTicks = 0;
  /// The landscape to play it on, by the file stem the content tree knows it as.
  std::string captureLandscape;
  std::filesystem::path captureDirectory;
};

/// The arguments as CommandLineToArgvW splits them, the executable first; false for anything
/// that is not the two options above, spelled exactly.
[[nodiscard]] bool ParseCommandLine(std::span<const std::wstring> _arguments, LaunchOptions& _options);

/// The process exit codes. A capture that saw a debug-layer warning or error exits with
/// EXIT_DEBUG_MESSAGES, which is what fails the CI job (TechnicalDesign.md §10).
inline constexpr int EXIT_CLEAN = 0;
inline constexpr int EXIT_FAILED = 1;
inline constexpr int EXIT_DEBUG_MESSAGES = 2;

/// The game: a window over the primary monitor with the scene target presented scaled into it, or,
/// with --capture, no window and no swap chain but the scene target written to disk every hundredth
/// frame (ADR-004). A failed HRESULT anywhere below is caught once here, logged and fatal.
class App
{
public:
  explicit App(const LaunchOptions& _options);

  [[nodiscard]] int Run();

private:
  [[nodiscard]] int RunWindowed();
  [[nodiscard]] int RunCapture();

  LaunchOptions m_options;
};

} // namespace Outpost

#include "pch.h"

#include <shellapi.h>
#include <shlobj.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
// The overseer (ADR-102): it oversees the universe whose Overseer.json it is given, or the default universe's, in a window of
// its own, drawn as the game's interface is.
constexpr auto OVERSEER_TITLE = L"Outpost Overseer";
// Linear color, behind the overview's panels.
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.0f, 0.0f, 1.0f};
// The client area it opens with, centered on the primary monitor.
constexpr std::uint32_t WINDOWED_WIDTH_PIXELS = 1600;
constexpr std::uint32_t WINDOWED_HEIGHT_PIXELS = 900;
// Minimized there is no frame to wait for, so the loop wakes this often for the link to keep taking what the server says.
constexpr DWORD MINIMIZED_WAKE_MILLISECONDS = 16;
// The default universe's folder, in the owner's documents, where OutpostServer runs it with no folder (ADR-078 decision 1).
constexpr std::wstring_view DEFAULT_UNIVERSE_FOLDER = L"Outpost Commander\\Universe";
constexpr auto MODELS_FILE = L"Models.json";

std::string Utf8(const std::filesystem::path& _path)
{
  return winrt::to_string(_path.wstring());
}

// The command line's arguments after the program's name, split by Windows' rules for quotes and backslashes, as the game's
// and the server's shells read theirs.
std::vector<std::wstring> CommandLineArguments()
{
  struct LocalFreer
  {
    void operator()(LPWSTR* _arguments) const noexcept
    {
      (void)LocalFree(static_cast<HLOCAL>(_arguments));
    }
  };

  int count = 0;
  const std::unique_ptr<LPWSTR, LocalFreer> arguments(winrt::check_pointer(CommandLineToArgvW(GetCommandLineW(), &count)));
  const std::span<const LPWSTR> all(arguments.get(), static_cast<size_t>(std::max(count, 0)));
  return all.empty() ? std::vector<std::wstring>() : std::vector<std::wstring>(all.begin() + 1, all.end());
}

// The default universe's folder in the owner's documents. Throws when Windows does not say where the documents are.
std::filesystem::path DefaultUniverseFolder()
{
  PWSTR documents = nullptr;
  const HRESULT found = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents);
  std::filesystem::path folder;
  if (SUCCEEDED(found))
    folder = std::filesystem::path(documents) / DEFAULT_UNIVERSE_FOLDER;
  CoTaskMemFree(documents);
  winrt::check_hresult(found);
  return folder;
}

// The universe's Overseer.json (ADR-101 decision 2): the argument, the file or the folder that holds it, or with none the
// default universe's. Throws Neuron::Exception, saying where it looked, when there is none.
std::filesystem::path OverseerFile(const std::vector<std::wstring>& _arguments)
{
  if (_arguments.size() > 1)
    throw Neuron::Exception("The overseer takes one argument: a universe's folder, or its Overseer.json.");
  std::filesystem::path path = _arguments.empty() ? DefaultUniverseFolder() : std::filesystem::path(_arguments.front());
  if (std::filesystem::is_directory(path))
    path /= Outpost::OVERSEER_TICKET_FILE;
  if (!std::filesystem::is_regular_file(path))
  {
    throw Neuron::Exception(std::format("There is no {} at {}.\n\nThe dedicated server writes it into its universe's folder each time it "
                                        "runs. Run OutpostServer first, or name the universe's folder or its {}.",
                                        Outpost::OVERSEER_TICKET_FILE, Utf8(path), Outpost::OVERSEER_TICKET_FILE));
  }
  return path;
}

std::string ReadText(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    throw Neuron::Exception(std::format("{} cannot be read.", Utf8(_path)));
  return {std::istreambuf_iterator<char>(file), {}};
}

// Each seat's color that has a set (Hud::SideColorsOf), from the game's Models.json beside the executable.
std::vector<Outpost::Hud::SideColor> SeatColors()
{
  const Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(MODELS_FILE);
  if (bytes.empty())
    throw Neuron::Exception("The game asset Assets\\Models.json is missing or cannot be read.");
  try
  {
    return Outpost::Hud::SideColorsOf(Outpost::LoadModelCatalog({reinterpret_cast<const char*>(bytes.data()), bytes.size()}));
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("Assets\\Models.json: {}", error.what()));
  }
}

// The renderer on the hardware adapter, as the game's (ADR-006 decision 6): the overseer takes no software adapter, so on a
// machine with none it says where to run instead.
std::unique_ptr<Neuron::Renderer> CreateRenderer(const Neuron::Window& _window)
{
  try
  {
    return std::make_unique<Neuron::Renderer>(_window.Handle(), _window.ClientWidthPixels(), _window.ClientHeightPixels());
  }
  catch (const winrt::hresult_error& error)
  {
    if (error.code() != DXGI_ERROR_UNSUPPORTED)
      throw;
    throw Neuron::Exception("No graphics adapter in this PC runs Direct3D 12, which the overseer draws with.\n\nOn a server without one, "
                            "run the overseer from a PC on the same network: open the server's overseer address to it, and copy the "
                            "universe's Overseer.json there.");
  }
}

void ReportFailure(std::wstring_view _message)
{
  MessageBoxW(nullptr, std::wstring(_message).c_str(), OVERSEER_TITLE, MB_OK | MB_ICONERROR);
}
} // namespace

int WINAPI wWinMain([[maybe_unused]] HINSTANCE _hInstance, [[maybe_unused]] HINSTANCE _hPrevInstance, [[maybe_unused]] LPWSTR _cmdLine,
                    [[maybe_unused]] int _cmdShow)
{
  // A failure anywhere below, at startup or when the graphics device is lost, is shown once, and its code is the exit code,
  // as the game's is (ADR-006).
  try
  {
    // Models.json is read from Assets beside the executable, as the game reads its own (ADR-008).
    std::array<wchar_t, MAX_PATH> filename{};
    (void)GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    Neuron::FileSys::SetHomeDirectory(std::filesystem::path(filename.data()).parent_path().wstring());

    const std::filesystem::path ticketFile = OverseerFile(CommandLineArguments());
    Outpost::OverseerTicket ticket;
    try
    {
      ticket = Outpost::ReadOverseerTicket(ReadText(ticketFile));
    }
    catch (const Neuron::Exception& error)
    {
      throw Neuron::Exception(std::format("{}: {}", Utf8(ticketFile), error.what()));
    }
    Outpost::OverseerScreen screen(SeatColors());

    Neuron::Window window({.title = OVERSEER_TITLE,
                           .windowedClientWidthPixels = WINDOWED_WIDTH_PIXELS,
                           .windowedClientHeightPixels = WINDOWED_HEIGHT_PIXELS,
                           .fullScreen = false,
                           .holdsCursor = false});
    const std::unique_ptr<Neuron::Renderer> renderer = CreateRenderer(window);
    Neuron::UiPipeline ui(*renderer, Outpost::Hud::Typefaces(), Outpost::Hud::Sprites(),
                          Outpost::RasterizeInterface(window.ClientWidthPixels(), window.ClientHeightPixels()));
    const Outpost::OverseerLink link{.server = Outpost::Hud::ServerName(ticket.address.host, ticket.address.port)};
    Outpost::OverseerClient client(std::move(ticket));
    window.Show();

    for (;;)
    {
      // Minimized, there is nothing to show, so the loop sleeps until a message arrives or the link is due again; otherwise
      // it waits until the swap chain can take a frame (ADR-006).
      if (window.IsMinimized())
        MsgWaitForMultipleObjects(0, nullptr, FALSE, MINIMIZED_WAKE_MILLISECONDS, QS_ALLINPUT);
      else
        renderer->WaitForNextFrame();
      if (!window.ProcessMessages())
        break;
      const auto now = std::chrono::steady_clock::now();
      client.Update(now);
      const Neuron::InputState input = window.ReadInput();
      if (window.IsMinimized())
        continue;

      renderer->Resize(window.ClientWidthPixels(), window.ClientHeightPixels());
      const std::uint32_t width = renderer->WidthPixels();
      const std::uint32_t height = renderer->HeightPixels();
      screen.Input(input);
      // The seat whose sight the owner picked, which the server sends from here on.
      if (const std::optional<Outpost::PlayerId> sight = screen.TakeSight())
        client.Watch(*sight);
      // The overview is laid out with the fonts at its scale, which the atlas is rasterized at first when it moves.
      const float scale = Outpost::Hud::Scale(width, height);
      ui.UseScale(scale);
      const Outpost::Hud::TextMetrics metrics(ui.Fonts(), scale);
      Outpost::OverseerLink state = link;
      state.connected = client.Connected();
      state.roundTrip = client.RoundTrip();
      const Neuron::DrawList& layout = screen.Lay(client.Model(), state, now, metrics, width, height);

      ID3D12GraphicsCommandList* commandList = renderer->BeginFrame(CLEAR_COLOR);
      renderer->BeginInterface();
      ui.Begin(width, height, scale);
      ui.Draw(layout, {.hatchPeriodPixels = Outpost::Hud::HATCH_PERIOD_UNITS * scale,
                       .hatchStripePixels = Outpost::Hud::HATCH_STRIPE_UNITS * scale});
      ui.End(commandList, renderer->FrameIndex());
      renderer->EndFrame();
    }
    return window.ExitCode();
  }
  catch (const winrt::hresult_error& error)
  {
    ReportFailure(std::format(L"{}\n\nError 0x{:08X}", std::wstring_view(error.message()), static_cast<std::uint32_t>(error.code())));
    return error.code();
  }
  catch (const std::exception& error)
  {
    // Neuron::Exception carries UTF-8, such as the ticket's reader's report of a bad file.
    ReportFailure(winrt::to_hstring(std::string_view(error.what())));
    return EXIT_FAILURE;
  }
}

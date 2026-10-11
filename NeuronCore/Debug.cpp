#include "pch.h"
#include "Debug.h"

void Neuron::FailFast(const char* _message) noexcept
{
  OutputDebugStringA(_message);
  OutputDebugStringA("\n");
  // The dedicated server's console. A windowed game has no standard error.
  const HANDLE standardError = SafeHandle(GetStdHandle(STD_ERROR_HANDLE));
  if (standardError != nullptr)
  {
    const std::string_view message(_message);
    DWORD written = 0;
    (void)WriteFile(standardError, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
    (void)WriteFile(standardError, "\r\n", 2, &written, nullptr);
  }
  // Stops in a debugger when one is attached; otherwise Windows Error Reporting takes the process as it stands.
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

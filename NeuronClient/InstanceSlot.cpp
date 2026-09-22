#include "pch.h"

#include "InstanceSlot.h"

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

#include <string>
#include <string_view>

namespace Neuron
{

std::wstring InstanceFileName(std::wstring_view _fileName, std::uint32_t _slot)
{
  if (_slot == 0)
  {
    return std::wstring{_fileName};
  }

  const std::wstring suffix = L"-" + std::to_wstring(_slot);
  const std::size_t dot = _fileName.find_last_of(L'.');
  if ((dot == std::wstring_view::npos) || (dot == 0))
  {
    return std::wstring{_fileName} + suffix;
  }
  return std::wstring{_fileName.substr(0, dot)} + suffix + std::wstring{_fileName.substr(dot)};
}

std::uint32_t ClaimInstanceSlot() noexcept
{
  // `FindOrRegisterInstanceForKey` hands back whichever instance holds the key, registering this one when
  // none does -- so `IsCurrentInstance` is "this slot is now mine". The registration ends with the
  // process, which is what frees a slot when a client closes.
  try
  {
    for (std::uint32_t slot = 0; slot < INSTANCE_SLOT_COUNT; ++slot)
    {
      const winrt::hstring key{L"slot-" + std::to_wstring(slot)};
      if (winrt::Windows::ApplicationModel::AppInstance::FindOrRegisterInstanceForKey(key).IsCurrentInstance())
      {
        return slot;
      }
    }
    return INSTANCE_SLOT_COUNT - 1;
  }
  catch (...)
  {
    return 0;
  }
}

} // namespace Neuron

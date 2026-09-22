#include "pch.h"

#include "AtlasPacker.h"

namespace Neuron
{

void AtlasPacker::Reset(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  m_widthPixels = _widthPixels;
  m_heightPixels = _heightPixels;
  m_shelfTop = 0;
  m_shelfHeightPixels = 0;
  m_cursorX = 0;
}

bool AtlasPacker::Place(std::uint32_t _widthPixels, std::uint32_t _heightPixels, AtlasSlot& _outSlot) noexcept
{
  // A rectangle wider than the atlas can never be placed, on this shelf or any other, so it is refused
  // here rather than after opening an empty shelf for it.
  if (_widthPixels > m_widthPixels)
  {
    return false;
  }

  // **THE SHELF ADVANCES ONLY WHEN THE ROW IS FULL**, and the test is the cursor rather than a count:
  // a rectangle fits on this shelf when what is left of the row is wide enough for it.
  if ((m_cursorX + _widthPixels) > m_widthPixels)
  {
    m_shelfTop += m_shelfHeightPixels;
    m_shelfHeightPixels = 0;
    m_cursorX = 0;
  }

  // The new shelf may be taller than what is left of the atlas, and so may the first rectangle on an
  // existing one -- both are the same check and it happens after the advance above, never before.
  if ((m_shelfTop + _heightPixels) > m_heightPixels)
  {
    return false;
  }

  _outSlot.left = m_cursorX;
  _outSlot.top = m_shelfTop;
  _outSlot.widthPixels = _widthPixels;
  _outSlot.heightPixels = _heightPixels;

  m_cursorX += _widthPixels;
  if (_heightPixels > m_shelfHeightPixels)
  {
    m_shelfHeightPixels = _heightPixels;
  }

  return true;
}

std::uint32_t AtlasPacker::WidthPixels() const noexcept
{
  return m_widthPixels;
}

std::uint32_t AtlasPacker::HeightPixels() const noexcept
{
  return m_heightPixels;
}

std::uint32_t AtlasPacker::UsedHeightPixels() const noexcept
{
  return m_shelfTop + m_shelfHeightPixels;
}

} // namespace Neuron

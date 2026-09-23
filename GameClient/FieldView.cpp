#include "pch.h"

#include "FieldView.h"

namespace Outpost
{

bool FieldView::Derive(std::uint64_t _matchSeed, std::size_t _playerCount)
{
  if (_playerCount == 0)
  {
    const bool hadField = IsDerived();
    Clear();
    return hadField;
  }

  if (IsDerived() && (_matchSeed == m_matchSeed) && (_playerCount == m_playerCount))
  {
    return false;
  }

  // THE ONE CALL, AND IT IS THE HOST'S FUNCTION. Nothing here adjusts, filters or reorders what comes
  // back: a client-side correction to a generated row is exactly how the two sides would come to
  // disagree about where a rock is.
  m_rocks = GenerateField(_matchSeed, _playerCount);
  m_matchSeed = _matchSeed;
  m_playerCount = _playerCount;
  return true;
}

void FieldView::Clear() noexcept
{
  m_rocks.clear();
  m_matchSeed = 0;
  m_playerCount = 0;
}

} // namespace Outpost

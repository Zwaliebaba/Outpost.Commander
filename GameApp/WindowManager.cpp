#include "pch.h"
#include "WindowManager.h"

#include <algorithm>

void Outpost::WindowManager::Open(WindowKind _window)
{
  const bool underTheOre = _window == WindowKind::Production || _window == WindowKind::Research;
  if (underTheOre && !IsOpen(_window) && !m_stack.PositionOf(_window).has_value())
  {
    for (std::size_t slot = 0; slot < SLOTS; ++slot)
    {
      if (std::ranges::find(m_slots, slot) == m_slots.end())
      {
        m_slots[IndexOf(_window)] = slot;
        break;
      }
    }
  }
  m_stack.Open(_window);
}

void Outpost::WindowManager::Close(WindowKind _window) noexcept
{
  m_stack.Close(_window);
  m_slots[IndexOf(_window)].reset();
}

std::optional<std::size_t> Outpost::WindowManager::SlotOf(WindowKind _window) const noexcept
{
  return m_slots[IndexOf(_window)];
}

bool Outpost::WindowManager::IsOpen(WindowKind _window) const noexcept
{
  return m_stack.IsOpen(_window);
}

bool Outpost::WindowManager::CloseFront() noexcept
{
  const std::optional<WindowKind> closed = m_stack.CloseFront();
  if (closed.has_value())
    m_slots[IndexOf(*closed)].reset();
  return closed.has_value();
}

void Outpost::WindowManager::CloseAll() noexcept
{
  m_stack.CloseAll();
  m_slots = {};
}

void Outpost::WindowManager::Settle(WindowKind _window, Point _corner) noexcept
{
  m_stack.Settle(_window, _corner);
}

std::optional<Outpost::WindowManager::Point> Outpost::WindowManager::PositionOf(WindowKind _window) const noexcept
{
  return m_stack.PositionOf(_window);
}

void Outpost::WindowManager::Grab(WindowKind _window, Point _pointer, Point _corner)
{
  m_stack.Grab(_window, _pointer, _corner);
  // Moved, it stands in no slot, and another window may take its place.
  m_slots[IndexOf(_window)].reset();
}

void Outpost::WindowManager::Drag(Point _pointer) noexcept
{
  m_stack.Drag(_pointer);
}

void Outpost::WindowManager::Release() noexcept
{
  m_stack.Release();
}

void Outpost::WindowManager::Place(WindowKind _window, Point _corner) noexcept
{
  m_stack.Place(_window, _corner);
  m_slots[IndexOf(_window)].reset();
}

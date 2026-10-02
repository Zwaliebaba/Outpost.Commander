#include "pch.h"
#include "WindowManager.h"

#include <algorithm>

void Outpost::WindowManager::Open(WindowKind _window)
{
  BringToFront(_window);
}

void Outpost::WindowManager::Close(WindowKind _window) noexcept
{
  std::erase(m_order, _window);
  if (m_grabbed == _window)
    m_grabbed.reset();
}

bool Outpost::WindowManager::IsOpen(WindowKind _window) const noexcept
{
  return std::ranges::find(m_order, _window) != m_order.end();
}

bool Outpost::WindowManager::CloseFront() noexcept
{
  if (m_order.empty())
    return false;
  Close(m_order.front());
  return true;
}

void Outpost::WindowManager::CloseAll() noexcept
{
  m_order.clear();
  m_grabbed.reset();
}

void Outpost::WindowManager::Settle(WindowKind _window, Point _corner) noexcept
{
  if (m_positions[IndexOf(_window)].has_value())
    m_positions[IndexOf(_window)] = _corner;
}

std::optional<Outpost::WindowManager::Point> Outpost::WindowManager::PositionOf(WindowKind _window) const noexcept
{
  return m_positions[IndexOf(_window)];
}

void Outpost::WindowManager::Grab(WindowKind _window, Point _pointer, Point _corner)
{
  BringToFront(_window);
  m_positions[IndexOf(_window)] = _corner;
  m_grabbed = _window;
  m_grabOffset = {.xUnits = _pointer.xUnits - _corner.xUnits, .yUnits = _pointer.yUnits - _corner.yUnits};
}

void Outpost::WindowManager::Drag(Point _pointer) noexcept
{
  if (m_grabbed.has_value())
    m_positions[IndexOf(*m_grabbed)] =
      Point{.xUnits = _pointer.xUnits - m_grabOffset.xUnits, .yUnits = _pointer.yUnits - m_grabOffset.yUnits};
}

void Outpost::WindowManager::Release() noexcept
{
  m_grabbed.reset();
}

void Outpost::WindowManager::BringToFront(WindowKind _window)
{
  std::erase(m_order, _window);
  m_order.insert(m_order.begin(), _window);
}

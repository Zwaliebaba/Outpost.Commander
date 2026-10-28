#include "pch.h"
#include "Interaction.h"

#include <utility>

namespace
{
// An identifier is a 64-bit FNV-1a hash, each label or index hashed onto its parent's after a byte that says which it is,
// so that the label "1" and the index 1 differ.
constexpr std::uint64_t HASH_START = 14695981039346656037ULL;
constexpr std::uint64_t HASH_PRIME = 1099511628211ULL;
constexpr std::uint8_t LABEL_MARK = 'L';
constexpr std::uint8_t INDEX_MARK = 'I';

constexpr std::uint64_t Mix(std::uint64_t _hash, std::uint8_t _byte) noexcept
{
  return (_hash ^ _byte) * HASH_PRIME;
}

// No hash is NO_WIDGET.
constexpr Neuron::WidgetId Named(std::uint64_t _hash) noexcept
{
  return _hash == Neuron::NO_WIDGET ? 1 : _hash;
}
} // namespace

void Neuron::Interaction::BeginBuild() noexcept
{
  // A field the last build did not build, as one in a window since closed, has given back the keyboard, and a drop-down
  // it did not build has closed.
  if (m_focused != NO_WIDGET && !m_claimed.contains(m_focused))
    m_focused = NO_WIDGET;
  if (m_dropDown != NO_WIDGET && !m_claimed.contains(m_dropDown))
    m_dropDown = NO_WIDGET;
  m_ids.clear();
  m_built.swap(m_claimed);
  m_claimed.clear();
}

Neuron::WidgetId Neuron::Interaction::IdOf(std::string_view _label) const noexcept
{
  return IdUnder(m_ids.empty() ? HASH_START : m_ids.back(), _label);
}

Neuron::WidgetId Neuron::Interaction::IdUnder(WidgetId _parent, std::string_view _label) noexcept
{
  std::uint64_t hash = Mix(_parent, LABEL_MARK);
  for (const char character : _label)
    hash = Mix(hash, static_cast<std::uint8_t>(character));
  return Named(hash);
}

Neuron::WidgetId Neuron::Interaction::IdOf(std::uint64_t _index) const noexcept
{
  std::uint64_t hash = Mix(m_ids.empty() ? HASH_START : m_ids.back(), INDEX_MARK);
  for (std::size_t shift = 0; shift < 64; shift += 8)
    hash = Mix(hash, static_cast<std::uint8_t>(_index >> shift));
  return Named(hash);
}

void Neuron::Interaction::PushId(WidgetId _widget)
{
  m_ids.push_back(_widget);
}

void Neuron::Interaction::PopId()
{
  ASSERT(!m_ids.empty());
  m_ids.pop_back();
}

bool Neuron::Interaction::Claim(WidgetId _widget)
{
  return m_claimed.insert(_widget).second;
}

bool Neuron::Interaction::WasBuilt(WidgetId _widget) const noexcept
{
  return m_built.contains(_widget);
}

void Neuron::Interaction::Move(const DrawList& _list, float _xPixels, float _yPixels)
{
  m_pointerYPixels = _yPixels;
  const CanvasRegion* region = _list.PointedAt(_xPixels, _yPixels);
  m_hot = region == nullptr ? NO_WIDGET : region->widget;
  if (m_active != NO_WIDGET && m_hot != m_active)
    m_hot = NO_WIDGET;
}

bool Neuron::Interaction::Dismiss(const DrawList& _list, float _xPixels, float _yPixels) noexcept
{
  if (m_dropDown == NO_WIDGET)
    return false;
  const std::size_t layer = _list.LayerAt(_xPixels, _yPixels);
  if (layer != 0 && _list.layers[layer - 1].overlay)
    return false;
  CloseDropDown();
  return true;
}

bool Neuron::Interaction::Press(const DrawList& _list, float _xPixels, float _yPixels)
{
  if (Dismiss(_list, _xPixels, _yPixels))
    return true;
  const CanvasRegion* region = _list.Covers(_xPixels, _yPixels) ? _list.HitAt(_xPixels, _yPixels) : nullptr;
  m_active = region == nullptr ? NO_WIDGET : region->widget;
  if (region != nullptr && region->dropsDown)
    OpenDropDown(region->widget);
  m_dragFromYPixels = _yPixels;
  if (m_focused != NO_WIDGET && m_focused != m_active)
    m_focused = NO_WIDGET;
  Move(_list, _xPixels, _yPixels);
  return _list.Covers(_xPixels, _yPixels);
}

Neuron::WidgetId Neuron::Interaction::Release(const DrawList& _list, float _xPixels, float _yPixels)
{
  const WidgetId pressed = m_active;
  m_active = NO_WIDGET;
  const CanvasRegion* region = _list.HitAt(_xPixels, _yPixels);
  Move(_list, _xPixels, _yPixels);
  return pressed != NO_WIDGET && region != nullptr && region->widget == pressed ? pressed : NO_WIDGET;
}

float Neuron::Interaction::TakeDragPixels() noexcept
{
  const float moved = m_pointerYPixels - m_dragFromYPixels;
  m_dragFromYPixels = m_pointerYPixels;
  return moved;
}

bool Neuron::Interaction::Wheel(const DrawList& _list, float _xPixels, float _yPixels, float _units)
{
  const CanvasRegion* region = _list.ScrollAt(_xPixels, _yPixels);
  if (region == nullptr)
    return false;
  m_scrolls[region->widget] += _units;
  return true;
}

void Neuron::Interaction::Focus(WidgetId _widget) noexcept
{
  m_focused = _widget;
}

bool Neuron::Interaction::Type(char32_t _character)
{
  if (m_focused == NO_WIDGET)
    return false;
  Stroke(m_focused, {.key = FieldKey::Character, .character = _character});
  return true;
}

bool Neuron::Interaction::Key(FieldKey _key)
{
  if (m_focused == NO_WIDGET)
    return false;
  Stroke(m_focused, {.key = _key});
  if (_key == FieldKey::Enter || _key == FieldKey::Escape)
    m_focused = NO_WIDGET;
  return true;
}

bool Neuron::Interaction::Escape()
{
  if (m_focused != NO_WIDGET)
    return Key(FieldKey::Escape);
  if (m_dropDown != NO_WIDGET)
  {
    CloseDropDown();
    return true;
  }
  return false;
}

std::vector<Neuron::FieldStroke> Neuron::Interaction::TakeStrokes(WidgetId _widget)
{
  if (_widget != m_strokesFor)
    return {};
  return std::exchange(m_strokes, {});
}

float Neuron::Interaction::ScrollOf(WidgetId _widget) const noexcept
{
  const auto found = m_scrolls.find(_widget);
  return found == m_scrolls.end() ? 0.0f : found->second;
}

void Neuron::Interaction::SetScroll(WidgetId _widget, float _offsetUnits)
{
  m_scrolls[_widget] = _offsetUnits;
}

std::size_t Neuron::Interaction::CaretOf(WidgetId _widget) const noexcept
{
  const auto found = m_carets.find(_widget);
  return found == m_carets.end() ? 0 : found->second;
}

void Neuron::Interaction::SetCaret(WidgetId _widget, std::size_t _caret)
{
  m_carets[_widget] = _caret;
}

void Neuron::Interaction::OpenDropDown(WidgetId _widget) noexcept
{
  m_dropDown = _widget;
}

void Neuron::Interaction::CloseDropDown() noexcept
{
  m_dropDown = NO_WIDGET;
}

void Neuron::Interaction::Stroke(WidgetId _widget, FieldStroke _stroke)
{
  if (_widget != m_strokesFor)
  {
    m_strokesFor = _widget;
    m_strokes.clear();
  }
  m_strokes.push_back(_stroke);
}

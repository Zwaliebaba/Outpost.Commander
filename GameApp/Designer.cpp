#include "pch.h"
#include "Designer.h"

#include <algorithm>

namespace
{
template <typename View, typename IdType> const View* Find(const std::vector<View>& _views, IdType _id) noexcept
{
  const auto found = std::ranges::find(_views, _id, &View::id);
  return found != _views.end() ? &*found : nullptr;
}

// The pick, if the snapshot lists it as available; otherwise the first available component, or none.
template <typename View, typename IdType> IdType Available(const std::vector<View>& _views, IdType _picked) noexcept
{
  if (const View* view = Find(_views, _picked); view != nullptr && view->available)
    return _picked;
  const auto first = std::ranges::find_if(_views, [](const View& _view) { return _view.available; });
  return first != _views.end() ? first->id : IdType{};
}
} // namespace

void Outpost::Designer::Update(const Snapshot& _newest)
{
  m_hull = Available(_newest.hulls, m_hull);
  m_drive = Available(_newest.drives, m_drive);
  m_weapon = Available(_newest.weapons, m_weapon);
}

const Outpost::DesignView* Outpost::Designer::Match(const Snapshot& _newest) const noexcept
{
  const auto found = std::ranges::find_if(_newest.designs, [this](const DesignView& _design)
                                          { return _design.hull == m_hull && _design.drive == m_drive && _design.weapon == m_weapon; });
  return found != _newest.designs.end() ? &*found : nullptr;
}

std::string Outpost::Designer::Name(const Snapshot& _newest) const
{
  if (m_typed.has_value())
    return *m_typed;
  if (const DesignView* match = Match(_newest))
    return match->nameUtf8;
  const HullView* hull = Find(_newest.hulls, m_hull);
  const DriveView* drive = Find(_newest.drives, m_drive);
  const WeaponView* weapon = Find(_newest.weapons, m_weapon);
  if (hull == nullptr || drive == nullptr || weapon == nullptr)
    return {};
  return std::format("{}+{}+{}", hull->nameUtf8, drive->nameUtf8, weapon->nameUtf8);
}

std::optional<Outpost::DesignStats> Outpost::Designer::Stats(const Snapshot& _newest) const
{
  const HullView* hull = Find(_newest.hulls, m_hull);
  const DriveView* drive = Find(_newest.drives, m_drive);
  const WeaponView* weapon = Find(_newest.weapons, m_weapon);
  if (hull == nullptr || drive == nullptr || weapon == nullptr)
    return std::nullopt;
  return DesignStatsOf(*hull, *drive, *weapon);
}

std::optional<Outpost::SaveDesignCommand> Outpost::Designer::SaveCommand(const Snapshot& _newest) const
{
  const std::string name = Name(_newest);
  if (!IsValidDesignName(name) || !Stats(_newest).has_value())
    return std::nullopt;
  if (const DesignView* match = Match(_newest))
  {
    if (match->nameUtf8 == name)
      return std::nullopt;
    return SaveDesignCommand{.design = match->id, .nameUtf8 = name, .hull = m_hull, .drive = m_drive, .weapon = m_weapon};
  }
  const bool available =
    Find(_newest.hulls, m_hull)->available && Find(_newest.drives, m_drive)->available && Find(_newest.weapons, m_weapon)->available;
  if (!available)
    return std::nullopt;
  return SaveDesignCommand{.design = {}, .nameUtf8 = name, .hull = m_hull, .drive = m_drive, .weapon = m_weapon};
}

void Outpost::Designer::BeginEditing(const Snapshot& _newest)
{
  if (!m_typed.has_value())
    m_typed = Name(_newest);
  m_editing = true;
}

std::optional<Outpost::SaveDesignCommand> Outpost::Designer::Edit(const Neuron::InputEvent& _event, const Snapshot& _newest)
{
  if (!m_editing)
    return std::nullopt;
  std::string& typed = m_typed.has_value() ? *m_typed : m_typed.emplace();
  if (_event.kind == Neuron::InputEventKind::Character)
  {
    // Only what the HUD's font holds (ADR-015); control characters come as their keys below.
    if (_event.character >= ' ' && _event.character <= '~' && typed.size() < DESIGN_NAME_LIMIT)
      typed += static_cast<char>(_event.character);
    return std::nullopt;
  }
  if (_event.kind != Neuron::InputEventKind::KeyDown)
    return std::nullopt;
  if (_event.key == VK_BACK && !typed.empty())
  {
    typed.pop_back();
  }
  else if (_event.key == VK_ESCAPE)
  {
    m_typed.reset();
    m_editing = false;
  }
  else if (_event.key == VK_RETURN)
  {
    m_editing = false;
    std::optional<SaveDesignCommand> save = SaveCommand(_newest);
    if (save.has_value())
      m_typed.reset();
    return save;
  }
  return std::nullopt;
}

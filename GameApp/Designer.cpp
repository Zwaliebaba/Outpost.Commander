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
  const EntityView* target = Target(_newest);
  m_target = target != nullptr ? target->id : EntityId{};
}

std::vector<const Outpost::EntityView*> Outpost::Designer::Shipyards(const Snapshot& _newest)
{
  std::vector<const EntityView*> yards;
  for (const EntityView& entity : _newest.entities)
  {
    if (entity.kind == EntityKind::Structure && entity.structure == StructureKind::Shipyard && entity.owner == _newest.player &&
        !entity.remembered && entity.builtPermille >= PERMILLE)
      yards.push_back(&entity);
  }
  std::ranges::sort(yards, {}, &EntityView::shipyardNumber);
  return yards;
}

const Outpost::EntityView* Outpost::Designer::Target(const Snapshot& _newest) const
{
  const std::vector<const EntityView*> yards = Shipyards(_newest);
  if (yards.empty())
    return nullptr;
  const auto aimed = std::ranges::find(yards, m_target, &EntityView::id);
  return aimed != yards.end() ? *aimed : yards.front();
}

void Outpost::Designer::SetTarget(EntityId _shipyard, const Snapshot& _newest)
{
  const std::vector<const EntityView*> yards = Shipyards(_newest);
  if (std::ranges::find(yards, _shipyard, &EntityView::id) != yards.end())
    m_target = _shipyard;
}

void Outpost::Designer::StepTarget(int _step, const Snapshot& _newest)
{
  const std::vector<const EntityView*> yards = Shipyards(_newest);
  if (yards.empty())
    return;
  const auto aimed = std::ranges::find(yards, m_target, &EntityView::id);
  const auto count = static_cast<std::ptrdiff_t>(yards.size());
  const std::ptrdiff_t from = aimed != yards.end() ? aimed - yards.begin() : 0;
  m_target = yards[static_cast<std::size_t>((((from + _step) % count) + count) % count)]->id;
}

std::uint32_t Outpost::Designer::Count(const Snapshot& _newest) const
{
  const EntityView* target = Target(_newest);
  const auto free = target != nullptr ? static_cast<std::uint32_t>(QUEUE_LIMIT - std::min(target->queue.size(), QUEUE_LIMIT)) : 0u;
  return std::clamp(m_count, 1u, std::max(free, 1u));
}

void Outpost::Designer::StepCount(int _step, const Snapshot& _newest)
{
  const auto stepped = static_cast<std::int64_t>(Count(_newest)) + _step;
  m_count = static_cast<std::uint32_t>(std::max<std::int64_t>(stepped, 1));
  m_count = Count(_newest);
}

void Outpost::Designer::Load(const DesignView& _design) noexcept
{
  m_hull = _design.hull;
  m_drive = _design.drive;
  m_weapon = _design.weapon;
  m_typed.reset();
  m_editing = false;
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

std::optional<Outpost::SaveDesignCommand> Outpost::Designer::SaveAndQueue(EntityId _producer, const Snapshot& _newest, std::uint32_t _count)
{
  std::optional<SaveDesignCommand> save = SaveCommand(_newest);
  if (!save.has_value() || save->design.IsValid())
    return std::nullopt;
  const DesignComponents picked = Picked();
  const bool saving = std::ranges::find(m_waiting, picked, &WaitingQueue::components) != m_waiting.end();
  for (std::uint32_t i = 0; i < std::max(_count, 1u); ++i)
    m_waiting.push_back({.producer = _producer, .components = picked, .savedTick = _newest.tick});
  if (saving)
    return std::nullopt;
  return save;
}

std::vector<Outpost::QueueShipCommand> Outpost::Designer::TakeQueueCommands(const Snapshot& _newest)
{
  std::vector<QueueShipCommand> queues;
  std::erase_if(m_waiting,
                [&](const WaitingQueue& _waiting)
                {
                  const auto saved =
                    std::ranges::find_if(_newest.designs, [&_waiting](const DesignView& _design)
                                         { return DesignComponents{_design.hull, _design.drive, _design.weapon} == _waiting.components; });
                  if (saved != _newest.designs.end())
                  {
                    queues.push_back({.producer = _waiting.producer, .design = saved->id});
                    return true;
                  }
                  return _newest.tick > _waiting.savedTick + SAVE_WAIT_TICKS;
                });
  return queues;
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

#include "pch.h"
#include "Tuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
using Neuron::JsonValue;

[[noreturn]] void Fail(std::string_view _path, std::string_view _message)
{
  throw Neuron::Exception(std::format("Tuning: {}: {}", _path, _message));
}

std::string ElementPath(std::string_view _path, size_t _index)
{
  return std::format("{}[{}]", _path, _index);
}

std::int32_t ReadInteger(const JsonValue& _value, std::string_view _path, std::int32_t _minimum)
{
  if (!_value.IsNumber())
    Fail(_path, "expected a number");
  const double number = _value.AsNumber();
  if (number != std::trunc(number))
    Fail(_path, std::format("expected a whole number, found {}", number));
  if (number < _minimum)
    Fail(_path, std::format("expected at least {}, found {}", _minimum, number));
  if (number > std::numeric_limits<std::int32_t>::max())
    Fail(_path, std::format("{} is too large", number));
  return static_cast<std::int32_t>(number);
}

enum class Bound : std::uint8_t
{
  NotNegative,
  Positive
};

double ReadNumber(const JsonValue& _value, std::string_view _path, Bound _bound)
{
  if (!_value.IsNumber())
    Fail(_path, "expected a number");
  const double number = _value.AsNumber();
  if (_bound == Bound::Positive && number <= 0.0)
    Fail(_path, std::format("expected more than 0, found {}", number));
  if (_bound == Bound::NotNegative && number < 0.0)
    Fail(_path, std::format("expected at least 0, found {}", number));
  return number;
}

template <typename IdType> IdType ReadId(const JsonValue& _value, std::string_view _path)
{
  return IdType{static_cast<std::uint32_t>(ReadInteger(_value, _path, 1))};
}

const JsonValue::Array& ReadArray(const JsonValue& _value, std::string_view _path)
{
  if (!_value.IsArray())
    Fail(_path, "expected an array");
  return _value.AsArray();
}

// The members of one object, read by name. Finish() rejects any member nothing asked for.
class ObjectReader
{
public:
  ObjectReader(const JsonValue& _value, std::string _path)
    : m_path(std::move(_path))
  {
    if (!_value.IsObject())
      Fail(Where(), "expected an object");
    m_value = &_value;
  }

  [[nodiscard]] const std::string& Path() const noexcept
  {
    return m_path;
  }

  // The path, or "the file" for the document itself.
  [[nodiscard]] std::string Where() const
  {
    return m_path.empty() ? std::string("the file") : m_path;
  }

  [[nodiscard]] std::string PathOf(std::string_view _name) const
  {
    return m_path.empty() ? std::string(_name) : std::format("{}.{}", m_path, _name);
  }

  [[nodiscard]] const JsonValue* Optional(std::string_view _name)
  {
    m_read.push_back(_name);
    return m_value->Find(_name);
  }

  [[nodiscard]] const JsonValue& Required(std::string_view _name)
  {
    const JsonValue* value = Optional(_name);
    if (value == nullptr)
      Fail(Where(), std::format("has no \"{}\"", _name));
    return *value;
  }

  [[nodiscard]] std::int32_t Integer(std::string_view _name, std::int32_t _minimum)
  {
    return ReadInteger(Required(_name), PathOf(_name), _minimum);
  }

  [[nodiscard]] double Number(std::string_view _name, Bound _bound)
  {
    return ReadNumber(Required(_name), PathOf(_name), _bound);
  }

  [[nodiscard]] std::string String(std::string_view _name)
  {
    const JsonValue& value = Required(_name);
    if (!value.IsString())
      Fail(PathOf(_name), "expected a string");
    return value.AsString();
  }

  template <typename IdType> [[nodiscard]] IdType Identifier(std::string_view _name)
  {
    return ReadId<IdType>(Required(_name), PathOf(_name));
  }

  void Finish() const
  {
    for (const Neuron::JsonMember& member : m_value->AsObject())
    {
      if (std::ranges::find(m_read, member.name) == m_read.end())
        Fail(PathOf(member.name), "is not a member the game knows");
    }
  }

private:
  const JsonValue* m_value = nullptr;
  std::string m_path;
  std::vector<std::string_view> m_read;
};

// Reads each element of the named array with _readElement(reader), which is handed an ObjectReader for the element.
template <typename Element, typename Fn> std::vector<Element> ReadList(ObjectReader& _parent, std::string_view _name, Fn _readElement)
{
  const std::string path = _parent.PathOf(_name);
  const JsonValue::Array& elements = ReadArray(_parent.Required(_name), path);
  std::vector<Element> list;
  list.reserve(elements.size());
  for (size_t i = 0; i < elements.size(); ++i)
  {
    ObjectReader reader(elements[i], ElementPath(path, i));
    list.push_back(_readElement(reader));
    reader.Finish();
  }
  return list;
}

// Identifiers are unique within their list.
template <typename Element> void CheckUniqueIds(const std::vector<Element>& _list, std::string_view _path)
{
  for (size_t i = 0; i < _list.size(); ++i)
  {
    for (size_t j = 0; j < i; ++j)
    {
      if (_list[i].id == _list[j].id)
        Fail(std::format("{}.id", ElementPath(_path, i)),
             std::format("{} is already used by {}", _list[i].id.value, ElementPath(_path, j)));
    }
  }
}

template <typename Element, typename IdType>
void CheckExists(const std::vector<Element>& _list, IdType _id, std::string_view _path, std::string_view _listName)
{
  if (std::ranges::none_of(_list, [_id](const Element& _element) { return _element.id == _id; }))
    Fail(_path, std::format("names {} {}, which does not exist", _listName, _id.value));
}

Outpost::RulesTuning ReadRules(ObjectReader& _reader)
{
  Outpost::RulesTuning rules;
  rules.tickHz = _reader.Integer("tickHz", 1);
  rules.startingOre = _reader.Integer("startingOre", 0);
  rules.miningRigOrePerSecondHome = _reader.Number("miningRigOrePerSecondHome", Bound::NotNegative);
  rules.miningRigOrePerSecondContested = _reader.Number("miningRigOrePerSecondContested", Bound::NotNegative);
  rules.aiReviewIntervalSeconds = _reader.Number("aiReviewIntervalSeconds", Bound::Positive);
  return rules;
}

Outpost::HullTuning ReadHull(ObjectReader& _reader)
{
  Outpost::HullTuning hull;
  hull.id = _reader.Identifier<Outpost::HullId>("id");
  hull.name = _reader.String("name");
  hull.hitPoints = _reader.Integer("hitPoints", 1);
  hull.armor = _reader.Integer("armor", 0);
  hull.speedMetersPerSecond = _reader.Number("speedMetersPerSecond", Bound::Positive);
  hull.cost = _reader.Integer("cost", 0);
  hull.buildSeconds = _reader.Number("buildSeconds", Bound::Positive);
  return hull;
}

Outpost::DriveTuning ReadDrive(ObjectReader& _reader)
{
  Outpost::DriveTuning drive;
  drive.id = _reader.Identifier<Outpost::DriveId>("id");
  drive.name = _reader.String("name");
  drive.speedFactor = _reader.Number("speedFactor", Bound::Positive);
  drive.hitPointsFactor = _reader.Number("hitPointsFactor", Bound::Positive);
  drive.cost = _reader.Integer("cost", 0);
  return drive;
}

Outpost::WeaponTuning ReadWeapon(ObjectReader& _reader)
{
  Outpost::WeaponTuning weapon;
  weapon.id = _reader.Identifier<Outpost::WeaponId>("id");
  weapon.name = _reader.String("name");
  weapon.damage = _reader.Integer("damage", 1);
  weapon.fireIntervalSeconds = _reader.Number("fireIntervalSeconds", Bound::Positive);
  weapon.rangeMeters = _reader.Number("rangeMeters", Bound::Positive);
  weapon.splashRadiusMeters = _reader.Number("splashRadiusMeters", Bound::NotNegative);
  weapon.cost = _reader.Integer("cost", 0);
  return weapon;
}

Outpost::StructureWeaponTuning ReadStructureWeapon(ObjectReader& _reader)
{
  Outpost::StructureWeaponTuning weapon;
  weapon.id = _reader.Identifier<Outpost::StructureWeaponId>("id");
  weapon.name = _reader.String("name");
  weapon.damage = _reader.Integer("damage", 1);
  weapon.fireIntervalSeconds = _reader.Number("fireIntervalSeconds", Bound::Positive);
  weapon.rangeMeters = _reader.Number("rangeMeters", Bound::Positive);
  return weapon;
}

// The file spells a kind as its enumerator.
constexpr std::array<std::pair<std::string_view, Outpost::StructureKind>, 5> STRUCTURE_KINDS = {{
  {"CommandStation", Outpost::StructureKind::CommandStation},
  {"Shipyard", Outpost::StructureKind::Shipyard},
  {"ResearchLab", Outpost::StructureKind::ResearchLab},
  {"MiningRig", Outpost::StructureKind::MiningRig},
  {"DefensePlatform", Outpost::StructureKind::DefensePlatform},
}};

Outpost::StructureTuning ReadStructure(ObjectReader& _reader)
{
  Outpost::StructureTuning structure;
  const std::string kind = _reader.String("kind");
  const auto found = std::ranges::find(STRUCTURE_KINDS, kind, &std::pair<std::string_view, Outpost::StructureKind>::first);
  if (found == STRUCTURE_KINDS.end())
    Fail(_reader.PathOf("kind"), std::format("\"{}\" is not a structure kind", kind));
  structure.kind = found->second;
  structure.name = _reader.String("name");
  structure.hitPoints = _reader.Integer("hitPoints", 1);
  structure.armor = _reader.Integer("armor", 0);

  const JsonValue* cost = _reader.Optional("cost");
  const JsonValue* build = _reader.Optional("buildConstructorSeconds");
  if ((cost == nullptr) != (build == nullptr))
    Fail(_reader.PathOf(cost == nullptr ? "buildConstructorSeconds" : "cost"), "needs \"cost\" and \"buildConstructorSeconds\" together");
  if (cost != nullptr)
  {
    structure.cost = ReadInteger(*cost, _reader.PathOf("cost"), 0);
    structure.buildConstructorSeconds = ReadNumber(*build, _reader.PathOf("buildConstructorSeconds"), Bound::Positive);
  }

  if (const JsonValue* weapon = _reader.Optional("structureWeapon"))
    structure.structureWeapon = ReadId<Outpost::StructureWeaponId>(*weapon, _reader.PathOf("structureWeapon"));
  return structure;
}

constexpr std::array<std::pair<std::string_view, Outpost::UpgradeTarget>, 4> UPGRADE_TARGETS = {{
  {"miningRig", Outpost::UpgradeTarget::MiningRig},
  {"allHulls", Outpost::UpgradeTarget::AllHulls},
  {"weapon", Outpost::UpgradeTarget::Weapon},
  {"shipyards", Outpost::UpgradeTarget::Shipyards},
}};

constexpr std::array<std::pair<std::string_view, Outpost::UpgradeStat>, 4> UPGRADE_STATS = {{
  {"income", Outpost::UpgradeStat::Income},
  {"hitPoints", Outpost::UpgradeStat::HitPoints},
  {"fireRate", Outpost::UpgradeStat::FireRate},
  {"buildSpeed", Outpost::UpgradeStat::BuildSpeed},
}};

template <typename Value, size_t Count>
Value ReadName(ObjectReader& _reader, std::string_view _name, const std::array<std::pair<std::string_view, Value>, Count>& _names)
{
  const std::string text = _reader.String(_name);
  const auto found = std::ranges::find(_names, text, &std::pair<std::string_view, Value>::first);
  if (found == _names.end())
    Fail(_reader.PathOf(_name), std::format("\"{}\" is not one the game knows", text));
  return found->second;
}

Outpost::ResearchEffect ReadEffect(ObjectReader& _reader)
{
  if (_reader.Optional("upgrade") != nullptr)
  {
    Outpost::UpgradeEffect upgrade;
    upgrade.target = ReadName(_reader, "upgrade", UPGRADE_TARGETS);
    if (upgrade.target == Outpost::UpgradeTarget::Weapon)
      upgrade.weapon = _reader.Identifier<Outpost::WeaponId>("weapon");
    upgrade.stat = ReadName(_reader, "stat", UPGRADE_STATS);
    upgrade.percent = _reader.Integer("percent", 1);
    return upgrade;
  }
  if (_reader.Optional("unlockHull") != nullptr)
    return _reader.Identifier<Outpost::HullId>("unlockHull");
  if (_reader.Optional("unlockDrive") != nullptr)
    return _reader.Identifier<Outpost::DriveId>("unlockDrive");
  if (_reader.Optional("unlockWeapon") != nullptr)
    return _reader.Identifier<Outpost::WeaponId>("unlockWeapon");
  Fail(_reader.Path(), "needs \"upgrade\", \"unlockHull\", \"unlockDrive\" or \"unlockWeapon\"");
}

Outpost::ResearchTopicTuning ReadResearchTopic(ObjectReader& _reader)
{
  Outpost::ResearchTopicTuning topic;
  topic.id = _reader.Identifier<Outpost::ResearchTopicId>("id");
  topic.name = _reader.String("name");
  topic.cost = _reader.Integer("cost", 0);
  topic.researchSeconds = _reader.Number("researchSeconds", Bound::Positive);

  const std::string requiresPath = _reader.PathOf("requires");
  const JsonValue::Array& prerequisites = ReadArray(_reader.Required("requires"), requiresPath);
  for (size_t i = 0; i < prerequisites.size(); ++i)
    topic.prerequisites.push_back(ReadId<Outpost::ResearchTopicId>(prerequisites[i], ElementPath(requiresPath, i)));

  ObjectReader effect(_reader.Required("effect"), _reader.PathOf("effect"));
  topic.effect = ReadEffect(effect);
  effect.Finish();
  return topic;
}

void CheckReferences(const Outpost::Tuning& _tuning)
{
  std::array<int, STRUCTURE_KINDS.size()> kindCount{};
  for (size_t i = 0; i < _tuning.structures.size(); ++i)
  {
    const Outpost::StructureTuning& structure = _tuning.structures[i];
    const std::string path = ElementPath("structures", i);
    if (++kindCount[Neuron::I(structure.kind)] > 1)
      Fail(std::format("{}.kind", path), "a structure kind appears twice");
    if (structure.structureWeapon.IsValid())
      CheckExists(_tuning.structureWeapons, structure.structureWeapon, std::format("{}.structureWeapon", path), "structure weapon");
  }
  for (const auto& [name, kind] : STRUCTURE_KINDS)
  {
    if (kindCount[Neuron::I(kind)] == 0)
      Fail("structures", std::format("has no {}", name));
  }

  for (size_t i = 0; i < _tuning.research.size(); ++i)
  {
    const Outpost::ResearchTopicTuning& topic = _tuning.research[i];
    const std::string path = ElementPath("research", i);
    for (size_t j = 0; j < topic.prerequisites.size(); ++j)
      CheckExists(_tuning.research, topic.prerequisites[j], ElementPath(std::format("{}.requires", path), j), "research topic");

    const std::string effectPath = std::format("{}.effect", path);
    if (const auto* upgrade = std::get_if<Outpost::UpgradeEffect>(&topic.effect); upgrade != nullptr && upgrade->weapon.IsValid())
      CheckExists(_tuning.weapons, upgrade->weapon, std::format("{}.weapon", effectPath), "weapon");
    else if (const auto* hull = std::get_if<Outpost::HullId>(&topic.effect))
      CheckExists(_tuning.hulls, *hull, std::format("{}.unlockHull", effectPath), "hull");
    else if (const auto* drive = std::get_if<Outpost::DriveId>(&topic.effect))
      CheckExists(_tuning.drives, *drive, std::format("{}.unlockDrive", effectPath), "drive");
    else if (const auto* weapon = std::get_if<Outpost::WeaponId>(&topic.effect))
      CheckExists(_tuning.weapons, *weapon, std::format("{}.unlockWeapon", effectPath), "weapon");
  }
}

// Every topic must be reachable: repeatedly mark the topics whose prerequisites are all marked, and any left over
// require themselves through a cycle.
void CheckResearchIsAcyclic(const std::vector<Outpost::ResearchTopicTuning>& _research)
{
  std::vector<Outpost::ResearchTopicId> reachable;
  bool progressed = true;
  while (progressed)
  {
    progressed = false;
    for (const Outpost::ResearchTopicTuning& topic : _research)
    {
      if (std::ranges::find(reachable, topic.id) != reachable.end())
        continue;
      if (std::ranges::all_of(topic.prerequisites,
                              [&reachable](Outpost::ResearchTopicId _id) { return std::ranges::find(reachable, _id) != reachable.end(); }))
      {
        reachable.push_back(topic.id);
        progressed = true;
      }
    }
  }
  for (size_t i = 0; i < _research.size(); ++i)
  {
    if (std::ranges::find(reachable, _research[i].id) == reachable.end())
      Fail(std::format("{}.requires", ElementPath("research", i)), "the topic requires itself through a cycle");
  }
}
} // namespace

Outpost::Tuning Outpost::LoadTuning(std::string_view _json)
{
  const JsonValue document = Neuron::ParseJson(_json);
  ObjectReader root(document, "");

  Tuning tuning;
  ObjectReader rules(root.Required("rules"), "rules");
  tuning.rules = ReadRules(rules);
  rules.Finish();

  tuning.hulls = ReadList<HullTuning>(root, "hulls", ReadHull);
  tuning.drives = ReadList<DriveTuning>(root, "drives", ReadDrive);
  tuning.weapons = ReadList<WeaponTuning>(root, "weapons", ReadWeapon);
  tuning.structureWeapons = ReadList<StructureWeaponTuning>(root, "structureWeapons", ReadStructureWeapon);
  tuning.structures = ReadList<StructureTuning>(root, "structures", ReadStructure);
  tuning.research = ReadList<ResearchTopicTuning>(root, "research", ReadResearchTopic);
  root.Finish();

  CheckUniqueIds(tuning.hulls, "hulls");
  CheckUniqueIds(tuning.drives, "drives");
  CheckUniqueIds(tuning.weapons, "weapons");
  CheckUniqueIds(tuning.structureWeapons, "structureWeapons");
  CheckUniqueIds(tuning.research, "research");
  CheckReferences(tuning);
  CheckResearchIsAcyclic(tuning.research);
  return tuning;
}

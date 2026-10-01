#include "pch.h"
#include "Tuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
using Neuron::JsonValue;

using Neuron::JsonBound;
using ObjectReader = Neuron::JsonObjectReader;

template <typename IdType> IdType ReadId(const JsonValue& _value, std::string_view _path)
{
  return IdType{static_cast<std::uint32_t>(Neuron::ReadJsonInteger(_value, _path, 1))};
}

// Identifiers are unique within their list.
template <typename Element> void CheckUniqueIds(const std::vector<Element>& _list, std::string_view _path)
{
  for (size_t i = 0; i < _list.size(); ++i)
  {
    for (size_t j = 0; j < i; ++j)
    {
      if (_list[i].id == _list[j].id)
      {
        Neuron::JsonFail(std::format("{}.id", Neuron::JsonElementPath(_path, i)),
                         std::format("{} is already used by {}", _list[i].id.value, Neuron::JsonElementPath(_path, j)));
      }
    }
  }
}

template <typename Element, typename IdType> void CheckExists(const std::vector<Element>& _list, IdType _id, std::string_view _path,
                                                              std::string_view _listName)
{
  if (std::ranges::none_of(_list, [_id](const Element& _element)
  {
    return _element.id == _id;
  }))
    Neuron::JsonFail(_path, std::format("names {} {}, which does not exist", _listName, _id.value));
}

Outpost::RulesTuning ReadRules(ObjectReader& _reader)
{
  Outpost::RulesTuning rules;
  rules.tickHz = _reader.Integer("tickHz", 1);
  rules.startingOre = _reader.Integer("startingOre", 0);
  rules.startingConstructors = _reader.Integer("startingConstructors", 0);
  rules.miningRigOrePerSecondHome = _reader.Number("miningRigOrePerSecondHome", JsonBound::NotNegative);
  rules.miningRigOrePerSecondContested = _reader.Number("miningRigOrePerSecondContested", JsonBound::NotNegative);
  rules.aiReviewIntervalSeconds = _reader.Number("aiReviewIntervalSeconds", JsonBound::Positive);
  return rules;
}

Outpost::HullTuning ReadHull(ObjectReader& _reader)
{
  Outpost::HullTuning hull;
  hull.id = _reader.Identifier<Outpost::HullId>("id");
  hull.name = _reader.String("name");
  hull.hitPoints = _reader.Integer("hitPoints", 1);
  hull.armor = _reader.Integer("armor", 0);
  hull.speedMetersPerSecond = _reader.Number("speedMetersPerSecond", JsonBound::Positive);
  hull.cost = _reader.Integer("cost", 0);
  hull.buildSeconds = _reader.Number("buildSeconds", JsonBound::Positive);
  hull.footprintRadiusMeters = _reader.Number("footprintRadiusMeters", JsonBound::Positive);
  hull.turnRateDegreesPerSecond = _reader.Number("turnRateDegreesPerSecond", JsonBound::Positive);
  return hull;
}

Outpost::DriveTuning ReadDrive(ObjectReader& _reader)
{
  Outpost::DriveTuning drive;
  drive.id = _reader.Identifier<Outpost::DriveId>("id");
  drive.name = _reader.String("name");
  drive.speedFactor = _reader.Number("speedFactor", JsonBound::Positive);
  drive.hitPointsFactor = _reader.Number("hitPointsFactor", JsonBound::Positive);
  drive.turnRateFactor = _reader.Number("turnRateFactor", JsonBound::Positive);
  drive.cost = _reader.Integer("cost", 0);
  return drive;
}

Outpost::WeaponTuning ReadWeapon(ObjectReader& _reader)
{
  Outpost::WeaponTuning weapon;
  weapon.id = _reader.Identifier<Outpost::WeaponId>("id");
  weapon.name = _reader.String("name");
  weapon.damage = _reader.Integer("damage", 1);
  weapon.fireIntervalSeconds = _reader.Number("fireIntervalSeconds", JsonBound::Positive);
  weapon.rangeMeters = _reader.Number("rangeMeters", JsonBound::Positive);
  weapon.splashRadiusMeters = _reader.Number("splashRadiusMeters", JsonBound::NotNegative);
  weapon.cost = _reader.Integer("cost", 0);
  return weapon;
}

Outpost::ConstructorTuning ReadConstructor(ObjectReader& _reader)
{
  Outpost::ConstructorTuning constructor;
  constructor.hitPoints = _reader.Integer("hitPoints", 1);
  constructor.armor = _reader.Integer("armor", 0);
  constructor.speedMetersPerSecond = _reader.Number("speedMetersPerSecond", JsonBound::Positive);
  constructor.cost = _reader.Integer("cost", 0);
  constructor.buildSeconds = _reader.Number("buildSeconds", JsonBound::Positive);
  constructor.footprintRadiusMeters = _reader.Number("footprintRadiusMeters", JsonBound::Positive);
  constructor.turnRateDegreesPerSecond = _reader.Number("turnRateDegreesPerSecond", JsonBound::Positive);
  constructor.extraConstructorBuildShare = _reader.Number("extraConstructorBuildShare", JsonBound::NotNegative);
  constructor.repairPercentPerSecond = _reader.Number("repairPercentPerSecond", JsonBound::Positive);
  return constructor;
}

Outpost::StructureWeaponTuning ReadStructureWeapon(ObjectReader& _reader)
{
  Outpost::StructureWeaponTuning weapon;
  weapon.id = _reader.Identifier<Outpost::StructureWeaponId>("id");
  weapon.name = _reader.String("name");
  weapon.damage = _reader.Integer("damage", 1);
  weapon.fireIntervalSeconds = _reader.Number("fireIntervalSeconds", JsonBound::Positive);
  weapon.rangeMeters = _reader.Number("rangeMeters", JsonBound::Positive);
  return weapon;
}

// The file spells a kind as its enumerator.
constexpr std::array<std::pair<std::string_view, Outpost::StructureKind>, 5> STRUCTURE_KINDS = {
  {{"CommandStation", Outpost::StructureKind::CommandStation}, {"Shipyard", Outpost::StructureKind::Shipyard},
   {"ResearchLab", Outpost::StructureKind::ResearchLab}, {"MiningRig", Outpost::StructureKind::MiningRig},
   {"DefensePlatform", Outpost::StructureKind::DefensePlatform},}};

Outpost::StructureTuning ReadStructure(ObjectReader& _reader)
{
  Outpost::StructureTuning structure;
  const std::string kind = _reader.String("kind");
  const auto found = std::ranges::find(STRUCTURE_KINDS, kind, &std::pair<std::string_view, Outpost::StructureKind>::first);
  if (found == STRUCTURE_KINDS.end())
    Neuron::JsonFail(_reader.PathOf("kind"), std::format("\"{}\" is not a structure kind", kind));
  structure.kind = found->second;
  structure.name = _reader.String("name");
  structure.hitPoints = _reader.Integer("hitPoints", 1);
  structure.armor = _reader.Integer("armor", 0);
  structure.footprintRadiusMeters = _reader.Number("footprintRadiusMeters", JsonBound::Positive);

  const JsonValue* cost = _reader.Optional("cost");
  const JsonValue* build = _reader.Optional("buildConstructorSeconds");
  if ((cost == nullptr) != (build == nullptr))
  {
    Neuron::JsonFail(_reader.PathOf(cost == nullptr ? "buildConstructorSeconds" : "cost"),
                     "needs \"cost\" and \"buildConstructorSeconds\" together");
  }
  if (cost != nullptr)
  {
    structure.cost = Neuron::ReadJsonInteger(*cost, _reader.PathOf("cost"), 0);
    structure.buildConstructorSeconds = Neuron::ReadJsonNumber(*build, _reader.PathOf("buildConstructorSeconds"), JsonBound::Positive);
  }

  if (const JsonValue* weapon = _reader.Optional("structureWeapon"))
    structure.structureWeapon = ReadId<Outpost::StructureWeaponId>(*weapon, _reader.PathOf("structureWeapon"));
  return structure;
}

constexpr std::array<std::pair<std::string_view, Outpost::UpgradeTarget>, 4> UPGRADE_TARGETS = {
  {{"miningRig", Outpost::UpgradeTarget::MiningRig}, {"allHulls", Outpost::UpgradeTarget::AllHulls},
   {"weapon", Outpost::UpgradeTarget::Weapon}, {"shipyards", Outpost::UpgradeTarget::Shipyards},}};

constexpr std::array<std::pair<std::string_view, Outpost::UpgradeStat>, 4> UPGRADE_STATS = {
  {{"income", Outpost::UpgradeStat::Income}, {"hitPoints", Outpost::UpgradeStat::HitPoints}, {"fireRate", Outpost::UpgradeStat::FireRate},
   {"buildSpeed", Outpost::UpgradeStat::BuildSpeed},}};

template <typename Value, size_t Count> Value ReadName(ObjectReader& _reader, std::string_view _name,
                                                       const std::array<std::pair<std::string_view, Value>, Count>& _names)
{
  const std::string text = _reader.String(_name);
  const auto found = std::ranges::find(_names, text, &std::pair<std::string_view, Value>::first);
  if (found == _names.end())
    Neuron::JsonFail(_reader.PathOf(_name), std::format("\"{}\" is not one the game knows", text));
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
  Neuron::JsonFail(_reader.Path(), "needs \"upgrade\", \"unlockHull\", \"unlockDrive\" or \"unlockWeapon\"");
}

Outpost::ResearchTopicTuning ReadResearchTopic(ObjectReader& _reader)
{
  Outpost::ResearchTopicTuning topic;
  topic.id = _reader.Identifier<Outpost::ResearchTopicId>("id");
  topic.name = _reader.String("name");
  topic.cost = _reader.Integer("cost", 0);
  topic.researchSeconds = _reader.Number("researchSeconds", JsonBound::Positive);

  const std::string requiresPath = _reader.PathOf("requires");
  const JsonValue::Array& prerequisites = Neuron::ReadJsonArray(_reader.Required("requires"), requiresPath);
  for (size_t i = 0; i < prerequisites.size(); ++i)
    topic.prerequisites.push_back(ReadId<Outpost::ResearchTopicId>(prerequisites[i], Neuron::JsonElementPath(requiresPath, i)));

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
    const std::string path = Neuron::JsonElementPath("structures", i);
    if (++kindCount[Neuron::I(structure.kind)] > 1)
      Neuron::JsonFail(std::format("{}.kind", path), "a structure kind appears twice");
    if (structure.structureWeapon.IsValid())
      CheckExists(_tuning.structureWeapons, structure.structureWeapon, std::format("{}.structureWeapon", path), "structure weapon");
  }
  for (const auto& [name, kind] : STRUCTURE_KINDS)
  {
    if (kindCount[Neuron::I(kind)] == 0)
      Neuron::JsonFail("structures", std::format("has no {}", name));
  }

  for (size_t i = 0; i < _tuning.research.size(); ++i)
  {
    const Outpost::ResearchTopicTuning& topic = _tuning.research[i];
    const std::string path = Neuron::JsonElementPath("research", i);
    for (size_t j = 0; j < topic.prerequisites.size(); ++j)
      CheckExists(_tuning.research, topic.prerequisites[j], Neuron::JsonElementPath(std::format("{}.requires", path), j), "research topic");

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
      if (std::ranges::all_of(topic.prerequisites, [&reachable](Outpost::ResearchTopicId _id)
      {
        return std::ranges::find(reachable, _id) != reachable.end();
      }))
      {
        reachable.push_back(topic.id);
        progressed = true;
      }
    }
  }
  for (size_t i = 0; i < _research.size(); ++i)
  {
    if (std::ranges::find(reachable, _research[i].id) == reachable.end())
      Neuron::JsonFail(std::format("{}.requires", Neuron::JsonElementPath("research", i)), "the topic requires itself through a cycle");
  }
}

Outpost::Tuning ReadTuning(std::string_view _json)
{
  const JsonValue document = Neuron::ParseJson(_json);
  ObjectReader root(document, "");

  Outpost::Tuning tuning;
  ObjectReader rules(root.Required("rules"), "rules");
  tuning.rules = ReadRules(rules);
  rules.Finish();

  tuning.hulls = Neuron::ReadJsonList<Outpost::HullTuning>(root, "hulls", ReadHull);
  tuning.drives = Neuron::ReadJsonList<Outpost::DriveTuning>(root, "drives", ReadDrive);
  tuning.weapons = Neuron::ReadJsonList<Outpost::WeaponTuning>(root, "weapons", ReadWeapon);
  ObjectReader constructor(root.Required("constructor"), "constructor");
  tuning.constructor = ReadConstructor(constructor);
  constructor.Finish();
  tuning.structureWeapons = Neuron::ReadJsonList<Outpost::StructureWeaponTuning>(root, "structureWeapons", ReadStructureWeapon);
  tuning.structures = Neuron::ReadJsonList<Outpost::StructureTuning>(root, "structures", ReadStructure);
  tuning.research = Neuron::ReadJsonList<Outpost::ResearchTopicTuning>(root, "research", ReadResearchTopic);
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
} // namespace

Outpost::Tuning Outpost::LoadTuning(std::string_view _json)
{
  try
  {
    return ReadTuning(_json);
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("Tuning: {}", error.what()));
  }
}
#include "pch.h"
#include "RepositoryData.h"

#include <array>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
// A loaded field, to compare with the member of the same name in the file.
struct LoadedField
{
  std::string_view name;
  std::variant<double, std::string> value;
};

std::wstring Widen(std::string_view _text)
{
  return {_text.begin(), _text.end()};
}

// Every loaded field equals the file's member of that name, and the file has no member the loader did not load, besides
// the _checkedElsewhere members the caller compares itself.
void ExpectSame(const Neuron::JsonValue& _json, const std::vector<LoadedField>& _loaded, std::string_view _path,
                size_t _checkedElsewhere = 0)
{
  Assert::AreEqual(_json.AsObject().size(), _loaded.size() + _checkedElsewhere, Widen(std::format("{}: member count", _path)).c_str());
  for (const LoadedField& field : _loaded)
  {
    const std::wstring where = Widen(std::format("{}.{}", _path, field.name));
    const Neuron::JsonValue* member = _json.Find(field.name);
    Assert::IsNotNull(member, where.c_str());
    if (const auto* number = std::get_if<double>(&field.value))
      Assert::IsTrue(member->AsNumber() == *number, where.c_str());
    else
      Assert::IsTrue(member->AsString() == std::get<std::string>(field.value), where.c_str());
  }
}

double Number(std::int64_t _value)
{
  return static_cast<double>(_value);
}

std::string_view KindName(Outpost::StructureKind _kind)
{
  switch (_kind)
  {
  case Outpost::StructureKind::CommandStation:
    return "CommandStation";
  case Outpost::StructureKind::Shipyard:
    return "Shipyard";
  case Outpost::StructureKind::ResearchLab:
    return "ResearchLab";
  case Outpost::StructureKind::MiningRig:
    return "MiningRig";
  case Outpost::StructureKind::DefensePlatform:
    return "DefensePlatform";
  case Outpost::StructureKind::Relay:
    return "Relay";
  }
  return {};
}

std::vector<LoadedField> EffectFields(const Outpost::ResearchEffect& _effect)
{
  if (const auto* upgrade = std::get_if<Outpost::UpgradeEffect>(&_effect))
  {
    constexpr std::array<std::string_view, 9> TARGETS = {"miningRig",       "allHulls", "weapon",       "shipyards", "allStructures",
                                                         "structureWeapon", "allShips", "constructors", "asteroids"};
    constexpr std::array<std::string_view, 7> STATS = {"income", "hitPoints", "fireRate", "buildSpeed", "speed", "buildRate", "oreReserve"};
    std::vector<LoadedField> fields = {{"upgrade", std::string(TARGETS[Neuron::I(upgrade->target)])},
                                       {"stat", std::string(STATS[Neuron::I(upgrade->stat)])},
                                       {"percent", Number(upgrade->percent)}};
    if (upgrade->weapon.IsValid())
      fields.push_back({"weapon", Number(upgrade->weapon.value)});
    if (upgrade->structureWeapon.IsValid())
      fields.push_back({"structureWeapon", Number(upgrade->structureWeapon.value)});
    return fields;
  }
  if (const auto* hull = std::get_if<Outpost::HullId>(&_effect))
    return {{"unlockHull", Number(hull->value)}};
  if (const auto* drive = std::get_if<Outpost::DriveId>(&_effect))
    return {{"unlockDrive", Number(drive->value)}};
  return {{"unlockWeapon", Number(std::get<Outpost::WeaponId>(_effect).value)}};
}

// A small file that loads, for the error cases to break one thing at a time.
constexpr std::string_view MINIMAL_TUNING = R"({
  "rules": { "tickHz": 20, "startingOre": 1000, "startingConstructors": 2, "miningRigOrePerSecondHome": 5,
             "miningRigOrePerSecondNear": 6, "miningRigOrePerSecondContested": 8, "miningRigOrePerSecondRich": 10,
             "exhaustedYieldPercent": 20, "levelHitPointsPercent": 20 },
  "sight": { "weaponMarginMeters": 50, "unarmedMeters": 200, "shotRevealSeconds": 3 },
  "territory": { "cutOffIncomePercent": 50, "suppressionRadiusMeters": 400, "tickets": 1000, "drainIntervalSeconds": 10,
                 "drainTicketsPerNodeDifference": 30 },
  "hulls": [ { "id": 1, "name": "Small", "hitPoints": 220, "armor": 2, "speedMetersPerSecond": 60, "cost": 32, "buildSeconds": 10,
               "footprintRadiusMeters": 8, "turnRateDegreesPerSecond": 180 } ],
  "drives": [ { "id": 1, "name": "Ion", "speedFactor": 1.3, "hitPointsFactor": 0.9, "turnRateFactor": 1.25, "cost": 20 } ],
  "weapons": [ { "id": 1, "name": "Mass Driver", "damage": 14, "fireIntervalSeconds": 0.4, "rangeMeters": 120,
                 "splashRadiusMeters": 0, "cost": 35 } ],
  "modules": [ { "id": 1, "name": "Sensor Array", "sightMeters": 700, "speedFactor": 0.9, "cost": 40 } ],
  "structureWeapons": [ { "id": 1, "name": "Defence gun", "damage": 30, "fireIntervalSeconds": 1.0, "rangeMeters": 250 } ],
  "constructor": { "hitPoints": 300, "armor": 3, "speedMetersPerSecond": 45, "cost": 60, "buildSeconds": 15,
                   "footprintRadiusMeters": 10, "turnRateDegreesPerSecond": 150, "extraConstructorBuildShare": 0.5,
                   "repairPercentPerSecond": 2 },
  "structures": [
    { "kind": "CommandStation", "name": "Command Station", "hitPoints": 5000, "armor": 10, "footprintRadiusMeters": 45,
      "structureWeapon": 1 },
    { "kind": "Shipyard", "name": "Shipyard", "hitPoints": 2500, "armor": 0, "footprintRadiusMeters": 40, "cost": 300,
      "buildConstructorSeconds": 40,
      "levels": [ { "cost": 150, "buildSeconds": 30 } ] },
    { "kind": "ResearchLab", "name": "Research Lab", "hitPoints": 1500, "armor": 0, "footprintRadiusMeters": 30, "cost": 200,
      "buildConstructorSeconds": 30,
      "levels": [ { "cost": 400, "buildSeconds": 60, "opensTier": 2, "requires": [1] },
                  { "cost": 800, "buildSeconds": 120, "researchSlots": 2 } ] },
    { "kind": "MiningRig", "name": "Mining Rig", "hitPoints": 800, "armor": 0, "footprintRadiusMeters": 25, "cost": 50,
      "buildConstructorSeconds": 10 },
    { "kind": "DefensePlatform", "name": "Defence Platform", "hitPoints": 1500, "armor": 10, "footprintRadiusMeters": 20,
      "cost": 150, "buildConstructorSeconds": 20, "structureWeapon": 1 },
    { "kind": "Relay", "name": "Relay", "hitPoints": 3000, "armor": 10, "footprintRadiusMeters": 30, "cost": 200,
      "buildConstructorSeconds": 40 }
  ],
  "research": [
    { "id": 1, "name": "Hull Plating", "tier": 1, "cost": 150, "researchSeconds": 60, "requires": [],
      "effect": { "upgrade": "allHulls", "stat": "hitPoints", "percent": 15 } },
    { "id": 2, "name": "Mass Driver Calibration", "tier": 1, "cost": 150, "researchSeconds": 75, "requires": [1],
      "effect": { "upgrade": "weapon", "weapon": 1, "stat": "fireRate", "percent": 15 } },
    { "id": 3, "name": "Ion Drive", "tier": 1, "cost": 200, "researchSeconds": 90, "requires": [2],
      "effect": { "unlockDrive": 1 } },
    { "id": 4, "name": "Automated Shipyards", "tier": 1, "cost": 200, "researchSeconds": 90, "requires": [1],
      "effect": { "upgrade": "shipyards", "stat": "buildSpeed", "percent": 25 } },
    { "id": 5, "name": "Reinforced Structures", "tier": 2, "cost": 250, "researchSeconds": 90, "requires": [4],
      "effect": { "upgrade": "allStructures", "stat": "hitPoints", "percent": 25 } },
    { "id": 6, "name": "Defence Autoloader", "tier": 2, "cost": 250, "researchSeconds": 90, "requires": [4],
      "effect": { "upgrade": "structureWeapon", "structureWeapon": 1, "stat": "fireRate", "percent": 20 } },
    { "id": 7, "name": "Drive Harmonics", "tier": 2, "cost": 450, "researchSeconds": 120, "requires": [4],
      "effect": { "upgrade": "allShips", "stat": "speed", "percent": 10 } },
    { "id": 8, "name": "Rapid Construction", "tier": 2, "cost": 350, "researchSeconds": 100, "requires": [4],
      "effect": { "upgrade": "constructors", "stat": "buildRate", "percent": 25 } },
    { "id": 9, "name": "Deep Core Survey", "tier": 2, "cost": 300, "researchSeconds": 90, "requires": [4, 1],
      "effect": { "upgrade": "asteroids", "stat": "oreReserve", "percent": 30 } }
  ]
})";

// MINIMAL_TUNING with the one occurrence of _from replaced by _to.
std::string Replace(std::string_view _from, std::string_view _to)
{
  std::string text(MINIMAL_TUNING);
  const size_t at = text.find(_from);
  Assert::IsTrue(at != std::string::npos && text.find(_from, at + 1) == std::string::npos, Widen(_from).c_str());
  text.replace(at, _from.size(), _to);
  return text;
}

// Loading fails, and the message names the place.
void ExpectLoadError(const std::string& _text, std::string_view _where)
{
  try
  {
    (void)Outpost::LoadTuning(_text);
  }
  catch (const Neuron::Exception& error)
  {
    const std::string message = error.what();
    Assert::IsTrue(message.find(_where) != std::string::npos, Widen(std::format("\"{}\" does not name {}", message, _where)).c_str());
    return;
  }
  Assert::Fail(Widen(std::format("loaded, but {} is wrong", _where)).c_str());
}
} // namespace

TEST_CLASS(TuningTests)
{
public:
  // The acceptance of plan task 3.1: the game loads the numbers the file holds, the ones Tools/BattleModel.py reads.
  TEST_METHOD(LoadsEveryNumberInTheRepositoryFile)
  {
    const std::string text = ReadRepositoryTuning();
    const Outpost::Tuning tuning = Outpost::LoadTuning(text);
    const Neuron::JsonValue json = Neuron::ParseJson(text);

    const Outpost::RulesTuning& rules = tuning.rules;
    ExpectSame(*json.Find("rules"),
               {{"tickHz", Number(rules.tickHz)},
                {"startingOre", Number(rules.startingOre)},
                {"startingConstructors", Number(rules.startingConstructors)},
                {"miningRigOrePerSecondHome", rules.miningRigOrePerSecondHome},
                {"miningRigOrePerSecondNear", rules.miningRigOrePerSecondNear},
                {"miningRigOrePerSecondContested", rules.miningRigOrePerSecondContested},
                {"miningRigOrePerSecondRich", rules.miningRigOrePerSecondRich},
                {"exhaustedYieldPercent", Number(rules.exhaustedYieldPercent)},
                {"levelHitPointsPercent", Number(rules.levelHitPointsPercent)}},
               "rules");
    ExpectSame(*json.Find("sight"),
               {{"weaponMarginMeters", tuning.sight.weaponMarginMeters},
                {"unarmedMeters", tuning.sight.unarmedMeters},
                {"shotRevealSeconds", tuning.sight.shotRevealSeconds}},
               "sight");
    ExpectSame(*json.Find("territory"),
               {{"cutOffIncomePercent", Number(tuning.territory.cutOffIncomePercent)},
                {"suppressionRadiusMeters", tuning.territory.suppressionRadiusMeters},
                {"tickets", Number(tuning.territory.tickets)},
                {"drainIntervalSeconds", tuning.territory.drainIntervalSeconds},
                {"drainTicketsPerNodeDifference", Number(tuning.territory.drainTicketsPerNodeDifference)}},
               "territory");

    const Neuron::JsonValue::Array& hulls = json.Find("hulls")->AsArray();
    Assert::AreEqual(hulls.size(), tuning.hulls.size());
    for (size_t i = 0; i < hulls.size(); ++i)
    {
      const Outpost::HullTuning& hull = tuning.hulls[i];
      ExpectSame(hulls[i],
                 {{"id", Number(hull.id.value)},
                  {"name", hull.name},
                  {"hitPoints", Number(hull.hitPoints)},
                  {"armor", Number(hull.armor)},
                  {"speedMetersPerSecond", hull.speedMetersPerSecond},
                  {"cost", Number(hull.cost)},
                  {"buildSeconds", hull.buildSeconds},
                  {"footprintRadiusMeters", hull.footprintRadiusMeters},
                  {"turnRateDegreesPerSecond", hull.turnRateDegreesPerSecond}},
                 std::format("hulls[{}]", i));
    }

    const Neuron::JsonValue::Array& drives = json.Find("drives")->AsArray();
    Assert::AreEqual(drives.size(), tuning.drives.size());
    for (size_t i = 0; i < drives.size(); ++i)
    {
      const Outpost::DriveTuning& drive = tuning.drives[i];
      ExpectSame(drives[i],
                 {{"id", Number(drive.id.value)},
                  {"name", drive.name},
                  {"speedFactor", drive.speedFactor},
                  {"hitPointsFactor", drive.hitPointsFactor},
                  {"turnRateFactor", drive.turnRateFactor},
                  {"cost", Number(drive.cost)}},
                 std::format("drives[{}]", i));
    }

    const Neuron::JsonValue::Array& weapons = json.Find("weapons")->AsArray();
    Assert::AreEqual(weapons.size(), tuning.weapons.size());
    for (size_t i = 0; i < weapons.size(); ++i)
    {
      const Outpost::WeaponTuning& weapon = tuning.weapons[i];
      ExpectSame(weapons[i],
                 {{"id", Number(weapon.id.value)},
                  {"name", weapon.name},
                  {"damage", Number(weapon.damage)},
                  {"fireIntervalSeconds", weapon.fireIntervalSeconds},
                  {"rangeMeters", weapon.rangeMeters},
                  {"splashRadiusMeters", weapon.splashRadiusMeters},
                  {"cost", Number(weapon.cost)}},
                 std::format("weapons[{}]", i));
    }

    const Neuron::JsonValue::Array& modules = json.Find("modules")->AsArray();
    Assert::AreEqual(modules.size(), tuning.modules.size());
    for (size_t i = 0; i < modules.size(); ++i)
    {
      const Outpost::ModuleTuning& module = tuning.modules[i];
      ExpectSame(modules[i],
                 {{"id", Number(module.id.value)},
                  {"name", module.name},
                  {"sightMeters", module.sightMeters},
                  {"speedFactor", module.speedFactor},
                  {"cost", Number(module.cost)}},
                 std::format("modules[{}]", i));
    }

    const Outpost::ConstructorTuning& constructor = tuning.constructor;
    ExpectSame(*json.Find("constructor"),
               {{"hitPoints", Number(constructor.hitPoints)},
                {"armor", Number(constructor.armor)},
                {"speedMetersPerSecond", constructor.speedMetersPerSecond},
                {"cost", Number(constructor.cost)},
                {"buildSeconds", constructor.buildSeconds},
                {"footprintRadiusMeters", constructor.footprintRadiusMeters},
                {"turnRateDegreesPerSecond", constructor.turnRateDegreesPerSecond},
                {"extraConstructorBuildShare", constructor.extraConstructorBuildShare},
                {"repairPercentPerSecond", constructor.repairPercentPerSecond}},
               "constructor");

    const Neuron::JsonValue::Array& structureWeapons = json.Find("structureWeapons")->AsArray();
    Assert::AreEqual(structureWeapons.size(), tuning.structureWeapons.size());
    for (size_t i = 0; i < structureWeapons.size(); ++i)
    {
      const Outpost::StructureWeaponTuning& weapon = tuning.structureWeapons[i];
      ExpectSame(structureWeapons[i],
                 {{"id", Number(weapon.id.value)},
                  {"name", weapon.name},
                  {"damage", Number(weapon.damage)},
                  {"fireIntervalSeconds", weapon.fireIntervalSeconds},
                  {"rangeMeters", weapon.rangeMeters}},
                 std::format("structureWeapons[{}]", i));
    }

    const Neuron::JsonValue::Array& structures = json.Find("structures")->AsArray();
    Assert::AreEqual(structures.size(), tuning.structures.size());
    for (size_t i = 0; i < structures.size(); ++i)
    {
      const Outpost::StructureTuning& structure = tuning.structures[i];
      std::vector<LoadedField> fields = {{"kind", std::string(KindName(structure.kind))},
                                         {"name", structure.name},
                                         {"hitPoints", Number(structure.hitPoints)},
                                         {"armor", Number(structure.armor)},
                                         {"footprintRadiusMeters", structure.footprintRadiusMeters}};
      if (structure.cost.has_value())
        fields.push_back({"cost", Number(*structure.cost)});
      if (structure.buildConstructorSeconds.has_value())
        fields.push_back({"buildConstructorSeconds", *structure.buildConstructorSeconds});
      if (structure.structureWeapon.IsValid())
        fields.push_back({"structureWeapon", Number(structure.structureWeapon.value)});
      if (structure.nodes > 0)
        fields.push_back({"nodes", Number(structure.nodes)});
      if (structure.guns > 0)
        fields.push_back({"guns", Number(structure.guns)});
      const std::string path = std::format("structures[{}]", i);
      // A list of hulls, or none, as the file holds it.
      const auto expectHulls = [](const Neuron::JsonValue& _json, const std::vector<Outpost::HullId>& _hulls, const std::string& _path)
      {
        const Neuron::JsonValue* member = _json.Find("hulls");
        Assert::AreEqual(member != nullptr ? member->AsArray().size() : size_t{0}, _hulls.size(), Widen(_path).c_str());
        for (size_t j = 0; j < _hulls.size(); ++j)
          Assert::IsTrue(member->AsArray()[j].AsNumber() == Number(_hulls[j].value), Widen(_path).c_str());
        return member != nullptr ? size_t{1} : size_t{0};
      };
      size_t elsewhere = expectHulls(structures[i], structure.hulls, path + ".hulls");
      if (!structure.levels.empty())
      {
        ++elsewhere;
        const Neuron::JsonValue::Array& levels = structures[i].Find("levels")->AsArray();
        Assert::AreEqual(levels.size(), structure.levels.size(), Widen(path).c_str());
        for (size_t j = 0; j < levels.size(); ++j)
        {
          const std::string levelPath = std::format("{}.levels[{}]", path, j);
          const Outpost::StructureLevelTuning& loaded = structure.levels[j];
          std::vector<LoadedField> levelFields = {{"cost", Number(loaded.cost)}, {"buildSeconds", loaded.buildSeconds}};
          if (loaded.opensTier > 0)
            levelFields.push_back({"opensTier", Number(loaded.opensTier)});
          if (loaded.researchSlots > 0)
            levelFields.push_back({"researchSlots", Number(loaded.researchSlots)});
          if (loaded.nodes > 0)
            levelFields.push_back({"nodes", Number(loaded.nodes)});
          if (loaded.guns > 0)
            levelFields.push_back({"guns", Number(loaded.guns)});
          size_t levelElsewhere = expectHulls(levels[j], loaded.hulls, levelPath + ".hulls");
          if (const Neuron::JsonValue* required = levels[j].Find("requires"))
          {
            ++levelElsewhere;
            Assert::AreEqual(required->AsArray().size(), loaded.prerequisites.size(), Widen(levelPath).c_str());
            for (size_t k = 0; k < loaded.prerequisites.size(); ++k)
              Assert::IsTrue(required->AsArray()[k].AsNumber() == Number(loaded.prerequisites[k].value), Widen(levelPath).c_str());
          }
          ExpectSame(levels[j], levelFields, levelPath, levelElsewhere);
        }
      }
      ExpectSame(structures[i], fields, path, elsewhere);
    }

    const Neuron::JsonValue::Array& research = json.Find("research")->AsArray();
    Assert::AreEqual(research.size(), tuning.research.size());
    for (size_t i = 0; i < research.size(); ++i)
    {
      const Outpost::ResearchTopicTuning& topic = tuning.research[i];
      const std::string path = std::format("research[{}]", i);
      const Neuron::JsonValue& entry = research[i];
      Assert::AreEqual(entry.AsObject().size(), size_t{7}, Widen(path).c_str());
      ExpectSame(*entry.Find("effect"), EffectFields(topic.effect), path + ".effect");

      std::vector<LoadedField> fields = {{"id", Number(topic.id.value)},
                                         {"name", topic.name},
                                         {"tier", Number(topic.tier)},
                                         {"cost", Number(topic.cost)},
                                         {"researchSeconds", topic.researchSeconds}};
      for (const LoadedField& field : fields)
      {
        const Neuron::JsonValue* member = entry.Find(field.name);
        Assert::IsNotNull(member, Widen(path).c_str());
        const auto* number = std::get_if<double>(&field.value);
        Assert::IsTrue(number != nullptr ? member->AsNumber() == *number : member->AsString() == std::get<std::string>(field.value),
                       Widen(std::format("{}.{}", path, field.name)).c_str());
      }

      const Neuron::JsonValue::Array& prerequisites = entry.Find("requires")->AsArray();
      Assert::AreEqual(prerequisites.size(), topic.prerequisites.size(), Widen(path).c_str());
      for (size_t j = 0; j < prerequisites.size(); ++j)
        Assert::IsTrue(prerequisites[j].AsNumber() == Number(topic.prerequisites[j].value), Widen(path).c_str());
    }

    // ADR-069: the starting designs' short names.
    const Neuron::JsonValue::Array& startingDesigns = json.Find("startingDesigns")->AsArray();
    Assert::AreEqual(startingDesigns.size(), tuning.startingDesigns.size());
    for (size_t i = 0; i < startingDesigns.size(); ++i)
    {
      const Outpost::StartingDesignTuning& design = tuning.startingDesigns[i];
      ExpectSame(startingDesigns[i],
                 {{"hull", Number(design.hull.value)},
                  {"drive", Number(design.drive.value)},
                  {"weapon", Number(design.weapon.value)},
                  {"name", design.name}},
                 std::format("startingDesigns[{}]", i));
    }
  }

  // ADR-069: a starting design's name is optional, and one that is given names components that exist and no research
  // unlocks, under a name the server takes, and neither its components nor its name is another's.
  TEST_METHOD(RejectsABrokenStartingDesignName)
  {
    Assert::IsTrue(Outpost::LoadTuning(MINIMAL_TUNING).startingDesigns.empty(), L"none named");
    const auto replaced = [](std::string_view _from, std::string_view _to)
    {
      std::string text = ReadRepositoryTuning();
      const size_t at = text.find(_from);
      Assert::IsTrue(at != std::string::npos && text.find(_from, at + 1) == std::string::npos, Widen(_from).c_str());
      text.replace(at, _from.size(), _to);
      return text;
    };
    ExpectLoadError(replaced("\"name\": \"Swarm\"", "\"name\": \"\""), "startingDesigns[0].name");
    ExpectLoadError(replaced("\"name\": \"Swarm\"", "\"name\": \"Swarm\", \"nickname\": \"S\""), "startingDesigns[0].nickname");
    ExpectLoadError(replaced("\"weapon\": 2, \"name\": \"Picket\"", "\"weapon\": 3, \"name\": \"Picket\""),
                    "startingDesigns[1]: is not a starting design");
    ExpectLoadError(replaced("\"weapon\": 2, \"name\": \"Picket\"", "\"weapon\": 9, \"name\": \"Picket\""), "startingDesigns[1].weapon");
    ExpectLoadError(replaced("\"name\": \"Picket\"", "\"name\": \"Swarm\""), "startingDesigns[1].name");
    ExpectLoadError(replaced("\"hull\": 1, \"drive\": 1, \"weapon\": 2,", "\"hull\": 1, \"drive\": 1, \"weapon\": 1,"),
                    "startingDesigns[1]: names the design of startingDesigns[0] again");
  }

  TEST_METHOD(LoadsAMinimalFile)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(MINIMAL_TUNING);
    Assert::AreEqual(20, tuning.rules.tickHz);
    Assert::AreEqual(2, tuning.rules.startingConstructors);
    Assert::AreEqual(0.5, tuning.constructor.extraConstructorBuildShare);
    Assert::AreEqual(45.0, tuning.structures[0].footprintRadiusMeters);
    Assert::AreEqual(size_t{6}, tuning.structures.size());
    Assert::IsFalse(tuning.structures[0].cost.has_value());
    Assert::IsTrue(tuning.structures[1].cost == 300);
    Assert::IsTrue(tuning.structures[0].structureWeapon == Outpost::StructureWeaponId{1});
    Assert::IsFalse(tuning.structures[1].structureWeapon.IsValid());

    const auto& upgrade = std::get<Outpost::UpgradeEffect>(tuning.research[1].effect);
    Assert::IsTrue(upgrade.target == Outpost::UpgradeTarget::Weapon);
    Assert::IsTrue(upgrade.weapon == Outpost::WeaponId{1});
    Assert::IsTrue(upgrade.stat == Outpost::UpgradeStat::FireRate);
    Assert::IsTrue(std::get<Outpost::DriveId>(tuning.research[2].effect) == Outpost::DriveId{1});
    Assert::IsTrue(tuning.research[2].prerequisites == std::vector{Outpost::ResearchTopicId{2}});
  }

  TEST_METHOD(RejectsAMissingOrUnknownMember)
  {
    ExpectLoadError(Replace("\"cost\": 32, ", ""), "hulls[0]: has no \"cost\"");
    ExpectLoadError(Replace("\"hitPoints\": 220,", "\"hitPoints\": 220, \"hitpoint\": 1,"), "hulls[0].hitpoint");
    ExpectLoadError(Replace("\"tickHz\": 20,", ""), "rules: has no \"tickHz\"");
    ExpectLoadError(Replace("\"unarmedMeters\": 200, ", ""), "sight: has no \"unarmedMeters\"");
    ExpectLoadError(Replace(", \"suppressionRadiusMeters\": 400", ""), "territory: has no \"suppressionRadiusMeters\"");
    ExpectLoadError(Replace("\"tickets\": 1000, ", ""), "territory: has no \"tickets\"");
    ExpectLoadError(Replace("\"sightMeters\": 700, ", ""), "modules[0]: has no \"sightMeters\"");
    ExpectLoadError(Replace("\"repairPercentPerSecond\": 2 },", "\"repairPercentPerSecond\": 0 },"), "constructor.repairPercentPerSecond");
    ExpectLoadError(Replace("\"turnRateDegreesPerSecond\": 150, ", ""), "constructor: has no \"turnRateDegreesPerSecond\"");
    ExpectLoadError(Replace("\"splashRadiusMeters\": 0,", "\"splashRadiusMeters\": 0, \"splashRadius\": 5,"), "weapons[0].splashRadius");
  }

  TEST_METHOD(RejectsAWrongNumber)
  {
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": 2.5,"), "hulls[0].armor");
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": -1,"), "hulls[0].armor");
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": \"2\","), "hulls[0].armor");
    ExpectLoadError(Replace("\"speedFactor\": 1.3,", "\"speedFactor\": 0,"), "drives[0].speedFactor");
    ExpectLoadError(Replace("\"tickHz\": 20,", "\"tickHz\": 0,"), "rules.tickHz");
    ExpectLoadError(Replace("\"exhaustedYieldPercent\": 20", "\"exhaustedYieldPercent\": 101"), "rules.exhaustedYieldPercent");
    ExpectLoadError(Replace("\"weaponMarginMeters\": 50,", "\"weaponMarginMeters\": 0,"), "sight.weaponMarginMeters");
    ExpectLoadError(Replace("\"cutOffIncomePercent\": 50,", "\"cutOffIncomePercent\": 101,"), "territory.cutOffIncomePercent");
    ExpectLoadError(Replace("\"suppressionRadiusMeters\": 400", "\"suppressionRadiusMeters\": 0"), "territory.suppressionRadiusMeters");
    ExpectLoadError(Replace("\"tickets\": 1000", "\"tickets\": 0"), "territory.tickets");
    ExpectLoadError(Replace("\"speedFactor\": 0.9", "\"speedFactor\": 0"), "modules[0].speedFactor");
    ExpectLoadError(Replace("\"drainIntervalSeconds\": 10", "\"drainIntervalSeconds\": 0"), "territory.drainIntervalSeconds");
    ExpectLoadError(Replace("\"hitPoints\": 220,", "\"hitPoints\": 1e10,"), "hulls[0].hitPoints");
  }

  TEST_METHOD(RejectsABrokenStructureList)
  {
    ExpectLoadError(Replace("\"kind\": \"DefensePlatform\"", "\"kind\": \"MiningRig\""), "structures[4].kind");
    ExpectLoadError(Replace("\"kind\": \"Shipyard\"", "\"kind\": \"Factory\""), "structures[1].kind");
    // Phase 2 design §5: the Relay is a structure kind like the others, which the file must have.
    ExpectLoadError(Replace("\"kind\": \"Relay\"", "\"kind\": \"Shipyard\""), "structures[5].kind");
    ExpectLoadError(Replace("\"footprintRadiusMeters\": 45,\n      \"structureWeapon\": 1 },",
                            "\"footprintRadiusMeters\": 45, \"structureWeapon\": 2 },"),
                    "structures[0].structureWeapon");
    ExpectLoadError(Replace("\"footprintRadiusMeters\": 20,", ""), "structures[4]: has no \"footprintRadiusMeters\"");
    ExpectLoadError(Replace(", \"cost\": 300,\n      \"buildConstructorSeconds\": 40", ", \"cost\": 300"), "structures[1].cost");
  }

  // Phase 3 design §4, ADR-064: the levels above the first, which only three kinds have, up to the fifth.
  TEST_METHOD(LoadsStructureLevels)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(MINIMAL_TUNING);
    Assert::AreEqual(20, tuning.rules.levelHitPointsPercent);
    const Outpost::StructureTuning& shipyard = tuning.structures[1];
    Assert::AreEqual(size_t{1}, shipyard.levels.size());
    Assert::AreEqual(150, shipyard.levels[0].cost);
    Assert::AreEqual(30.0, shipyard.levels[0].buildSeconds);
    Assert::AreEqual(2, shipyard.TopLevel());
    Assert::AreEqual(1, tuning.structures[5].TopLevel());

    // The repository's: the Command Station to 5, the Shipyard to 3 and the Lab to 4 (gate K3).
    const Outpost::Tuning repository = Outpost::LoadTuning(ReadRepositoryTuning());
    const auto top = [&repository](Outpost::StructureKind _kind)
    { return std::ranges::find(repository.structures, _kind, &Outpost::StructureTuning::kind)->TopLevel(); };
    Assert::AreEqual(5, top(Outpost::StructureKind::CommandStation));
    Assert::AreEqual(3, top(Outpost::StructureKind::Shipyard));
    Assert::AreEqual(4, top(Outpost::StructureKind::ResearchLab));
    Assert::AreEqual(1, top(Outpost::StructureKind::Relay));
    // The station's cap and guns at each level (gates K4, K6).
    for (std::int32_t level = 1; level <= 5; ++level)
    {
      Assert::AreEqual(level + 2, Outpost::NodeCap(repository, level));
      Assert::AreEqual(std::array{1, 1, 2, 2, 3}[static_cast<size_t>(level - 1)],
                       Outpost::StationGuns(repository, Outpost::StructureKind::CommandStation, level));
      Assert::AreEqual(1, Outpost::StationGuns(repository, Outpost::StructureKind::DefensePlatform, level));
    }
  }

  TEST_METHOD(RejectsBrokenLevels)
  {
    ExpectLoadError(Replace("\"exhaustedYieldPercent\": 20, \"levelHitPointsPercent\": 20", "\"exhaustedYieldPercent\": 20"),
                    "rules: has no \"levelHitPointsPercent\"");
    ExpectLoadError(Replace("\"levelHitPointsPercent\": 20", "\"levelHitPointsPercent\": -5"), "rules.levelHitPointsPercent");
    ExpectLoadError(Replace("{ \"cost\": 150, \"buildSeconds\": 30 }", "{ \"buildSeconds\": 30 }"),
                    "structures[1].levels[0]: has no \"cost\"");
    ExpectLoadError(Replace("{ \"cost\": 150, \"buildSeconds\": 30 }", "{ \"cost\": 150, \"buildSeconds\": 0 }"),
                    "structures[1].levels[0].buildSeconds");
    ExpectLoadError(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                            "\"levels\": [ { \"cost\": 1, \"buildSeconds\": 1 }, { \"cost\": 1, \"buildSeconds\": 1 }, "
                            "{ \"cost\": 1, \"buildSeconds\": 1 }, { \"cost\": 1, \"buildSeconds\": 1 }, "
                            "{ \"cost\": 1, \"buildSeconds\": 1 } ]"),
                    "structures[1].levels");
    // A Shipyard's hulls (Phase 3 design §5): only a Shipyard names them, each hull exists, and none is named twice.
    (void)Outpost::LoadTuning(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                                      "\"hulls\": [], \"levels\": [ { \"cost\": 150, \"buildSeconds\": 30, \"hulls\": [1] } ]"));
    ExpectLoadError(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                            "\"hulls\": [1], \"levels\": [ { \"cost\": 150, \"buildSeconds\": 30, \"hulls\": [1] } ]"),
                    "structures[1].levels[0].hulls[0]");
    ExpectLoadError(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                            "\"hulls\": [2], \"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]"),
                    "structures[1].hulls[0]");
    ExpectLoadError(Replace("\"footprintRadiusMeters\": 30, \"cost\": 200,\n      \"buildConstructorSeconds\": 30,",
                            "\"footprintRadiusMeters\": 30, \"cost\": 200,\n      \"buildConstructorSeconds\": 30, \"hulls\": [1],"),
                    "structures[2].hulls");
    // A Command Station's cap and guns (Phase 3 design §7), which no other kind has, and at least one of each.
    ExpectLoadError(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                            "\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30, \"nodes\": 4 } ]"),
                    "structures[1].levels[0].nodes");
    ExpectLoadError(Replace("\"footprintRadiusMeters\": 45,\n      \"structureWeapon\": 1 },",
                            "\"footprintRadiusMeters\": 45,\n      \"structureWeapon\": 1, \"guns\": 0 },"),
                    "structures[0].guns");
    // A kind the owner gave no levels to (Phase 3 design §11).
    ExpectLoadError(Replace("\"footprintRadiusMeters\": 30, \"cost\": 200,\n      \"buildConstructorSeconds\": 40 }",
                            "\"footprintRadiusMeters\": 30, \"cost\": 200,\n      \"buildConstructorSeconds\": 40, \"levels\": [] }"),
                    "structures[5].levels");
  }

  TEST_METHOD(RejectsABrokenResearchTree)
  {
    ExpectLoadError(Replace("\"id\": 3, \"name\": \"Ion Drive\"", "\"id\": 2, \"name\": \"Ion Drive\""), "research[2].id");
    ExpectLoadError(Replace("\"requires\": [2],", "\"requires\": [99],"), "research[2].requires[0]");
    ExpectLoadError(Replace("\"researchSeconds\": 60, \"requires\": [],", "\"researchSeconds\": 60, \"requires\": [3],"),
                    "research[0].requires");
    ExpectLoadError(Replace("\"unlockDrive\": 1", "\"unlockDrive\": 2"), "research[2].effect.unlockDrive");
    ExpectLoadError(Replace("\"weapon\": 1, \"stat\"", "\"weapon\": 4, \"stat\""), "research[1].effect.weapon");
    ExpectLoadError(Replace("\"weapon\": 1, \"stat\": \"fireRate\"", "\"weapon\": 1, \"stat\": \"damage\""), "research[1].effect.stat");
    // Design §8: an upgrade raises its target's one rate, so a weapon's hit points are no upgrade the game can apply.
    ExpectLoadError(Replace("\"weapon\": 1, \"stat\": \"fireRate\"", "\"weapon\": 1, \"stat\": \"hitPoints\""), "research[1].effect.stat");
    ExpectLoadError(Replace("\"upgrade\": \"allHulls\",", "\"upgrade\": \"allHulls\", \"weapon\": 1,"), "research[0].effect.weapon");
    ExpectLoadError(Replace("{ \"unlockDrive\": 1 }", "{}"), "research[2].effect");
    // Phase 1 design §6: each target raises its one rate, and a structure weapon's upgrade names one that exists.
    ExpectLoadError(Replace("\"stat\": \"speed\"", "\"stat\": \"hitPoints\""), "research[6].effect.stat");
    ExpectLoadError(Replace("\"structureWeapon\": 1, \"stat\"", "\"structureWeapon\": 2, \"stat\""), "research[5].effect.structureWeapon");
  }

  // ADR-033: a topic has a tier, from 1 to the last; a gateway opens its own tier, which has no other; every other topic of
  // a later tier requires its gateway; and no topic requires one of a later tier.
  // ADR-033: a topic has a tier, from 1 to the last, and requires none of a later tier; each tier after the first is
  // opened by one level of the Research Lab, a later tier by a later level, and a level's topics exist (Phase 3 design §6).
  TEST_METHOD(RejectsBrokenTiers)
  {
    ExpectLoadError(Replace("\"name\": \"Ion Drive\", \"tier\": 1", "\"name\": \"Ion Drive\""), "research[2]: has no \"tier\"");
    ExpectLoadError(Replace("\"name\": \"Ion Drive\", \"tier\": 1", "\"name\": \"Ion Drive\", \"tier\": 4"), "research[2].tier");
    ExpectLoadError(Replace("\"opensTier\": 2", "\"opensTier\": 3"), "structures[2].levels[0].opensTier");
    ExpectLoadError(Replace("\"opensTier\": 2", "\"opensTier\": 4"), "structures[2].levels[0].opensTier");
    ExpectLoadError(Replace("\"opensTier\": 2, ", ""), "research[4].tier");
    ExpectLoadError(Replace("\"opensTier\": 2, \"requires\": [1]", "\"opensTier\": 2, \"requires\": [99]"),
                    "structures[2].levels[0].requires[0]");
    ExpectLoadError(Replace("\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30 } ]",
                            "\"levels\": [ { \"cost\": 150, \"buildSeconds\": 30, \"opensTier\": 2 } ]"),
                    "structures[1].levels[0].opensTier");
    ExpectLoadError(Replace("\"name\": \"Ion Drive\", \"tier\": 1, \"cost\": 200, \"researchSeconds\": 90, \"requires\": [2]",
                            "\"name\": \"Ion Drive\", \"tier\": 1, \"cost\": 200, \"researchSeconds\": 90, \"requires\": [5]"),
                    "research[2].requires[0]");
    ExpectLoadError(Replace("\"name\": \"Deep Core Survey\", \"tier\": 2", "\"name\": \"Deep Core Survey\", \"tier\": 3"),
                    "research[8].tier");
    // No topic opens a tier now; that is a level's.
    ExpectLoadError(Replace("{ \"upgrade\": \"allShips\", \"stat\": \"speed\", \"percent\": 10 }", "{ \"opensTier\": 2 }"),
                    "research[6].effect");
  }

  // Phase 1 design §6: the kinds of effect load; and a tier is opened by the Research Lab's level, which also gives the
  // second slot (Phase 3 design §6).
  TEST_METHOD(LoadsTheTiersEffects)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(MINIMAL_TUNING);
    Assert::AreEqual(size_t{9}, tuning.research.size());
    Assert::AreEqual(1, Outpost::LabLevelFor(tuning, 1));
    Assert::AreEqual(2, Outpost::LabLevelFor(tuning, 2));
    Assert::AreEqual(1, Outpost::OpenTier(tuning, 1));
    Assert::AreEqual(2, Outpost::OpenTier(tuning, 2));
    Assert::AreEqual(1, Outpost::ResearchSlots(tuning, 2));
    Assert::AreEqual(2, Outpost::ResearchSlots(tuning, 3));
    Assert::IsTrue(tuning.structures[2].levels[0].prerequisites == std::vector{Outpost::ResearchTopicId{1}});
    const auto upgrade = [&tuning](size_t _index) { return std::get<Outpost::UpgradeEffect>(tuning.research[_index].effect); };
    Assert::IsTrue(upgrade(4).target == Outpost::UpgradeTarget::AllStructures && upgrade(4).stat == Outpost::UpgradeStat::HitPoints);
    Assert::IsTrue(upgrade(5).target == Outpost::UpgradeTarget::StructureWeapon &&
                   upgrade(5).structureWeapon == Outpost::StructureWeaponId{1});
    Assert::IsTrue(upgrade(6).target == Outpost::UpgradeTarget::AllShips && upgrade(6).stat == Outpost::UpgradeStat::Speed);
    Assert::IsTrue(upgrade(7).target == Outpost::UpgradeTarget::Constructors && upgrade(7).stat == Outpost::UpgradeStat::BuildRate);
    Assert::IsTrue(upgrade(8).target == Outpost::UpgradeTarget::Asteroids && upgrade(8).percent == 30);
    Assert::AreEqual(std::string("Shipyard build speed +25%"), Outpost::EffectText(tuning, tuning.research[3]));
    Assert::AreEqual(std::string("Defence gun fire rate +20%"), Outpost::EffectText(tuning, tuning.research[5]));
    Assert::AreEqual(std::string("Constructor build and repair rate +25%"), Outpost::EffectText(tuning, tuning.research[7]));
  }

  TEST_METHOD(RejectsText)
  {
    ExpectLoadError(Replace("\"cost\": 20 } ],", "\"cost\": 20, } ],"), "JSON line 10");
    ExpectLoadError("[]", "the file");
  }
};
} // namespace GameLogicTests
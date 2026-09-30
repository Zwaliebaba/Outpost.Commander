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

// Every loaded field equals the file's member of that name, and the file has no member the loader did not load.
void ExpectSame(const Neuron::JsonValue& _json, const std::vector<LoadedField>& _loaded, std::string_view _path)
{
  Assert::AreEqual(_json.AsObject().size(), _loaded.size(), Widen(std::format("{}: member count", _path)).c_str());
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
  }
  return {};
}

std::vector<LoadedField> EffectFields(const Outpost::ResearchEffect& _effect)
{
  if (const auto* upgrade = std::get_if<Outpost::UpgradeEffect>(&_effect))
  {
    constexpr std::array<std::string_view, 4> TARGETS = {"miningRig", "allHulls", "weapon", "shipyards"};
    constexpr std::array<std::string_view, 4> STATS = {"income", "hitPoints", "fireRate", "buildSpeed"};
    std::vector<LoadedField> fields = {{"upgrade", std::string(TARGETS[Neuron::I(upgrade->target)])},
                                       {"stat", std::string(STATS[Neuron::I(upgrade->stat)])},
                                       {"percent", Number(upgrade->percent)}};
    if (upgrade->weapon.IsValid())
      fields.push_back({"weapon", Number(upgrade->weapon.value)});
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
  "rules": { "tickHz": 20, "startingOre": 1000, "miningRigOrePerSecondHome": 5, "miningRigOrePerSecondContested": 8,
             "aiReviewIntervalSeconds": 60 },
  "hulls": [ { "id": 1, "name": "Small", "hitPoints": 220, "armor": 2, "speedMetersPerSecond": 60, "cost": 32, "buildSeconds": 10 } ],
  "drives": [ { "id": 1, "name": "Ion", "speedFactor": 1.3, "hitPointsFactor": 0.9, "cost": 20 } ],
  "weapons": [ { "id": 1, "name": "Mass Driver", "damage": 14, "fireIntervalSeconds": 0.4, "rangeMeters": 120,
                 "splashRadiusMeters": 0, "cost": 35 } ],
  "structureWeapons": [ { "id": 1, "name": "Defence gun", "damage": 30, "fireIntervalSeconds": 1.0, "rangeMeters": 250 } ],
  "structures": [
    { "kind": "CommandStation", "name": "Command Station", "hitPoints": 5000, "armor": 10, "structureWeapon": 1 },
    { "kind": "Shipyard", "name": "Shipyard", "hitPoints": 2500, "armor": 0, "cost": 300, "buildConstructorSeconds": 40 },
    { "kind": "ResearchLab", "name": "Research Lab", "hitPoints": 1500, "armor": 0, "cost": 200, "buildConstructorSeconds": 30 },
    { "kind": "MiningRig", "name": "Mining Rig", "hitPoints": 800, "armor": 0, "cost": 50, "buildConstructorSeconds": 10 },
    { "kind": "DefensePlatform", "name": "Defence Platform", "hitPoints": 1500, "armor": 10, "cost": 150,
      "buildConstructorSeconds": 20, "structureWeapon": 1 }
  ],
  "research": [
    { "id": 1, "name": "Hull Plating", "cost": 150, "researchSeconds": 60, "requires": [],
      "effect": { "upgrade": "allHulls", "stat": "hitPoints", "percent": 15 } },
    { "id": 2, "name": "Mass Driver Calibration", "cost": 150, "researchSeconds": 75, "requires": [1],
      "effect": { "upgrade": "weapon", "weapon": 1, "stat": "fireRate", "percent": 15 } },
    { "id": 3, "name": "Ion Drive", "cost": 200, "researchSeconds": 90, "requires": [2],
      "effect": { "unlockDrive": 1 } }
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
                {"miningRigOrePerSecondHome", rules.miningRigOrePerSecondHome},
                {"miningRigOrePerSecondContested", rules.miningRigOrePerSecondContested},
                {"aiReviewIntervalSeconds", rules.aiReviewIntervalSeconds}},
               "rules");

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
                  {"buildSeconds", hull.buildSeconds}},
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
                                         {"armor", Number(structure.armor)}};
      if (structure.cost.has_value())
        fields.push_back({"cost", Number(*structure.cost)});
      if (structure.buildConstructorSeconds.has_value())
        fields.push_back({"buildConstructorSeconds", *structure.buildConstructorSeconds});
      if (structure.structureWeapon.IsValid())
        fields.push_back({"structureWeapon", Number(structure.structureWeapon.value)});
      ExpectSame(structures[i], fields, std::format("structures[{}]", i));
    }

    const Neuron::JsonValue::Array& research = json.Find("research")->AsArray();
    Assert::AreEqual(research.size(), tuning.research.size());
    for (size_t i = 0; i < research.size(); ++i)
    {
      const Outpost::ResearchTopicTuning& topic = tuning.research[i];
      const std::string path = std::format("research[{}]", i);
      const Neuron::JsonValue& entry = research[i];
      Assert::AreEqual(entry.AsObject().size(), size_t{6}, Widen(path).c_str());
      ExpectSame(*entry.Find("effect"), EffectFields(topic.effect), path + ".effect");

      std::vector<LoadedField> fields = {
        {"id", Number(topic.id.value)}, {"name", topic.name}, {"cost", Number(topic.cost)}, {"researchSeconds", topic.researchSeconds}};
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
  }

  TEST_METHOD(LoadsAMinimalFile)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(MINIMAL_TUNING);
    Assert::AreEqual(20, tuning.rules.tickHz);
    Assert::AreEqual(size_t{5}, tuning.structures.size());
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
    ExpectLoadError(Replace("\"splashRadiusMeters\": 0,", "\"splashRadiusMeters\": 0, \"splashRadius\": 5,"), "weapons[0].splashRadius");
  }

  TEST_METHOD(RejectsAWrongNumber)
  {
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": 2.5,"), "hulls[0].armor");
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": -1,"), "hulls[0].armor");
    ExpectLoadError(Replace("\"armor\": 2,", "\"armor\": \"2\","), "hulls[0].armor");
    ExpectLoadError(Replace("\"speedFactor\": 1.3,", "\"speedFactor\": 0,"), "drives[0].speedFactor");
    ExpectLoadError(Replace("\"tickHz\": 20,", "\"tickHz\": 0,"), "rules.tickHz");
    ExpectLoadError(Replace("\"hitPoints\": 220,", "\"hitPoints\": 1e10,"), "hulls[0].hitPoints");
  }

  TEST_METHOD(RejectsABrokenStructureList)
  {
    ExpectLoadError(Replace("\"kind\": \"Shipyard\"", "\"kind\": \"MiningRig\""), "structures[3].kind");
    ExpectLoadError(Replace("\"kind\": \"Shipyard\"", "\"kind\": \"Factory\""), "structures[1].kind");
    ExpectLoadError(Replace("\"armor\": 10, \"structureWeapon\": 1 },", "\"armor\": 10, \"structureWeapon\": 2 },"),
                    "structures[0].structureWeapon");
    ExpectLoadError(Replace(", \"cost\": 300, \"buildConstructorSeconds\": 40", ", \"cost\": 300"), "structures[1].cost");
  }

  TEST_METHOD(RejectsABrokenResearchTree)
  {
    ExpectLoadError(Replace("\"id\": 3, \"name\": \"Ion Drive\"", "\"id\": 2, \"name\": \"Ion Drive\""), "research[2].id");
    ExpectLoadError(Replace("\"requires\": [2],", "\"requires\": [9],"), "research[2].requires[0]");
    ExpectLoadError(Replace("\"researchSeconds\": 60, \"requires\": [],", "\"researchSeconds\": 60, \"requires\": [3],"),
                    "research[0].requires");
    ExpectLoadError(Replace("\"unlockDrive\": 1", "\"unlockDrive\": 2"), "research[2].effect.unlockDrive");
    ExpectLoadError(Replace("\"weapon\": 1, \"stat\"", "\"weapon\": 4, \"stat\""), "research[1].effect.weapon");
    ExpectLoadError(Replace("\"stat\": \"fireRate\"", "\"stat\": \"damage\""), "research[1].effect.stat");
    ExpectLoadError(Replace("\"upgrade\": \"allHulls\",", "\"upgrade\": \"allHulls\", \"weapon\": 1,"), "research[0].effect.weapon");
    ExpectLoadError(Replace("{ \"unlockDrive\": 1 }", "{}"), "research[2].effect");
  }

  TEST_METHOD(RejectsText)
  {
    ExpectLoadError(Replace("\"cost\": 20 } ],", "\"cost\": 20, } ],"), "JSON line 5");
    ExpectLoadError("[]", "the file");
  }
};
} // namespace GameLogicTests

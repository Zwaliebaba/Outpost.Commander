#include "pch.h"

#include "ContentHash.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A field the digest misses is two different rule sets that hash alike, which is the failure the
// digest exists to prevent (OpenQuestions.md Q20). One mutation per table, each one must move it.
namespace ContentTests
{

namespace
{

/// One row in every table the simulation reads, so that a mutation has something to move.
Outpost::ContentTree Furnished()
{
  Outpost::ContentTree tree{};
  Outpost::ChassisDesc chassis{};
  chassis.id = "ChassisLight";
  chassis.hitPoints = 100;
  chassis.mounts = 1;
  tree.components.chassis.push_back(chassis);

  Outpost::DriveDesc drive{};
  drive.id = "DriveWheels";
  drive.maxSlopePercent = 25;
  drive.crossesWater = false;
  tree.components.drives.push_back(drive);

  Outpost::ModuleDesc module{};
  module.id = "ModuleCannon";
  module.damage = 60;
  module.longRangeSubunits = 16 * 64 * 256;
  tree.components.modules.push_back(module);

  Outpost::StructureDesc structure{};
  structure.id = "Generator";
  structure.role = Outpost::StructureRole::Generator;
  structure.servesExtractors = 4;
  structure.modules = {"GeneratorModule"};
  tree.structures.structures.push_back(structure);

  Outpost::StructureModuleDesc structureModule{};
  structureModule.id = "GeneratorModule";
  structureModule.amount = 1;
  tree.structures.modules.push_back(structureModule);

  Outpost::ResearchItemDesc research{};
  research.id = "ResearchCannon";
  research.costHundredths = 20000;
  research.prerequisites = {"ResearchMachineGun"};
  tree.research.push_back(research);

  tree.damage.modifierPercent[0][0] = 120;
  tree.damage.armorFactorPercent[0] = 100;
  tree.damage.armorKind[0] = Outpost::ArmorKind::Kinetic;
  return tree;
}

} // namespace

TEST_CLASS(ContentHashTests)
{
public:
  TEST_METHOD(TheSameTreeHashesTheSameAndAnEmptyOneDiffers)
  {
    Assert::AreEqual(Outpost::ContentHash(Furnished()), Outpost::ContentHash(Furnished()));
    const Outpost::ContentTree empty{};
    Assert::AreNotEqual(Outpost::ContentHash(Furnished()), Outpost::ContentHash(empty));
    Assert::AreEqual(Outpost::ContentHash(empty), Outpost::ContentHash(Outpost::ContentTree{}));
  }

  TEST_METHOD(OneFieldOfEveryTableMovesTheDigest)
  {
    const std::uint64_t baseline = Outpost::ContentHash(Furnished());
    const auto moved = [baseline](const Outpost::ContentTree& _tree, const wchar_t* _what)
    { Assert::AreNotEqual(baseline, Outpost::ContentHash(_tree), _what); };

    Outpost::ContentTree chassis = Furnished();
    chassis.components.chassis[0].hitPoints += 1;
    moved(chassis, L"a chassis's hit points");

    Outpost::ContentTree drive = Furnished();
    drive.components.drives[0].maxSlopePercent += 1;
    moved(drive, L"a drive's maximum slope, which pathing reads");

    Outpost::ContentTree water = Furnished();
    water.components.drives[0].crossesWater = true;
    moved(water, L"a drive's water flag, which is a bool and easy to leave out");

    Outpost::ContentTree module = Furnished();
    module.components.modules[0].longRangeSubunits += 1;
    moved(module, L"a module's long range");

    Outpost::ContentTree structure = Furnished();
    structure.structures.structures[0].servesExtractors += 1;
    moved(structure, L"a structure's served extractors");

    Outpost::ContentTree structureModules = Furnished();
    structureModules.structures.structures[0].modules[0] = "Other";
    moved(structureModules, L"a structure's module list, which is a vector of ids");

    Outpost::ContentTree structureModule = Furnished();
    structureModule.structures.modules[0].amount += 1;
    moved(structureModule, L"a structure module's amount");

    Outpost::ContentTree research = Furnished();
    research.research[0].costHundredths += 1;
    moved(research, L"a research item's cost");

    Outpost::ContentTree prerequisites = Furnished();
    prerequisites.research[0].prerequisites.push_back("ResearchArmor");
    moved(prerequisites, L"a research item's prerequisites");

    Outpost::ContentTree damage = Furnished();
    damage.damage.modifierPercent[0][0] += 1;
    moved(damage, L"one cell of the damage matrix");

    Outpost::ContentTree armor = Furnished();
    armor.damage.armorFactorPercent[0] += 1;
    moved(armor, L"an armour factor");

    Outpost::ContentTree kind = Furnished();
    kind.damage.armorKind[0] = Outpost::ArmorKind::Thermal;
    moved(kind, L"which armour a weapon class is read against");

    Outpost::ContentTree renamed = Furnished();
    renamed.components.chassis[0].id = "ChassisLightII";
    moved(renamed, L"a row's id");
  }

  TEST_METHOD(RowsThatRunTogetherDoNotHashAsOne)
  {
    // Ids are length-prefixed, so "ab" then "c" cannot digest as "a" then "bc".
    Outpost::ContentTree split = Furnished();
    split.components.chassis[0].id = "ab";
    split.components.chassis[0].name = "c";
    Outpost::ContentTree joined = Furnished();
    joined.components.chassis[0].id = "a";
    joined.components.chassis[0].name = "bc";
    Assert::AreNotEqual(Outpost::ContentHash(split), Outpost::ContentHash(joined));
  }

  TEST_METHOD(TheClientsOwnTablesAreNotInTheDigest)
  {
    // Biomes, models and sounds change no outcome, so a player with different art plays the same
    // match; m3-multiplayer/T3's join check may want a wider digest and this is deliberately not it.
    Outpost::ContentTree art = Furnished();
    Outpost::BiomeDesc biome{};
    biome.id = "Desert";
    art.biomes.push_back(biome);
    Outpost::ModelDesc model{};
    model.id = "ChassisLight";
    art.models.push_back(model);
    Assert::AreEqual(Outpost::ContentHash(Furnished()), Outpost::ContentHash(art));
  }
};

} // namespace ContentTests

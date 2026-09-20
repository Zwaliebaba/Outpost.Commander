#include "pch.h"

#include "BrokenFixtures.h"
#include "ContentLoader.h"
#include "ContentValidator.h"

#include <filesystem>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ContentTests
{

namespace
{

/// A scratch directory per instance, so that two tests in one process never share one.
int g_counter = 0;

struct ScratchTree
{
  std::filesystem::path path;

  ScratchTree()
  {
    path = std::filesystem::temp_directory_path() / "OutpostValidatorTests" / std::to_string(::GetCurrentProcessId()) /
           std::to_string(++g_counter);
    std::filesystem::create_directories(path);
    WriteGoodTree(path);
  }

  ~ScratchTree()
  {
    RemoveScratch(path);
  }
};

/// Loads the scratch tree, validates it without touching the disk for assets, and returns the
/// findings. Loading itself must succeed: these are the faults loading cannot see.
[[nodiscard]] std::vector<Outpost::ContentDiagnostic> FindingsOf(const ScratchTree& _tree)
{
  Outpost::ContentTree loaded;
  std::vector<Outpost::ContentDiagnostic> diagnostics;
  Assert::IsTrue(Outpost::LoadContent(_tree.path, loaded, diagnostics), L"the tree must load before it can be validated");
  // The result is the emptiness of the findings, which the caller checks for itself.
  static_cast<void>(Outpost::ValidateContent(loaded, std::filesystem::path(), diagnostics));
  return diagnostics;
}

[[nodiscard]] bool Mentions(const std::vector<Outpost::ContentDiagnostic>& _diagnostics, const char* _text)
{
  for (const Outpost::ContentDiagnostic& diagnostic : _diagnostics)
  {
    if (diagnostic.message.find(_text) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CLASS(ContentValidatorTests)
{
public:
  TEST_METHOD(TheGoodTreeHasNoFindings)
  {
    const ScratchTree tree;
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    for (const Outpost::ContentDiagnostic& finding : findings)
    {
      Logger::WriteMessage(finding.ToString().c_str());
    }
    Assert::AreEqual(std::size_t{0}, findings.size());
  }

  TEST_METHOD(APrerequisiteNothingDefinesIsFoundAtItsRowsLine)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Research.json", "{\n"
                                              "  \"version\": 1,\n"
                                              "  \"items\": [\n"
                                              "    {\n"
                                              "      \"id\": \"Armour\", \"name\": \"Armour\",\n"
                                              "      \"prerequisites\": [\"Nothing\"], \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"ChassisArmor\", \"targetClass\": 0, \"upgradePercent\": 5\n"
                                              "    }\n"
                                              "  ]\n"
                                              "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{1}, findings.size());
    Assert::AreEqual(std::string("Research.json"), findings.front().file);
    Assert::AreEqual(4, findings.front().line, L"the row that names the missing prerequisite");
    Assert::IsTrue(Mentions(findings, "'Nothing' is not a research item"));
  }

  TEST_METHOD(ACycleInTheResearchTreeIsFound)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Research.json", "{\n"
                                              "  \"version\": 1,\n"
                                              "  \"items\": [\n"
                                              "    { \"id\": \"A\", \"name\": \"A\", \"prerequisites\": [\"B\"],\n"
                                              "      \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"Unlock\", \"unlocks\": \"MachineGun\" },\n"
                                              "    { \"id\": \"B\", \"name\": \"B\", \"prerequisites\": [\"A\"],\n"
                                              "      \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"Unlock\", \"unlocks\": \"Cannon\" }\n"
                                              "  ]\n"
                                              "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::IsTrue(Mentions(findings, "the research tree has a cycle here"));
    Assert::AreEqual(std::string("Research.json"), findings.front().file);
    Assert::IsTrue(findings.front().line > 0, L"a cycle names a line like every other finding");
  }

  TEST_METHOD(AnUnlockNamingNothingUnlockableIsFound)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Research.json", "{\n"
                                              "  \"version\": 1,\n"
                                              "  \"items\": [\n"
                                              "    { \"id\": \"A\", \"name\": \"A\", \"prerequisites\": [],\n"
                                              "      \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"Unlock\", \"unlocks\": \"Teleporter\" }\n"
                                              "  ]\n"
                                              "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{1}, findings.size());
    Assert::IsTrue(Mentions(findings, "'Teleporter'"));
    Assert::AreEqual(4, findings.front().line);
  }

  TEST_METHOD(ADuplicateIdNamesBothLines)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Structures.json", "{\n"
                                                "  \"version\": 1,\n"
                                                "  \"structures\": [\n"
                                                "    { \"id\": \"Cannon\", \"name\": \"Not a cannon\", \"role\": \"Tower\",\n"
                                                "      \"strength\": \"Medium\", \"model\": \"m\",\n"
                                                "      \"footprintCellsX\": 1, \"footprintCellsY\": 1, \"hitPoints\": 1,\n"
                                                "      \"kineticArmor\": 0, \"thermalArmor\": 0, \"costHundredths\": 1,\n"
                                                "      \"buildTimeTicks\": 1, \"sightSubunits\": 1, \"moduleSlots\": 0 }\n"
                                                "  ],\n"
                                                "  \"modules\": []\n"
                                                "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{1}, findings.size());
    Assert::AreEqual(std::string("Structures.json"), findings.front().file, L"the second use is the one to delete");
    Assert::AreEqual(4, findings.front().line);
    Assert::IsTrue(Mentions(findings, "already used at Components.json("), L"and it points at the first");
  }

  TEST_METHOD(AStructureNamingAModuleNothingDefinesIsFound)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Structures.json", "{\n"
                                                "  \"version\": 1,\n"
                                                "  \"structures\": [\n"
                                                "    { \"id\": \"Factory\", \"name\": \"Factory\", \"role\": \"Factory\",\n"
                                                "      \"strength\": \"Medium\", \"model\": \"m\",\n"
                                                "      \"footprintCellsX\": 3, \"footprintCellsY\": 3, \"hitPoints\": 1,\n"
                                                "      \"kineticArmor\": 0, \"thermalArmor\": 0, \"costHundredths\": 1,\n"
                                                "      \"buildTimeTicks\": 1, \"sightSubunits\": 1, \"moduleSlots\": 2,\n"
                                                "      \"modules\": [\"Nothing\"] }\n"
                                                "  ],\n"
                                                "  \"modules\": []\n"
                                                "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{1}, findings.size());
    Assert::IsTrue(Mentions(findings, "'Nothing', which is not a structure module"));
  }

  TEST_METHOD(AWeaponWhoseLongRangeIsUnderItsShortRangeIsFound)
  {
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    for (Outpost::ModuleDesc& module : loaded.components.modules)
    {
      if (module.id == "Cannon")
      {
        module.longRangeSubunits = 1;
      }
    }
    Assert::IsFalse(Outpost::ValidateContent(loaded, std::filesystem::path(), diagnostics));
    Assert::IsTrue(Mentions(diagnostics, "its long range is under its short range"));
  }

  TEST_METHOD(AShotsLookIsBothHalvesOrNeither)
  {
    // m1-vertical-slice/C8. A weapon row says what its shot looks like and for how long, and either
    // field on its own is a row somebody meant to finish: a model with no lifetime is drawn for no
    // ticks, and a lifetime with no model names nothing to draw. NEITHER is legal and is what a
    // weapon showing nothing in flight says, so the pair is checked rather than each field required.
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    Assert::IsTrue(Outpost::ValidateContent(loaded, std::filesystem::path(), diagnostics), L"neither half is set, and that is legal");

    for (Outpost::ModuleDesc& module : loaded.components.modules)
    {
      if (module.id == "Cannon")
      {
        module.projectileLifetimeTicks = 10; // A duration for a shot that has no look
      }
    }
    Assert::IsFalse(Outpost::ValidateContent(loaded, std::filesystem::path(), diagnostics));
    Assert::IsTrue(Mentions(diagnostics, "a shot has a look and a duration or it has neither"));
  }

  TEST_METHOD(AProjectileModelNoFileDefinesIsFoundLikeAnyOther)
  {
    // The look a weapon names is checked exactly as its own model is, which is the point of putting
    // it through the same lambda rather than writing a second rule - mutation testing found this
    // check had no case at all while the pairing rule above had one.
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    for (Outpost::ModuleDesc& module : loaded.components.modules)
    {
      if (module.id == "Cannon")
      {
        module.projectileModel = "NoSuchShell";
        module.projectileLifetimeTicks = 10;
      }
    }
    // WITH the directory, because the asset checks are skipped without one. The scratch tree has
    // nothing under Models, so every row naming a model is a finding here - which is why this case
    // asserts the PROJECTILE'S OWN NAME and not the shared wording. No other row names it, so the
    // string appears only if the projectile is checked at all.
    Assert::IsFalse(Outpost::ValidateContent(loaded, tree.path, diagnostics));
    Assert::IsTrue(Mentions(diagnostics, "'NoSuchShell'"));
  }

  TEST_METHOD(ASystemModuleMayNotCarryAProjectile)
  {
    // A builder or a sensor fires nothing, so a projectile on one is a row that would never draw
    // what it names - which is worth saying at the table rather than leaving as silence.
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    bool found = false;
    for (Outpost::ModuleDesc& module : loaded.components.modules)
    {
      if (module.systemKind != Outpost::SystemKind::None)
      {
        module.projectileModel = module.model; // A model that certainly exists, so this is the only fault
        module.projectileLifetimeTicks = 10;
        found = true;
      }
    }
    Assert::IsTrue(found, L"the fixture has a system module, or this case tests nothing");
    Assert::IsFalse(Outpost::ValidateContent(loaded, std::filesystem::path(), diagnostics));
    Assert::IsTrue(Mentions(diagnostics, "a system module fires nothing"));
  }

  TEST_METHOD(AModelNoFileDefinesIsFoundWhenTheDirectoryIsGiven)
  {
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    // Nothing under Models defines a model at all, so every row that names one is a finding.
    Assert::IsFalse(Outpost::ValidateContent(loaded, tree.path, diagnostics));
    Assert::IsTrue(Mentions(diagnostics, "which no file under Models defines"));
    Assert::IsTrue(Mentions(diagnostics, "the texture 'LandscapeDefault.dds' is not under Terrain"));
    Assert::IsTrue(Mentions(diagnostics, "the wave 'Cannon.wav' is not under Sounds"));
  }

  TEST_METHOD(ATilePaletteNamingNoBiomeIsFoundAndOneNamingABiomeIsNot)
  {
    const ScratchTree tree;
    Outpost::ContentTree loaded;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    Assert::IsTrue(Outpost::LoadContent(tree.path, loaded, diagnostics));
    Assert::AreEqual(std::size_t{1}, loaded.landscapes.size());
    Assert::IsTrue(loaded.landscapes.front().tiles.front().palette.empty(), L"absent means the landscape's own");

    // A tile that names a biome is a region (OpenQuestions.md Q18) and validates.
    std::string document(GOOD_LANDSCAPE);
    const std::string anchor = "\"edgeFalloff\": 32";
    document.replace(document.find(anchor), anchor.size(), anchor + ", \"palette\": \"Default\"");
    WriteFixture(tree.path / "Landscapes" / "Slice.json", document);
    Assert::AreEqual(std::size_t{0}, FindingsOf(tree).size());

    // One that names something else is not.
    std::string broken(GOOD_LANDSCAPE);
    broken.replace(broken.find(anchor), anchor.size(), anchor + ", \"palette\": \"Tundra\"");
    WriteFixture(tree.path / "Landscapes" / "Slice.json", broken);
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{1}, findings.size());
    Assert::IsTrue(Mentions(findings, "tile 0 is coloured by 'Tundra', which is not a biome"));
  }

  TEST_METHOD(TheValidatorReportsEveryFaultRatherThanTheFirst)
  {
    const ScratchTree tree;
    WriteFixture(tree.path / "Research.json", "{\n"
                                              "  \"version\": 1,\n"
                                              "  \"items\": [\n"
                                              "    { \"id\": \"A\", \"name\": \"A\", \"prerequisites\": [\"X\"],\n"
                                              "      \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"Unlock\", \"unlocks\": \"Y\" },\n"
                                              "    { \"id\": \"B\", \"name\": \"B\", \"prerequisites\": [\"Z\"],\n"
                                              "      \"costHundredths\": 1, \"timeTicks\": 1,\n"
                                              "      \"effect\": \"Unlock\", \"unlocks\": \"W\" }\n"
                                              "  ]\n"
                                              "}\n");
    const std::vector<Outpost::ContentDiagnostic> findings = FindingsOf(tree);
    Assert::AreEqual(std::size_t{4}, findings.size(), L"two missing prerequisites and two unknown unlocks");
  }
};

} // namespace ContentTests

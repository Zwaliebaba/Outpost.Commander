#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <limits>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr float TOLERANCE = 1e-3f;
constexpr std::array<const char*, 3> HULLS{"Small", "Medium", "Large"};

// Every kind of structure drawn with the one model, Small, for the catalogs below.
constexpr std::string_view STRUCTURES = R"("structures": [ { "structure": "CommandStation", "model": "Small" },
  { "structure": "Shipyard", "model": "Small", "tint": 0.5 }, { "structure": "ResearchLab", "model": "Small" },
  { "structure": "MiningRig", "model": "Small" }, { "structure": "DefensePlatform", "model": "Small" },
  { "structure": "Relay", "model": "Small" }, { "structure": "RepairBay", "model": "Small" } ], "constructor": "Small",
  "exhausts": [ { "drive": 1, "color": { "red": 0.3, "green": 0.85, "blue": 1 } } ],
  "constructorExhaust": { "red": 0.8, "green": 0.8, "blue": 0.8 },
  "shots": [ { "weapon": 2, "look": "beam" } ])";

// A catalog of one set of one model, with one member replaced, for the loader's error cases.
std::string OneModel(std::string_view _setName, std::string_view _model, std::string_view _color)
{
  return std::format(R"({{ "sets": [ {{ "name": "{}", "color": {}, "models": [ {} ] }} ], "players": [], "hulls": [], {} }})", _setName,
                     _color, _model, STRUCTURES);
}

constexpr std::string_view GOOD_MODEL = R"({ "name": "Small", "lengthMeters": 20 })";
constexpr std::string_view GOOD_COLOR = R"({ "red": 0.5, "green": 0.5, "blue": 0.5 })";

void ExpectRejected(const std::string& _json)
{
  Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::LoadModelCatalog(_json); });
}
} // namespace

TEST_CLASS(ModelCatalogTests)
{
public:
  TEST_METHOD(LoadsTheRepositoryCatalog)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const Outpost::ModelSet& human = catalog.Set("Human");
    const Outpost::ModelSet& tarkan = catalog.Set("Tarkan");
    Assert::AreEqual(size_t{3}, catalog.Set("Asteroids").models.size());

    // The two sides have the same models, so that every design can be drawn for either (design §11).
    Assert::AreEqual(human.models.size(), tarkan.models.size());
    for (const Outpost::ModelEntry& model : human.models)
      Assert::AreEqual(model.lengthMeters, tarkan.Model(model.name).lengthMeters);

    // The player draws with the Human set and the AI with the Tarkan set, and each hull of the tuning data has a model.
    Assert::IsTrue(catalog.SetForPlayer(Outpost::PlayerId{1}) == &human);
    Assert::IsTrue(catalog.SetForPlayer(Outpost::PlayerId{2}) == &tarkan);
    Assert::IsNull(catalog.SetForPlayer(Outpost::PlayerId{3}));
    // The pirates draw with the Tarkan set's meshes, in a color of their own (Phase 4 design §8, ADR-073).
    const Outpost::ModelSet& pirate = *catalog.SetForPlayer(Outpost::PIRATES);
    Assert::AreEqual(std::string("Tarkan"), pirate.meshes);
    Assert::AreEqual(tarkan.models.size(), pirate.models.size());
    Assert::IsFalse(pirate.color.x == tarkan.color.x && pirate.color.y == tarkan.color.y && pirate.color.z == tarkan.color.z);
    Assert::AreEqual(std::wstring(L"Models\\Tarkan\\Small.nmf"), Outpost::ModelFileName(pirate, pirate.Model("Small")));
    // The derelicts draw with the Human set's meshes in gray, by their hull's model (ADR-074).
    const Outpost::ModelSet& wrecks = *catalog.SetForDerelicts();
    Assert::AreEqual(std::string("Human"), wrecks.meshes);
    Assert::IsTrue(wrecks.color.x == wrecks.color.y, L"gray");
    const std::string* smallModel = catalog.ModelForHull(Outpost::HullId{1});
    Assert::IsNotNull(smallModel);
    Assert::AreEqual(std::string("Small"), *smallModel);
    Assert::IsNotNull(catalog.ModelForHull(Outpost::HullId{3}));
    Assert::IsNull(catalog.ModelForHull(Outpost::HullId{4}));

    // Every kind of structure and the Constructor has a model of its own name, drawn in its set's color.
    Assert::AreEqual(std::string("CommandStation"), catalog.ModelForStructure(Outpost::StructureKind::CommandStation)->model);
    Assert::AreEqual(std::string("Shipyard"), catalog.ModelForStructure(Outpost::StructureKind::Shipyard)->model);
    Assert::AreEqual(1.0f, catalog.ModelForStructure(Outpost::StructureKind::Shipyard)->tint);
    Assert::AreEqual(std::string("ResearchLab"), catalog.ModelForStructure(Outpost::StructureKind::ResearchLab)->model);
    Assert::AreEqual(std::string("MiningRig"), catalog.ModelForStructure(Outpost::StructureKind::MiningRig)->model);
    Assert::AreEqual(std::string("DefensePlatform"), catalog.ModelForStructure(Outpost::StructureKind::DefensePlatform)->model);
    // The Relay and the Repair Bay borrow a model and are told apart by their tint, until they have art (Phase 4 design §10).
    Assert::AreEqual(std::string("Shipyard"), catalog.ModelForStructure(Outpost::StructureKind::RepairBay)->model);
    Assert::IsTrue(catalog.ModelForStructure(Outpost::StructureKind::RepairBay)->tint < 1.0f);
    Assert::AreEqual(std::string("Constructor"), catalog.constructor);
  }

  TEST_METHOD(RejectsAMissingOrRepeatedStructure)
  {
    const std::string good = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
    const auto replaced = [&good](std::string_view _from, std::string_view _to)
    {
      std::string json = good;
      json.replace(json.find(_from), _from.size(), _to);
      return json;
    };
    (void)Outpost::LoadModelCatalog(good);
    ExpectRejected(replaced(R"({ "structure": "MiningRig", "model": "Small" }, )", ""));
    ExpectRejected(replaced(R"("structure": "MiningRig")", R"("structure": "Shipyard")"));
    ExpectRejected(replaced(R"("structure": "MiningRig")", R"("structure": "Factory")"));
    ExpectRejected(replaced(R"("tint": 0.5)", R"("tint": 3)"));
    // A model a player's set lacks.
    std::string json = replaced(R"("constructor": "Small")", R"("constructor": "Huge")");
    const std::string_view noPlayers = R"("players": [])";
    json.replace(json.find(noPlayers), noPlayers.size(), R"("players": [ { "player": 1, "set": "Human" } ])");
    ExpectRejected(json);
  }

  TEST_METHOD(RejectsAHullWhoseModelAPlayersSetLacks)
  {
    std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
    const std::string empty = R"("players": [], "hulls": [])";
    json.replace(json.find(empty), empty.size(),
                 R"("players": [ { "player": 1, "set": "Human" } ], "hulls": [ { "hull": 1, "model": "Huge" } ])");
    ExpectRejected(json);
  }

  TEST_METHOD(RejectsAPlayerOfAnUnknownSet)
  {
    std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
    const std::string empty = R"("players": [])";
    json.replace(json.find(empty), empty.size(), R"("players": [ { "player": 1, "set": "Martian" } ])");
    ExpectRejected(json);
  }

  // ADR-073: a set that borrows another's meshes names one that has meshes of its own, and the pirates' set is one there is.
  TEST_METHOD(RejectsABrokenPirateSet)
  {
    const std::string set =
      R"({ "name": "Human", "color": { "red": 0.5, "green": 0.5, "blue": 0.5 }, "models": [ { "name": "Small", "lengthMeters": 20 } ] })";
    const auto catalog = [&set](std::string_view _pirate, std::string_view _pirates)
    {
      std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
      json.replace(json.find(set), set.size(), std::format("{}, {}", set, _pirate));
      const std::string players = R"("players": [])";
      json.replace(json.find(players), players.size(), std::format(R"("players": [], "pirates": "{}")", _pirates));
      return json;
    };
    const std::string borrowing = R"({ "name": "Pirate", "color": { "red": 0.6, "green": 0.2, "blue": 0.9 }, "meshes": "Human" })";
    const Outpost::ModelCatalog loaded = Outpost::LoadModelCatalog(catalog(borrowing, "Pirate"));
    Assert::AreEqual(20.0f, loaded.SetForPlayer(Outpost::PIRATES)->Model("Small").lengthMeters);
    ExpectRejected(catalog(borrowing, "Corsair"));
    ExpectRejected(catalog(R"({ "name": "Pirate", "color": { "red": 0.6, "green": 0.2, "blue": 0.9 }, "meshes": "Martian" })", "Pirate"));
    ExpectRejected(catalog(R"({ "name": "Pirate", "color": { "red": 0.6, "green": 0.2, "blue": 0.9 }, "meshes": "Pirate" })", "Pirate"));
  }

  // Task 1.3's acceptance: the hulls load at their intended relative sizes in both sets, and the same hull is the same
  // size in both.
  TEST_METHOD(LoadsTheHullsAtTheirIntendedSizes)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    for (const char* setName : {"Human", "Tarkan"})
    {
      const Outpost::ModelSet& set = catalog.Set(setName);
      float previousLength = 0.0f;
      for (const char* hull : HULLS)
      {
        const Outpost::ModelEntry& model = set.Model(hull);
        const Neuron::MeshData mesh = ReadRepositoryModel(set, model);
        // Its length is along +x, its front (ADR-018), and it is the length the data asks for. Its front is where its guns
        // are: every gun stands ahead of every exhaust. A hull may be wider than it is long, as the Human Small's wings
        // make it.
        Assert::AreEqual(model.lengthMeters, mesh.Extents().x, TOLERANCE);
        float rearmostGun = std::numeric_limits<float>::infinity();
        float foremostExhaust = -std::numeric_limits<float>::infinity();
        for (const Neuron::MeshHardpoint& hardpoint : mesh.hardpoints)
        {
          const std::optional<Outpost::HardpointKind> kind = Outpost::HardpointKindOf(hardpoint.tag);
          if (kind == Outpost::HardpointKind::Gun)
            rearmostGun = std::min(rearmostGun, hardpoint.position.x);
          else if (kind == Outpost::HardpointKind::Exhaust)
            foremostExhaust = std::max(foremostExhaust, hardpoint.position.x);
        }
        Assert::IsTrue(rearmostGun > foremostExhaust, L"a gun stands behind an exhaust: the model's front is not +x");
        Assert::IsTrue(mesh.Extents().x > previousLength);
        previousLength = mesh.Extents().x;
      }
    }
  }

  TEST_METHOD(NamesEachModelsFile)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const Outpost::ModelSet& human = catalog.Set("Human");
    Assert::AreEqual(std::wstring(L"Models\\Human\\Small.nmf"), Outpost::ModelFileName(human, human.Model("Small")));
    // ADR-045: a model that grows is drawn at its first level until the game grows it.
    Assert::AreEqual(5, human.Model("ResearchLab").levels);
    Assert::AreEqual(std::wstring(L"Models\\Human\\ResearchLab_L1.nmf"), Outpost::ModelFileName(human, human.Model("ResearchLab")));
  }

  // Phase 3 design §4, gate K5: every level of a structure's model is drawn at level 1's size. A level after the first
  // stands within the wider of level 1's length and depth, whatever its own shape, and a structure is drawn at its own
  // level as far as its model has levels (ADR-064).
  TEST_METHOD(FitsEveryLevelOnTheFirstLevelsGround)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    for (const std::string_view setName : {"Human", "Tarkan"})
    {
      const Outpost::ModelSet& set = catalog.Set(setName);
      for (const std::string_view modelName : {"CommandStation", "Shipyard", "ResearchLab"})
      {
        const Outpost::ModelEntry& model = set.Model(modelName);
        Assert::AreEqual(5, Outpost::ModelLevels(model));
        const Neuron::MeshData first = ReadRepositoryModel(set, model);
        Assert::AreEqual(model.lengthMeters, first.Extents().x, TOLERANCE);
        const float widest = std::max(first.Extents().x, first.Extents().z);
        for (int level = 2; level <= Outpost::ModelLevels(model); ++level)
        {
          const Neuron::MeshData mesh = ReadRepositoryModel(set, model, level, widest);
          const std::string narrow = std::format("{} {} level {}", setName, modelName, level);
          const std::wstring where(narrow.begin(), narrow.end());
          Assert::AreEqual(widest, std::max(mesh.Extents().x, mesh.Extents().z), TOLERANCE, where.c_str());
        }
      }
    }
    const Outpost::ModelSet& human = catalog.Set("Human");
    Assert::AreEqual(std::wstring(L"Models\\Human\\Shipyard_L3.nmf"), Outpost::ModelFileName(human, human.Model("Shipyard"), 3));
    Assert::AreEqual(std::wstring(L"Models\\Human\\Small.nmf"), Outpost::ModelFileName(human, human.Model("Small"), 3), L"no levels");
    Assert::AreEqual(3, Outpost::DrawnLevel(human.Model("Shipyard"), 3));
    Assert::AreEqual(5, Outpost::DrawnLevel(human.Model("Shipyard"), 7));
    Assert::AreEqual(1, Outpost::DrawnLevel(human.Model("Shipyard"), 0));
    Assert::AreEqual(1, Outpost::DrawnLevel(human.Model("Small"), 3));
  }

  TEST_METHOD(RejectsLevelsOutOfRange)
  {
    (void)Outpost::LoadModelCatalog(OneModel("Human", R"({ "name": "Small", "lengthMeters": 20, "levels": 9 })", GOOD_COLOR));
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "lengthMeters": 20, "levels": 0 })", GOOD_COLOR));
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "lengthMeters": 20, "levels": 10 })", GOOD_COLOR));
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "lengthMeters": 20, "levels": 2.5 })", GOOD_COLOR));
  }

  // A model's front is in its mesh since ADR-018, so the data no longer says it.
  TEST_METHOD(RejectsAForwardAxis)
  {
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "forwardAxis": "+x", "lengthMeters": 20 })", GOOD_COLOR));
  }

  // ADR-019: a warship's exhaust is its drive's color, and a Constructor's is its own; a drive listed twice is refused.
  TEST_METHOD(ColorsEachExhaustByItsDrive)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    Outpost::EntityView ship{.kind = Outpost::EntityKind::Ship, .drive = Outpost::DriveId{1}};
    const DirectX::XMFLOAT4* ion = catalog.ExhaustColor(ship);
    ship.drive = Outpost::DriveId{2};
    const DirectX::XMFLOAT4* fusion = catalog.ExhaustColor(ship);
    ship.drive = Outpost::DriveId{3};
    const DirectX::XMFLOAT4* pulse = catalog.ExhaustColor(ship);
    Assert::IsTrue(ion != nullptr && fusion != nullptr && pulse != nullptr && ion != fusion && pulse != ion && pulse != fusion);
    ship.drive = Outpost::DriveId{9};
    Assert::IsNull(catalog.ExhaustColor(ship));
    ship.role = Outpost::ShipRole::Constructor;
    Assert::IsTrue(catalog.ExhaustColor(ship) == &catalog.constructorExhaust);
    Assert::IsNull(catalog.ExhaustColor({.kind = Outpost::EntityKind::Structure}));

    std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
    const std::string_view one = R"([ { "drive": 1, "color": { "red": 0.3, "green": 0.85, "blue": 1 } } ])";
    json.replace(
      json.find(one), one.size(),
      R"([ { "drive": 1, "color": { "red": 0.3, "green": 0.85, "blue": 1 } }, { "drive": 1, "color": { "red": 1, "green": 0, "blue": 0 } } ])");
    ExpectRejected(json);
  }

  // ADR-034: the Lance fires a beam and the Rail Cannon a slug; every other weapon fires tracers. A look the file does
  // not know, or a weapon listed twice, is refused.
  TEST_METHOD(GivesEachWeaponItsShot)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{1}) == Outpost::ShotLook::Tracer);
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{2}) == Outpost::ShotLook::Beam);
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{3}) == Outpost::ShotLook::Tracer);
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{4}) == Outpost::ShotLook::Tracer);
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{5}) == Outpost::ShotLook::Slug);
    Assert::IsTrue(catalog.ShotLookOf(Outpost::WeaponId{9}) == Outpost::ShotLook::Tracer);

    const std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
    const std::string_view one = R"([ { "weapon": 2, "look": "beam" } ])";
    Assert::IsTrue(Outpost::LoadModelCatalog(json).ShotLookOf(Outpost::WeaponId{2}) == Outpost::ShotLook::Beam);
    for (const std::string_view shots :
         {R"([ { "weapon": 2, "look": "laser" } ])", R"([ { "weapon": 2, "look": "beam" }, { "weapon": 2, "look": "slug" } ])"})
    {
      std::string broken = json;
      broken.replace(broken.find(one), one.size(), shots);
      ExpectRejected(broken);
    }
  }

  // ADR-029: every hull and the Constructor bank, a Small hull harder and faster than a Large one; anything that is not
  // a ship does not.
  TEST_METHOD(GivesEveryShipABank)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    Outpost::EntityView ship{.kind = Outpost::EntityKind::Ship, .hull = Outpost::HullId{1}};
    const Outpost::BankLimits* smallHull = catalog.BankFor(ship);
    ship.hull = Outpost::HullId{3};
    const Outpost::BankLimits* largeHull = catalog.BankFor(ship);
    Assert::IsTrue(smallHull != nullptr && largeHull != nullptr);
    Assert::IsTrue(smallHull->maxBankRadians > largeHull->maxBankRadians);
    Assert::IsTrue(smallHull->settleSeconds < largeHull->settleSeconds);
    for (const Outpost::HullModel& hull : catalog.hulls)
      Assert::IsTrue(hull.bank.maxBankRadians > 0.0f && hull.bank.fullBankMetersPerSecondSquared > 0.0f && hull.bank.settleSeconds > 0.0f);
    ship.role = Outpost::ShipRole::Constructor;
    Assert::IsTrue(catalog.BankFor(ship) == &catalog.constructorBank);
    Assert::IsTrue(catalog.constructorBank.maxBankRadians > 0.0f);
    Assert::IsNull(catalog.BankFor({.kind = Outpost::EntityKind::Structure}));
  }

  // A bank is optional, and a ship without one flies level; one too steep, or missing a member, is refused.
  TEST_METHOD(ReadsAnOptionalBank)
  {
    const Outpost::ModelCatalog level = Outpost::LoadModelCatalog(OneModel("Human", GOOD_MODEL, GOOD_COLOR));
    Assert::AreEqual(0.0f, level.constructorBank.maxBankRadians);

    const auto withBank = [](std::string_view _bank)
    {
      std::string json = OneModel("Human", GOOD_MODEL, GOOD_COLOR);
      const std::string_view constructor = R"("constructor": "Small",)";
      json.replace(json.find(constructor), constructor.size(), std::format(R"("constructor": "Small", "constructorBank": {},)", _bank));
      return json;
    };
    const Outpost::ModelCatalog banked =
      Outpost::LoadModelCatalog(withBank(R"({ "maxDegrees": 30, "fullAtMetersPerSecondSquared": 100, "settleSeconds": 0.2 })"));
    Assert::AreEqual(std::numbers::pi_v<float> / 6.0f, banked.constructorBank.maxBankRadians, TOLERANCE);
    Assert::AreEqual(100.0f, banked.constructorBank.fullBankMetersPerSecondSquared);
    Assert::AreEqual(0.2f, banked.constructorBank.settleSeconds);

    ExpectRejected(withBank(R"({ "maxDegrees": 75, "fullAtMetersPerSecondSquared": 100, "settleSeconds": 0.2 })"));
    ExpectRejected(withBank(R"({ "maxDegrees": 30, "settleSeconds": 0.2 })"));
    ExpectRejected(withBank(R"({ "maxDegrees": 30, "fullAtMetersPerSecondSquared": 100, "settleSeconds": 0 })"));
  }

  TEST_METHOD(RejectsANameThatIsNotAFileName)
  {
    ExpectRejected(OneModel("Hu man", GOOD_MODEL, GOOD_COLOR));
    ExpectRejected(OneModel("Human", R"({ "name": "..", "lengthMeters": 20 })", GOOD_COLOR));
  }

  TEST_METHOD(RejectsAColorChannelAboveOne)
  {
    ExpectRejected(OneModel("Human", GOOD_MODEL, R"({ "red": 1.5, "green": 0.5, "blue": 0.5 })"));
  }

  TEST_METHOD(RejectsAModelListedTwice)
  {
    ExpectRejected(OneModel("Human", std::format("{}, {}", GOOD_MODEL, GOOD_MODEL), GOOD_COLOR));
  }

  TEST_METHOD(RejectsANonPositiveLength)
  {
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "lengthMeters": 0 })", GOOD_COLOR));
  }

  TEST_METHOD(RejectsAnUnknownMember)
  {
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "lengthMeters": 20, "scale": 2 })", GOOD_COLOR));
  }

  TEST_METHOD(AcceptsAWellFormedCatalog)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(OneModel("Human", GOOD_MODEL, GOOD_COLOR));
    Assert::AreEqual(20.0f, catalog.Set("Human").Model("Small").lengthMeters);
  }
};
} // namespace GameAppTests
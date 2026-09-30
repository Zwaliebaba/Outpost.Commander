#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr float TOLERANCE = 1e-3f;
constexpr std::array<const char*, 3> HULLS{"Small", "Medium", "Large"};

// A catalog of one set of one model, with one member replaced, for the loader's error cases.
std::string OneModel(std::string_view _setName, std::string_view _model, std::string_view _color)
{
  return std::format(R"({{ "sets": [ {{ "name": "{}", "color": {}, "models": [ {} ] }} ], "players": [], "hulls": [] }})", _setName, _color,
                     _model);
}

constexpr std::string_view GOOD_MODEL = R"({ "name": "Small", "forwardAxis": "+x", "lengthMeters": 20 })";
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
    Assert::AreEqual(size_t{1}, catalog.Set("Asteroids").models.size());

    // The two sides have the same models, so that every design can be drawn for either (design §11).
    Assert::AreEqual(human.models.size(), tarkan.models.size());
    for (const Outpost::ModelEntry& model : human.models)
      Assert::AreEqual(model.lengthMeters, tarkan.Model(model.name).lengthMeters);

    // The player draws with the Human set and the AI with the Tarkan set, and each hull of the tuning data has a model.
    Assert::IsTrue(catalog.SetForPlayer(Outpost::PlayerId{1}) == &human);
    Assert::IsTrue(catalog.SetForPlayer(Outpost::PlayerId{2}) == &tarkan);
    Assert::IsNull(catalog.SetForPlayer(Outpost::PlayerId{3}));
    const std::string* smallModel = catalog.ModelForHull(Outpost::HullId{1});
    Assert::IsNotNull(smallModel);
    Assert::AreEqual(std::string("Small"), *smallModel);
    Assert::IsNotNull(catalog.ModelForHull(Outpost::HullId{3}));
    Assert::IsNull(catalog.ModelForHull(Outpost::HullId{4}));
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

  // Task 1.3's acceptance: the hulls load at their intended relative sizes in both sets, and the same hull is the same
  // size in both. Before scaling the Tarkan Medium is longer than the Tarkan Large (design §11).
  TEST_METHOD(LoadsTheHullsAtTheirIntendedSizes)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const Outpost::ModelSet& tarkan = catalog.Set("Tarkan");
    const Neuron::MeshData rawMedium = Neuron::ParseCmo(ReadRepositoryAsset("Models\\Tarkan\\Medium.cmo"), "Medium.cmo");
    const Neuron::MeshData rawLarge = Neuron::ParseCmo(ReadRepositoryAsset("Models\\Tarkan\\Large.cmo"), "Large.cmo");
    Assert::IsTrue(rawMedium.Extents().x > rawLarge.Extents().x);
    Assert::IsTrue(tarkan.Model("Medium").lengthMeters < tarkan.Model("Large").lengthMeters);

    for (const char* setName : {"Human", "Tarkan"})
    {
      const Outpost::ModelSet& set = catalog.Set(setName);
      float previousLength = 0.0f;
      for (const char* hull : HULLS)
      {
        const Outpost::ModelEntry& model = set.Model(hull);
        const Neuron::MeshData mesh = ReadRepositoryModel(set, model);
        // Its length is along +x once oriented, and it is the length the data asks for.
        Assert::AreEqual(model.lengthMeters, mesh.Extents().x, TOLERANCE);
        Assert::IsTrue(mesh.Extents().x > mesh.Extents().z);
        Assert::IsTrue(mesh.Extents().x > previousLength);
        previousLength = mesh.Extents().x;
      }
    }
  }

  TEST_METHOD(NamesEachModelsFile)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const Outpost::ModelSet& human = catalog.Set("Human");
    Assert::AreEqual(std::wstring(L"Models\\Human\\Small.cmo"), Outpost::ModelFileName(human, human.Model("Small")));
  }

  TEST_METHOD(RejectsAnUnknownAxis)
  {
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "forwardAxis": "+y", "lengthMeters": 20 })", GOOD_COLOR));
  }

  TEST_METHOD(RejectsANameThatIsNotAFileName)
  {
    ExpectRejected(OneModel("Hu man", GOOD_MODEL, GOOD_COLOR));
    ExpectRejected(OneModel("Human", R"({ "name": "..", "forwardAxis": "+x", "lengthMeters": 20 })", GOOD_COLOR));
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
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "forwardAxis": "+x", "lengthMeters": 0 })", GOOD_COLOR));
  }

  TEST_METHOD(RejectsAnUnknownMember)
  {
    ExpectRejected(OneModel("Human", R"({ "name": "Small", "forwardAxis": "+x", "lengthMeters": 20, "scale": 2 })", GOOD_COLOR));
  }

  TEST_METHOD(AcceptsAWellFormedCatalog)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(OneModel("Human", GOOD_MODEL, GOOD_COLOR));
    Assert::AreEqual(20.0f, catalog.Set("Human").Model("Small").lengthMeters);
  }
};
} // namespace GameAppTests

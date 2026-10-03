#include "pch.h"
#include "ModelCatalog.h"

#include <algorithm>
#include <array>
#include <numbers>

namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

// A set's or a model's name, which is also a folder or a file name under Assets\Models, so letters and digits only.
std::string ReadName(JsonObjectReader& _reader)
{
  std::string name = _reader.String("name");
  if (name.empty() ||
      !std::ranges::all_of(name, [](char _c) { return (_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z') || (_c >= '0' && _c <= '9'); }))
    Neuron::JsonFail(_reader.PathOf("name"), std::format("\"{}\" is not a name of ASCII letters and digits", name));
  return name;
}

float ReadChannel(JsonObjectReader& _reader, std::string_view _name)
{
  const double value = _reader.Number(_name, JsonBound::NotNegative);
  if (value > 1.0)
    Neuron::JsonFail(_reader.PathOf(_name), std::format("a linear color channel is at most 1, found {}", value));
  return static_cast<float>(value);
}

Outpost::ModelEntry ReadModel(JsonObjectReader& _reader)
{
  Outpost::ModelEntry model;
  model.name = ReadName(_reader);
  model.lengthMeters = static_cast<float>(_reader.Number("lengthMeters", JsonBound::Positive));
  return model;
}

// A linear color of three channels, each from 0 to 1.
DirectX::XMFLOAT4 ReadColor(JsonObjectReader& _reader, std::string_view _name)
{
  JsonObjectReader color(_reader.Required(_name), _reader.PathOf(_name));
  const DirectX::XMFLOAT4 value{ReadChannel(color, "red"), ReadChannel(color, "green"), ReadChannel(color, "blue"), 1.0f};
  color.Finish();
  return value;
}

Outpost::ModelSet ReadSet(JsonObjectReader& _reader)
{
  Outpost::ModelSet set;
  set.name = ReadName(_reader);
  set.color = ReadColor(_reader, "color");

  set.models = Neuron::ReadJsonList<Outpost::ModelEntry>(_reader, "models", ReadModel);
  for (size_t i = 0; i < set.models.size(); ++i)
  {
    const auto first = std::ranges::find(set.models, set.models[i].name, &Outpost::ModelEntry::name);
    if (first != set.models.begin() + static_cast<std::ptrdiff_t>(i))
    {
      Neuron::JsonFail(Neuron::JsonElementPath(_reader.PathOf("models"), i),
                       std::format("the model \"{}\" is listed twice", set.models[i].name));
    }
  }
  return set;
}

Outpost::PlayerModels ReadPlayer(JsonObjectReader& _reader)
{
  return {.player = _reader.Identifier<Outpost::PlayerId>("player"), .set = _reader.String("set")};
}

// A bank steeper than this would stand a ship on its side, which reads as a roll, not a turn.
constexpr double MAXIMUM_BANK_DEGREES = 60.0;
constexpr double RADIANS_PER_DEGREE = std::numbers::pi / 180.0;

// How a ship banks (ADR-029), from the optional member _name; all zero, flying level, when it is absent.
Outpost::BankLimits ReadBank(JsonObjectReader& _reader, std::string_view _name)
{
  const Neuron::JsonValue* value = _reader.Optional(_name);
  if (value == nullptr)
    return {};
  JsonObjectReader bank(*value, _reader.PathOf(_name));
  const double degrees = bank.Number("maxDegrees", JsonBound::Positive);
  if (degrees > MAXIMUM_BANK_DEGREES)
    Neuron::JsonFail(bank.PathOf("maxDegrees"), std::format("a bank is at most {} degrees, found {}", MAXIMUM_BANK_DEGREES, degrees));
  const Outpost::BankLimits limits{.maxBankRadians = static_cast<float>(degrees * RADIANS_PER_DEGREE),
                                   .fullBankMetersPerSecondSquared =
                                     static_cast<float>(bank.Number("fullAtMetersPerSecondSquared", JsonBound::Positive)),
                                   .settleSeconds = static_cast<float>(bank.Number("settleSeconds", JsonBound::Positive))};
  bank.Finish();
  return limits;
}

Outpost::HullModel ReadHull(JsonObjectReader& _reader)
{
  Outpost::HullModel hull{.hull = _reader.Identifier<Outpost::HullId>("hull"), .model = _reader.String("model")};
  hull.bank = ReadBank(_reader, "bank");
  return hull;
}

Outpost::DriveExhaust ReadExhaust(JsonObjectReader& _reader)
{
  return {.drive = _reader.Identifier<Outpost::DriveId>("drive"), .color = ReadColor(_reader, "color")};
}

// The file spells a look as its enumerator does, in lower case.
constexpr std::array<std::pair<std::string_view, Outpost::ShotLook>, 3> SHOT_LOOKS = {{
  {"tracer", Outpost::ShotLook::Tracer},
  {"beam", Outpost::ShotLook::Beam},
  {"slug", Outpost::ShotLook::Slug},
}};

Outpost::WeaponShot ReadShot(JsonObjectReader& _reader)
{
  const Outpost::WeaponId weapon = _reader.Identifier<Outpost::WeaponId>("weapon");
  const std::string look = _reader.String("look");
  const auto found = std::ranges::find(SHOT_LOOKS, look, &std::pair<std::string_view, Outpost::ShotLook>::first);
  if (found == SHOT_LOOKS.end())
    Neuron::JsonFail(_reader.PathOf("look"), std::format("\"{}\" is not a look: tracer, beam or slug", look));
  return {.weapon = weapon, .look = found->second};
}

// The file spells a kind as its enumerator, as the tuning data does.
constexpr std::array<std::pair<std::string_view, Outpost::StructureKind>, 5> STRUCTURE_KINDS = {{
  {"CommandStation", Outpost::StructureKind::CommandStation},
  {"Shipyard", Outpost::StructureKind::Shipyard},
  {"ResearchLab", Outpost::StructureKind::ResearchLab},
  {"MiningRig", Outpost::StructureKind::MiningRig},
  {"DefensePlatform", Outpost::StructureKind::DefensePlatform},
}};
// A tint brighter than this would wash a set's color out to white.
constexpr double MAXIMUM_TINT = 2.0;

Outpost::StructureModel ReadStructure(JsonObjectReader& _reader)
{
  const std::string kind = _reader.String("structure");
  const auto found = std::ranges::find(STRUCTURE_KINDS, kind, &std::pair<std::string_view, Outpost::StructureKind>::first);
  if (found == STRUCTURE_KINDS.end())
    Neuron::JsonFail(_reader.PathOf("structure"), std::format("\"{}\" is not a structure kind", kind));
  Outpost::StructureModel structure{.structure = found->second, .model = _reader.String("model")};
  if (const Neuron::JsonValue* tint = _reader.Optional("tint"))
  {
    const double value = Neuron::ReadJsonNumber(*tint, _reader.PathOf("tint"), JsonBound::Positive);
    if (value > MAXIMUM_TINT)
      Neuron::JsonFail(_reader.PathOf("tint"), std::format("a tint is at most {}, found {}", MAXIMUM_TINT, value));
    structure.tint = static_cast<float>(value);
  }
  return structure;
}

// The index of the first element before _index whose _key equals element _index's, or _index when there is none.
template <typename T, typename Key> size_t FirstWithSameKey(const std::vector<T>& _list, size_t _index, Key T::*_key)
{
  for (size_t i = 0; i < _index; ++i)
  {
    if (_list[i].*_key == _list[_index].*_key)
      return i;
  }
  return _index;
}
} // namespace

const Outpost::ModelEntry& Outpost::ModelSet::Model(std::string_view _name) const
{
  const auto found = std::ranges::find(models, _name, &ModelEntry::name);
  if (found == models.end())
    throw Neuron::Exception(std::format("The model set \"{}\" has no model \"{}\".", name, _name));
  return *found;
}

const Outpost::ModelSet& Outpost::ModelCatalog::Set(std::string_view _name) const
{
  const auto found = std::ranges::find(sets, _name, &ModelSet::name);
  if (found == sets.end())
    throw Neuron::Exception(std::format("There is no model set \"{}\".", _name));
  return *found;
}

Outpost::ModelCatalog Outpost::LoadModelCatalog(std::string_view _json)
{
  const Neuron::JsonValue document = Neuron::ParseJson(_json);
  JsonObjectReader reader(document, "");
  ModelCatalog catalog;
  catalog.sets = Neuron::ReadJsonList<ModelSet>(reader, "sets", ReadSet);
  catalog.players = Neuron::ReadJsonList<PlayerModels>(reader, "players", ReadPlayer);
  catalog.hulls = Neuron::ReadJsonList<HullModel>(reader, "hulls", ReadHull);
  catalog.structures = Neuron::ReadJsonList<StructureModel>(reader, "structures", ReadStructure);
  catalog.constructor = reader.String("constructor");
  catalog.exhausts = Neuron::ReadJsonList<DriveExhaust>(reader, "exhausts", ReadExhaust);
  catalog.constructorExhaust = ReadColor(reader, "constructorExhaust");
  catalog.shots = Neuron::ReadJsonList<WeaponShot>(reader, "shots", ReadShot);
  catalog.constructorBank = ReadBank(reader, "constructorBank");
  reader.Finish();

  for (size_t i = 0; i < catalog.sets.size(); ++i)
  {
    const auto first = std::ranges::find(catalog.sets, catalog.sets[i].name, &ModelSet::name);
    if (first != catalog.sets.begin() + static_cast<std::ptrdiff_t>(i))
      Neuron::JsonFail(Neuron::JsonElementPath("sets", i), std::format("the set \"{}\" is listed twice", catalog.sets[i].name));
  }

  for (size_t i = 0; i < catalog.players.size(); ++i)
  {
    const std::string path = Neuron::JsonElementPath("players", i);
    if (FirstWithSameKey(catalog.players, i, &PlayerModels::player) != i)
      Neuron::JsonFail(path, std::format("player {} is listed twice", catalog.players[i].player.value));
    if (std::ranges::find(catalog.sets, catalog.players[i].set, &ModelSet::name) == catalog.sets.end())
      Neuron::JsonFail(path + ".set", std::format("there is no set \"{}\"", catalog.players[i].set));
  }
  for (size_t i = 0; i < catalog.hulls.size(); ++i)
  {
    const std::string path = Neuron::JsonElementPath("hulls", i);
    if (FirstWithSameKey(catalog.hulls, i, &HullModel::hull) != i)
      Neuron::JsonFail(path, std::format("hull {} is listed twice", catalog.hulls[i].hull.value));
    for (const PlayerModels& player : catalog.players)
    {
      const ModelSet& set = catalog.Set(player.set);
      if (std::ranges::find(set.models, catalog.hulls[i].model, &ModelEntry::name) == set.models.end())
        Neuron::JsonFail(path + ".model", std::format("the set \"{}\" has no model \"{}\"", set.name, catalog.hulls[i].model));
    }
  }

  for (size_t i = 0; i < catalog.exhausts.size(); ++i)
  {
    if (FirstWithSameKey(catalog.exhausts, i, &DriveExhaust::drive) != i)
      Neuron::JsonFail(Neuron::JsonElementPath("exhausts", i), std::format("drive {} is listed twice", catalog.exhausts[i].drive.value));
  }
  for (size_t i = 0; i < catalog.shots.size(); ++i)
  {
    if (FirstWithSameKey(catalog.shots, i, &WeaponShot::weapon) != i)
      Neuron::JsonFail(Neuron::JsonElementPath("shots", i), std::format("weapon {} is listed twice", catalog.shots[i].weapon.value));
  }

  // A model every player's set must have, for drawing whichever player owns it.
  const auto checkInEverySet = [&catalog](const std::string& _model, const std::string& _path)
  {
    for (const PlayerModels& player : catalog.players)
    {
      const ModelSet& set = catalog.Set(player.set);
      if (std::ranges::find(set.models, _model, &ModelEntry::name) == set.models.end())
        Neuron::JsonFail(_path, std::format("the set \"{}\" has no model \"{}\"", set.name, _model));
    }
  };
  for (size_t i = 0; i < catalog.structures.size(); ++i)
  {
    const std::string path = Neuron::JsonElementPath("structures", i);
    if (FirstWithSameKey(catalog.structures, i, &StructureModel::structure) != i)
      Neuron::JsonFail(path + ".structure", "the kind is listed twice");
    checkInEverySet(catalog.structures[i].model, path + ".model");
  }
  for (const auto& [name, kind] : STRUCTURE_KINDS)
  {
    if (catalog.ModelForStructure(kind) == nullptr)
      Neuron::JsonFail("structures", std::format("has no {}", name));
  }
  checkInEverySet(catalog.constructor, "constructor");
  return catalog;
}

const Outpost::ModelSet* Outpost::ModelCatalog::SetForPlayer(PlayerId _player) const noexcept
{
  const auto found = std::ranges::find(players, _player, &PlayerModels::player);
  if (found == players.end())
    return nullptr;
  const auto set = std::ranges::find(sets, found->set, &ModelSet::name);
  return set == sets.end() ? nullptr : &*set;
}

const std::string* Outpost::ModelCatalog::ModelForHull(HullId _hull) const noexcept
{
  const auto found = std::ranges::find(hulls, _hull, &HullModel::hull);
  return found == hulls.end() ? nullptr : &found->model;
}

const Outpost::StructureModel* Outpost::ModelCatalog::ModelForStructure(StructureKind _structure) const noexcept
{
  const auto found = std::ranges::find(structures, _structure, &StructureModel::structure);
  return found == structures.end() ? nullptr : &*found;
}

const DirectX::XMFLOAT4* Outpost::ModelCatalog::ExhaustColor(const EntityView& _entity) const noexcept
{
  if (_entity.kind != EntityKind::Ship)
    return nullptr;
  if (_entity.role == ShipRole::Constructor)
    return &constructorExhaust;
  const auto found = std::ranges::find(exhausts, _entity.drive, &DriveExhaust::drive);
  return found == exhausts.end() ? nullptr : &found->color;
}

Outpost::ShotLook Outpost::ModelCatalog::ShotLookOf(WeaponId _weapon) const noexcept
{
  const auto found = std::ranges::find(shots, _weapon, &WeaponShot::weapon);
  return found == shots.end() ? ShotLook::Tracer : found->look;
}

const Outpost::BankLimits* Outpost::ModelCatalog::BankFor(const EntityView& _entity) const noexcept
{
  if (_entity.kind != EntityKind::Ship)
    return nullptr;
  if (_entity.role == ShipRole::Constructor)
    return &constructorBank;
  const auto found = std::ranges::find(hulls, _entity.hull, &HullModel::hull);
  return found == hulls.end() ? nullptr : &found->bank;
}

std::wstring Outpost::ModelFileName(const ModelSet& _set, const ModelEntry& _model)
{
  // The loader allows only ASCII letters and digits in names, so widening them character by character is exact.
  const std::string name = std::format("Models\\{}\\{}.nmf", _set.name, _model.name);
  return {name.begin(), name.end()};
}

Neuron::MeshData Outpost::BuildModelMesh(std::span<const std::uint8_t> _nmfBytes, const ModelEntry& _model, std::string_view _fileName)
{
  Neuron::MeshData mesh = Neuron::ParseNmf(_nmfBytes, _fileName);
  for (const Neuron::MeshHardpoint& hardpoint : mesh.hardpoints)
  {
    if (!HardpointKindOf(hardpoint.tag).has_value())
      throw Neuron::Exception(
        std::format("The mesh {} has a hardpoint tagged \"{}\", which the game does not know.", _fileName, hardpoint.tag));
  }
  Neuron::FitMesh(mesh, _model.lengthMeters);
  return mesh;
}
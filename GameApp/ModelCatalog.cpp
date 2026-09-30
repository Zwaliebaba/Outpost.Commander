#include "pch.h"
#include "ModelCatalog.h"

#include <algorithm>

namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

Neuron::MeshAxis ReadAxis(JsonObjectReader& _reader)
{
  const std::string axis = _reader.String("forwardAxis");
  if (axis == "+x")
    return Neuron::MeshAxis::PositiveX;
  if (axis == "-x")
    return Neuron::MeshAxis::NegativeX;
  if (axis == "+z")
    return Neuron::MeshAxis::PositiveZ;
  if (axis == "-z")
    return Neuron::MeshAxis::NegativeZ;
  Neuron::JsonFail(_reader.PathOf("forwardAxis"), std::format("\"{}\" is not \"+x\", \"-x\", \"+z\" or \"-z\"", axis));
}

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
  model.forward = ReadAxis(_reader);
  model.lengthMeters = static_cast<float>(_reader.Number("lengthMeters", JsonBound::Positive));
  return model;
}

Outpost::ModelSet ReadSet(JsonObjectReader& _reader)
{
  Outpost::ModelSet set;
  set.name = ReadName(_reader);

  JsonObjectReader color(_reader.Required("color"), _reader.PathOf("color"));
  set.color = {ReadChannel(color, "red"), ReadChannel(color, "green"), ReadChannel(color, "blue"), 1.0f};
  color.Finish();

  set.models = Neuron::ReadJsonList<Outpost::ModelEntry>(_reader, "models", ReadModel);
  for (size_t i = 0; i < set.models.size(); ++i)
  {
    const auto first = std::ranges::find(set.models, set.models[i].name, &Outpost::ModelEntry::name);
    if (first != set.models.begin() + static_cast<std::ptrdiff_t>(i))
      Neuron::JsonFail(Neuron::JsonElementPath(_reader.PathOf("models"), i),
                       std::format("the model \"{}\" is listed twice", set.models[i].name));
  }
  return set;
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
  reader.Finish();

  for (size_t i = 0; i < catalog.sets.size(); ++i)
  {
    const auto first = std::ranges::find(catalog.sets, catalog.sets[i].name, &ModelSet::name);
    if (first != catalog.sets.begin() + static_cast<std::ptrdiff_t>(i))
      Neuron::JsonFail(Neuron::JsonElementPath("sets", i), std::format("the set \"{}\" is listed twice", catalog.sets[i].name));
  }
  return catalog;
}

std::wstring Outpost::ModelFileName(const ModelSet& _set, const ModelEntry& _model)
{
  // The loader allows only ASCII letters and digits in names, so widening them character by character is exact.
  const std::string name = std::format("Models\\{}\\{}.cmo", _set.name, _model.name);
  return {name.begin(), name.end()};
}

Neuron::MeshData Outpost::BuildModelMesh(std::span<const std::uint8_t> _cmoBytes, const ModelEntry& _model, std::string_view _fileName)
{
  Neuron::MeshData mesh = Neuron::ParseCmo(_cmoBytes, _fileName);
  Neuron::OrientMesh(mesh, _model.forward, _model.lengthMeters);
  return mesh;
}

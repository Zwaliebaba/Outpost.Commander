#include "pch.h"
#include "Matchup.h"

namespace
{
template <typename Id, typename Component> void Known(const std::vector<Component>& _components, Id _id, std::string_view _path)
{
  if (std::ranges::find(_components, _id, &Component::id) == _components.end())
    Neuron::JsonFail(_path, "expected a component of the tuning data");
}
} // namespace

std::vector<Outpost::Matchup> Outpost::LoadMatchups(std::string_view _json, const Tuning& _tuning)
{
  try
  {
    const Neuron::JsonValue document = Neuron::ParseJson(_json);
    Neuron::JsonObjectReader reader(document, "");
    std::vector<Matchup> matchups = Neuron::ReadJsonList<Matchup>(
      reader, "matchups",
      [&_tuning](Neuron::JsonObjectReader& _matchup)
      {
        Matchup matchup{.name = _matchup.String("name"),
                        .distanceMeters = static_cast<float>(_matchup.Number("distanceMeters", Neuron::JsonBound::Positive))};
        const std::string sidesPath = _matchup.PathOf("sides");
        const Neuron::JsonValue::Array& sides = Neuron::ReadJsonArray(_matchup.Required("sides"), sidesPath);
        if (sides.size() != matchup.sides.size())
          Neuron::JsonFail(sidesPath, "expected two sides");
        for (std::size_t side = 0; side < sides.size(); ++side)
        {
          const std::string sidePath = Neuron::JsonElementPath(sidesPath, side);
          for (std::size_t group = 0; const Neuron::JsonValue& element : Neuron::ReadJsonArray(sides[side], sidePath))
          {
            Neuron::JsonObjectReader groupReader(element, Neuron::JsonElementPath(sidePath, group++));
            MatchupGroup ships{.hull = groupReader.Identifier<HullId>("hull"),
                               .drive = groupReader.Identifier<DriveId>("drive"),
                               .weapon = groupReader.Identifier<WeaponId>("weapon"),
                               .count = groupReader.Integer("count", 1)};
            Known(_tuning.hulls, ships.hull, groupReader.PathOf("hull"));
            Known(_tuning.drives, ships.drive, groupReader.PathOf("drive"));
            Known(_tuning.weapons, ships.weapon, groupReader.PathOf("weapon"));
            if (groupReader.Optional("module") != nullptr)
            {
              ships.module = groupReader.Identifier<ModuleId>("module");
              Known(_tuning.modules, ships.module, groupReader.PathOf("module"));
            }
            groupReader.Finish();
            matchup.sides[side].push_back(ships);
          }
          if (matchup.sides[side].empty())
            Neuron::JsonFail(sidePath, "expected a group of ships at least");
        }
        return matchup;
      });
    reader.Finish();
    if (matchups.empty())
      Neuron::JsonFail("matchups", "expected a matchup at least");
    return matchups;
  }
  catch (const Neuron::Exception& exception)
  {
    throw Neuron::Exception(std::format("Matchups.json: {}", exception.what()));
  }
}

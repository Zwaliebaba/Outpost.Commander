#include "pch.h"
#include "AiSettings.h"

namespace
{
using Neuron::JsonBound;
using ObjectReader = Neuron::JsonObjectReader;

Outpost::DesignComponents ReadComponents(ObjectReader& _parent, std::string_view _name)
{
  ObjectReader reader(_parent.Required(_name), _parent.PathOf(_name));
  Outpost::DesignComponents components{.hull = reader.Identifier<Outpost::HullId>("hull"),
                                       .drive = reader.Identifier<Outpost::DriveId>("drive"),
                                       .weapon = reader.Identifier<Outpost::WeaponId>("weapon")};
  // A module is optional, as in a design (Phase 2 design §10).
  if (reader.Optional("module") != nullptr)
    components.module = reader.Identifier<Outpost::ModuleId>("module");
  reader.Finish();
  return components;
}

Outpost::CounterRule ReadCounter(ObjectReader& _reader)
{
  Outpost::CounterRule rule;
  rule.enemy = ReadComponents(_reader, "enemy");
  rule.answer = ReadComponents(_reader, "answer");
  return rule;
}

Outpost::AiSettings ReadAiSettings(std::string_view _json)
{
  const Neuron::JsonValue document = Neuron::ParseJson(_json);
  ObjectReader root(document, "");

  Outpost::AiSettings settings;
  settings.attackGroupShips = root.Integer("attackGroupShips", 1);
  settings.attackGroupGrowthPerTier = root.Integer("attackGroupGrowthPerTier", 0);
  settings.retreatLossShare = root.Number("retreatLossShare", JsonBound::NotNegative);
  if (settings.retreatLossShare >= 1.0)
    Neuron::JsonFail(root.PathOf("retreatLossShare"), "a share below 1, since a group that has lost every ship has none to bring back");
  settings.regroupSeconds = root.Number("regroupSeconds", JsonBound::NotNegative);
  settings.reviewIntervalSeconds = root.Number("reviewIntervalSeconds", JsonBound::Positive);
  settings.constructors = root.Integer("constructors", 1);
  settings.incomePerShipyardOrePerSecond = root.Number("incomePerShipyardOrePerSecond", JsonBound::Positive);
  settings.homeAsteroids = root.Integer("homeAsteroids", 0);
  settings.contestedAsteroids = root.Integer("contestedAsteroids", 0);
  settings.homePlatformsPerShipyard = root.Integer("homePlatformsPerShipyard", 0);
  settings.shipyardQueueJobs = root.Integer("shipyardQueueJobs", 1);
  if (std::cmp_greater(settings.shipyardQueueJobs, Outpost::QUEUE_LIMIT))
    Neuron::JsonFail(root.PathOf("shipyardQueueJobs"), std::format("a queue holds at most {} jobs", Outpost::QUEUE_LIMIT));
  settings.defenseHoldSeconds = root.Number("defenseHoldSeconds", JsonBound::Positive);
  settings.structureGapMeters = root.Number("structureGapMeters", JsonBound::NotNegative);
  settings.rallyDistanceMeters = root.Number("rallyDistanceMeters", JsonBound::Positive);

  const std::string orderPath = root.PathOf("researchOrder");
  const Neuron::JsonValue::Array& order = Neuron::ReadJsonArray(root.Required("researchOrder"), orderPath);
  for (size_t i = 0; i < order.size(); ++i)
  {
    const std::string path = Neuron::JsonElementPath(orderPath, i);
    const Outpost::ResearchTopicId topic{static_cast<std::uint32_t>(Neuron::ReadJsonInteger(order[i], path, 1))};
    if (std::ranges::find(settings.researchOrder, topic) != settings.researchOrder.end())
      Neuron::JsonFail(path, std::format("topic {} is already in the order", topic.value));
    settings.researchOrder.push_back(topic);
  }

  settings.defaultDesign = ReadComponents(root, "defaultDesign");
  settings.counters = Neuron::ReadJsonList<Outpost::CounterRule>(root, "counters", ReadCounter);
  settings.scouts = root.Integer("scouts", 0);
  settings.scoutDesign = ReadComponents(root, "scoutDesign");
  settings.raidShips = root.Integer("raidShips", 0);
  settings.raidLossShare = root.Number("raidLossShare", JsonBound::NotNegative);
  if (settings.raidLossShare >= 1.0)
    Neuron::JsonFail(root.PathOf("raidLossShare"), "a share below 1, since a raid that has lost every ship has none to bring back");
  settings.raidIntervalSeconds = root.Number("raidIntervalSeconds", JsonBound::NotNegative);
  settings.attackNodeLead = root.Integer("attackNodeLead", 0);
  settings.attackWithoutLeadShare = root.Number("attackWithoutLeadShare", JsonBound::Positive);
  settings.frontPlatforms = root.Integer("frontPlatforms", 0);
  settings.claimSectors = root.Integer("claimSectors", 0);
  root.Finish();

  for (size_t i = 0; i < settings.counters.size(); ++i)
  {
    for (size_t j = 0; j < i; ++j)
    {
      if (settings.counters[i].enemy == settings.counters[j].enemy && settings.counters[i].answer == settings.counters[j].answer)
      {
        Neuron::JsonFail(Neuron::JsonElementPath("counters", i),
                         std::format("the same answer to the same design as {}", Neuron::JsonElementPath("counters", j)));
      }
    }
  }
  return settings;
}
} // namespace

Outpost::AiSettings Outpost::LoadAiSettings(std::string_view _json)
{
  try
  {
    return ReadAiSettings(_json);
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("Opponent: {}", error.what()));
  }
}

Outpost::AiSettings Outpost::LoadPackagedAiSettings()
{
  const Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(L"Opponent.json");
  if (bytes.empty())
    throw Neuron::Exception("The game data file Assets\\Opponent.json is missing or cannot be read.");
  return LoadAiSettings({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
}

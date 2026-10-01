#pragma once

namespace Outpost
{
// One model of a set, as OutpostCommander/Assets/Models.json describes it (ADR-011): which way its front points in its
// file, and how long it is in the game along that axis.
struct ModelEntry
{
  std::string name;
  Neuron::MeshAxis forward = Neuron::MeshAxis::PositiveX;
  float lengthMeters = 0.0f;
};

// A mesh set and the color it is drawn in: the player's Human set, the AI's Tarkan set, and the asteroid (design §1,
// §11). The color is linear and provisional until team colors are decided (design §15).
struct ModelSet
{
  std::string name;
  DirectX::XMFLOAT4 color{};
  std::vector<ModelEntry> models;

  // The model with this name. Throws Neuron::Exception when the set has none.
  [[nodiscard]] const ModelEntry& Model(std::string_view _name) const;
};

// Which set draws a player's ships (design §1): the player's Human set, the AI's Tarkan set.
struct PlayerModels
{
  PlayerId player;
  std::string set;
};

// Which model draws a hull, as the tuning data numbers hulls (task 2.5). The same model name is in every player's set.
struct HullModel
{
  HullId hull;
  std::string model;
};

// Which model draws a kind of structure (design §6), drawn to the structure's footprint across, and the share of its
// set's color it is drawn in, so that two kinds sharing a mesh read apart: the Shipyard is a darker Station.
struct StructureModel
{
  StructureKind structure = StructureKind::CommandStation;
  std::string model;
  float tint = 1.0f;
};

// Every model the game can draw, from OutpostCommander/Assets/Models.json.
struct ModelCatalog
{
  std::vector<ModelSet> sets;
  std::vector<PlayerModels> players;
  std::vector<HullModel> hulls;
  // One per kind of structure, and the model a Constructor is drawn with; the same names are in every player's set.
  std::vector<StructureModel> structures;
  std::string constructor;

  // The set with this name. Throws Neuron::Exception when there is none.
  [[nodiscard]] const ModelSet& Set(std::string_view _name) const;
  // The set a player's ships are drawn with, or nullptr for a player the data does not name.
  [[nodiscard]] const ModelSet* SetForPlayer(PlayerId _player) const noexcept;
  // The model a hull is drawn with, or nullptr for a hull the data does not name.
  [[nodiscard]] const std::string* ModelForHull(HullId _hull) const noexcept;
  // How a kind of structure is drawn, or nullptr for one the data does not name.
  [[nodiscard]] const StructureModel* ModelForStructure(StructureKind _structure) const noexcept;
};

// Reads the text of OutpostCommander/Assets/Models.json. Throws Neuron::Exception on the first problem, naming where it
// is, such as "sets[1].models[4].forwardAxis". Besides types and ranges it checks that set names are unique, and model
// names within a set, that each player, hull and kind of structure is listed once and every kind is, and that every
// hull's, structure's and the Constructor's model is in every player's set.
[[nodiscard]] ModelCatalog LoadModelCatalog(std::string_view _json);

// Where a model's converted mesh is under the package's Assets folder: Models\<set>\<model>.cmo (design §11).
[[nodiscard]] std::wstring ModelFileName(const ModelSet& _set, const ModelEntry& _model);

// A model's mesh in the game's frame: read from the bytes of its .cmo file, centered, turned to face +x and scaled to its
// length (ADR-011). Throws Neuron::Exception naming _fileName when the file cannot be read.
[[nodiscard]] Neuron::MeshData BuildModelMesh(std::span<const std::uint8_t> _cmoBytes, const ModelEntry& _model,
                                              std::string_view _fileName);
} // namespace Outpost

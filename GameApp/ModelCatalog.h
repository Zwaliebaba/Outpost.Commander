#pragma once

namespace Outpost
{
// One model of a set, as OutpostCommander/Assets/Models.json describes it (ADR-011): how long it is in the game along
// its front, which its mesh faces (ADR-018).
struct ModelEntry
{
  std::string name;
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

// How far a ship leans into its turns, and how quickly (ADR-029). It is presentation: the server knows nothing of it.
// The bank grows with the ship's sideways acceleration, its speed times how fast it turns, up to maxBankRadians at
// fullBankMetersPerSecondSquared, and follows that through a spring that settles in about settleSeconds. All zero, a
// ship flies level.
struct BankLimits
{
  float maxBankRadians = 0.0f;
  float fullBankMetersPerSecondSquared = 0.0f;
  float settleSeconds = 0.0f;
};

// Which model draws a hull, as the tuning data numbers hulls (task 2.5), and how ships of the hull bank. The same model
// name is in every player's set.
struct HullModel
{
  HullId hull;
  std::string model;
  BankLimits bank;
};

// Which model draws a kind of structure (design §6), drawn to the structure's footprint across, and the share of its
// set's color it is drawn in, so that two kinds sharing a mesh read apart: the Shipyard is a darker Station.
struct StructureModel
{
  StructureKind structure = StructureKind::CommandStation;
  std::string model;
  float tint = 1.0f;
};

// The color a drive's exhaust glows in (ADR-019), so that a ship's drive reads on sight. Linear, and provisional, like
// the team colors (design §15).
struct DriveExhaust
{
  DriveId drive;
  DirectX::XMFLOAT4 color{};
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
  // One per drive the data names, and the Constructor's, which has no drive (design §7).
  std::vector<DriveExhaust> exhausts;
  DirectX::XMFLOAT4 constructorExhaust{};
  // How the Constructor banks, which has no hull (design §7).
  BankLimits constructorBank;

  // The set with this name. Throws Neuron::Exception when there is none.
  [[nodiscard]] const ModelSet& Set(std::string_view _name) const;
  // The set a player's ships are drawn with, or nullptr for a player the data does not name.
  [[nodiscard]] const ModelSet* SetForPlayer(PlayerId _player) const noexcept;
  // The model a hull is drawn with, or nullptr for a hull the data does not name.
  [[nodiscard]] const std::string* ModelForHull(HullId _hull) const noexcept;
  // How a kind of structure is drawn, or nullptr for one the data does not name.
  [[nodiscard]] const StructureModel* ModelForStructure(StructureKind _structure) const noexcept;
  // The color a ship's exhaust glows in: its drive's, or the Constructor's; nullptr for a drive the data does not name,
  // or anything that is not a ship.
  [[nodiscard]] const DirectX::XMFLOAT4* ExhaustColor(const EntityView& _entity) const noexcept;
  // How a ship banks: its hull's, or the Constructor's; nullptr for a hull the data does not name, or anything that is
  // not a ship.
  [[nodiscard]] const BankLimits* BankFor(const EntityView& _entity) const noexcept;
};

// Reads the text of OutpostCommander/Assets/Models.json. Throws Neuron::Exception on the first problem, naming where it
// is, such as "sets[1].models[4].lengthMeters". Besides types and ranges it checks that set names are unique, and model
// names within a set, that each player, hull, drive and kind of structure is listed once and every kind is, and that
// every hull's, structure's and the Constructor's model is in every player's set. A hull's "bank" and the
// "constructorBank" are optional; without one, those ships fly level.
[[nodiscard]] ModelCatalog LoadModelCatalog(std::string_view _json);

// Where a model's baked mesh is under the package's Assets folder: Models\<set>\<model>.nmf (ADR-018).
[[nodiscard]] std::wstring ModelFileName(const ModelSet& _set, const ModelEntry& _model);

// A model's mesh at its size in the game: read from the bytes of its .nmf file, centered and scaled to its length, its
// hardpoints with it (ADR-018). Throws Neuron::Exception naming _fileName when the file cannot be read or has a
// hardpoint whose tag the game does not know.
[[nodiscard]] Neuron::MeshData BuildModelMesh(std::span<const std::uint8_t> _nmfBytes, const ModelEntry& _model,
                                              std::string_view _fileName);
} // namespace Outpost